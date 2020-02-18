#include "directory.h"
#include "bitVector.h"
#include "listDirEntries.h"
#include "fcb.h"

Directory::Directory(Partition* p)
{
	partition = p;
	index1 = new char[ClusterSize];
	numberOfFiles = 0;
	availableEntriesInDir = new ListOfAvailableEntries();
}

Directory::~Directory()
{
	partition = nullptr;
	delete[] index1;
	numberOfFiles = 0;
	cacheDir.clear(); //radi
	delete availableEntriesInDir;
}

void Directory::unmount()
{
	partition = nullptr;
	numberOfFiles = 0;
	cacheDir.clear(); //radi
	delete availableEntriesInDir;
}

char* Directory::getIndex1()
{
	return index1;
}

void Directory::format()
{
	for (int i = 0; i < ClusterSize; ++i) {
		index1[i] = 0x00;
	}
	numberOfFiles = 0;
	cacheDir.empty();
	delete availableEntriesInDir;
	availableEntriesInDir = new ListOfAvailableEntries();
}

char Directory::createNewFile(char* fileName, BitVector* bitVector)
{
	DirEntryPath* dirEntryPath;
	dirEntryPath = availableEntriesInDir->get();
	if (!dirEntryPath) {
		if (!expand(bitVector)) return 0; // nema mesta u dir-u
		dirEntryPath = availableEntriesInDir->get();
		if (!dirEntryPath) return 0; //greska
	}
	dirEntryPath->valueOfentryInDirData = 0;
	cacheDir[std::string(fileName)] = dirEntryPath;

	//azuriranje particije:
	char* entry = createEntry(fileName);
	if (!entry) return 0;
	char* buffer = new char[ClusterSize];
	if (!buffer) return 0;
	if (!partition->readCluster(dirEntryPath->valueOfentryInDirIndex2, buffer)) return 0;
	for (unsigned long i = 0; i < 20; ++i) {
		buffer[dirEntryPath->entryInDirData * 20 + i] = entry[i];
	}
	if (!partition->writeCluster(dirEntryPath->valueOfentryInDirIndex2, buffer)) return 0; //azuriranje ulaza dir-a na disku
	++numberOfFiles;
	delete[] buffer;

	return 1;
}

char Directory::deleteFileEntry(char* fname, BitVector* bitVector)
{
	int cnt = 0;
	ClusterNo indexEntryValue;
	unsigned long buffIterator;
	char* buffer = new char[ClusterSize];
	if (!buffer) return 0;
	DirEntryPath* dirEntryPath = cacheDir[fname];
	dirEntryPath->valueOfentryInDirData = 0;

	cacheDir.erase(std::string(fname)); // radi
	if (!partition->readCluster(dirEntryPath->valueOfentryInDirIndex2, buffer)) return 0;
	for (int i = 0; i < 20; ++i) buffer[dirEntryPath->entryInDirData * 20 + i] = 0x00;

	//provera da li je to bio posl zauzeti ulaz, ako jeste oslobodi sve potrebne klastere
	for (buffIterator = 0; buffIterator < ClusterSize / 20; ++buffIterator) {
		char* fn = new char[11];
		if (!fn) return 0;
		for (int j = 0; j < 8; ++j) fn[j] = buffer[buffIterator * 20 + 7 - j];
		for (int j = 0; j < 3; j++) fn[8 + j] = buffer[buffIterator * 20 + 10 - j];
		if (fn[0] == 0x00) ++cnt;
	}
	if (cnt == ClusterSize / 20) { //ceo klaster s ulazima je prazan
		bitVector->freeCluster(dirEntryPath->valueOfentryInDirIndex2, false);
		//izbaci iz liste
		if (!partition->readCluster(dirEntryPath->valueOfentryInDirIndex1, buffer));
		for (int i = 0; i < 4; ++i) buffer[dirEntryPath->entryInDirIndex2 * 4 + i] = 0x00;
		cnt = 0;
		for (buffIterator = 0; buffIterator < ClusterSize / 4; ++buffIterator) { // proverava da li je index2 postao prazan
			indexEntryValue = 0;
			indexEntryValue |= (buffer[buffIterator * 4 + 3] & 0x000000FF) << 3 * 8;
			indexEntryValue |= (buffer[buffIterator * 4 + 2] & 0x000000FF) << 2 * 8;
			indexEntryValue |= (buffer[buffIterator * 4 + 1] & 0x000000FF) << 1 * 8;
			indexEntryValue |= (buffer[buffIterator * 4] & 0x000000FF);
			if (indexEntryValue == 0) ++cnt;
			if (buffIterator < ClusterSize / 20) availableEntriesInDir->deleteAllElemFromOneCluster(dirEntryPath);
		}
		if (cnt == ClusterSize / 4) {
			bitVector->freeCluster(dirEntryPath->valueOfentryInDirIndex1, false);
			for (int i = 0; i < 4; ++i) index1[dirEntryPath->entryInDirIndex1 * 4 + i] = 0x00;
			if (!partition->writeCluster(1, index1)) return 0;
		}
		if (!bitVector->save()) return 0;
	}
	else {
		if (!partition->writeCluster(dirEntryPath->valueOfentryInDirIndex2, buffer)) return 0;
		availableEntriesInDir->put(dirEntryPath);
	}
	delete[] buffer;
	--numberOfFiles;
	return 0;
}

char* Directory::createEntry(char* fname)
{
	//name 8B
	char* entry = new char[20];
	if (!entry) return 0;
	int entryIterator = 0;
	int fnameIterator = 0;
	while (fname[fnameIterator] != '.') ++fnameIterator;
	--fnameIterator;
	while (fnameIterator >= 0) {
		if (entryIterator < 8) entry[entryIterator] = fname[fnameIterator];
		else break;
		++entryIterator;
		--fnameIterator;
	}
	while (entryIterator < 8) {
		entry[entryIterator] = ' ';
		++entryIterator;
	}
	//extension 3B
	while (fname[fnameIterator] != '\0') ++fnameIterator;
	--fnameIterator;
	while (fname[fnameIterator] != '.') {
		if (entryIterator < 11) entry[entryIterator] = fname[fnameIterator];
		else break;
		++entryIterator;
		--fnameIterator;
	}
	while (entryIterator < 11) {
		entry[entryIterator] = ' ';
		++entryIterator;
	}

	//unused 1B
	entry[11] = 0x00; // '0'; koje od ova dva?

	//index1 cluster number 4B
	entry[11 + 1] = 0x00; // (clusterNo & 0xFF000000) >> 3;
	entry[11 + 2] = 0x00; // (clusterNo & 0x00FF0000) >> 2;
	entry[11 + 3] = 0x00; // (clusterNo & 0x0000FF00) >> 1;
	entry[11 + 4] = 0x00; // (clusterNo & 0x000000FF);

	//file size 4B - size do not include indexe1 and index2 and data
	unsigned long s = 0;
	entry[15 + 1] = 0x00;
	entry[15 + 2] = 0x00;
	entry[15 + 3] = 0x00;
	entry[15 + 4] = 0x00;

	//unused 12 B ??? //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	return entry;
}

char Directory::expand(BitVector* bitVector)
{
	unsigned short index1iterator;
	ClusterNo index1entryValue = 0;
	for (index1iterator = 0; index1iterator < ClusterSize / 4; ++index1iterator) {
		index1entryValue = 0;
		index1entryValue |= (index1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
		index1entryValue |= (index1[index1iterator * 4] & 0x000000FF);
		if (index1entryValue == 0) break;
	}
	bool dirtyIndex1 = false;
	char* buffIndex2 = new char[ClusterSize];
	if (!buffIndex2) return 0;
	for (int i = 0; i < ClusterSize; ++i) {
		buffIndex2[i] = 0x00;
	}
	char* buffDataEntries = new char[ClusterSize];
	if (!buffDataEntries) return 0;
	for (int i = 0; i < ClusterSize; ++i) {
		buffDataEntries[i] = 0x00;
	}
	ClusterNo newIndex2ClusterID;
	ClusterNo newDataClusterID;

	if (index1iterator == 0) {
		newIndex2ClusterID = bitVector->getNextAvailableCluster();
		if (newIndex2ClusterID < 0) return 0;
		index1[3] = (newIndex2ClusterID & 0xFF000000) >> 3 * 8;
		index1[2] = (newIndex2ClusterID & 0x00FF0000) >> 2 * 8;
		index1[1] = (newIndex2ClusterID & 0x0000FF00) >> 1 * 8;
		index1[0] = (newIndex2ClusterID & 0x000000FF);

		newDataClusterID = bitVector->getNextAvailableCluster();
		if (newDataClusterID < 0) return 0;
		buffIndex2[3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
		buffIndex2[2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
		buffIndex2[1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
		buffIndex2[0] = (newDataClusterID & 0x000000FF);

		for (int i = 0; i < ClusterSize / 20; i++) {
			DirEntryPath* newFilePath = new DirEntryPath();
			if (!newFilePath) return 0;
			newFilePath->entryInDirIndex1 = 0;
			newFilePath->valueOfentryInDirIndex1 = newIndex2ClusterID;
			newFilePath->entryInDirIndex2 = 0;
			newFilePath->valueOfentryInDirIndex2 = newDataClusterID;
			newFilePath->entryInDirData = i;
			newFilePath->valueOfentryInDirData = 0;
			availableEntriesInDir->putOnEnd(newFilePath);
		}
		dirtyIndex1 = true;
	}
	else {
		if (index1iterator == ClusterSize / 4) return 0; //**greska nema mesta vise;
		--index1iterator;
		index1entryValue = 0;
		index1entryValue |= (index1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
		index1entryValue |= (index1[index1iterator * 4] & 0x000000FF);

		if (!partition->readCluster(index1entryValue, buffIndex2)) return 0;
		unsigned short index2iterator;
		ClusterNo index2entryValue = 0;
		for (index2iterator = 0; index2iterator < ClusterSize / 4; ++index2iterator) {
			index2entryValue = 0;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
			index2entryValue |= (buffIndex2[index2iterator * 4] & 0x000000FF);
			if (index2entryValue == 0) break;
		}
		if (index2iterator != ClusterSize / 4) {
			newDataClusterID = bitVector->getNextAvailableCluster();
			if (newDataClusterID < 0) return 0;
			buffIndex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
			buffIndex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
			buffIndex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
			buffIndex2[index2iterator * 4 + 0] = (newDataClusterID & 0x000000FF);
			for (int i = 0; i < ClusterSize / 20; i++) {
				DirEntryPath* newFilePath = new DirEntryPath();
				if (newFilePath == 0) return 0;
				newFilePath->entryInDirIndex1 = index1iterator;
				newFilePath->valueOfentryInDirIndex1 = index1entryValue;
				newFilePath->entryInDirIndex2 = index2iterator;
				newFilePath->valueOfentryInDirIndex2 = newDataClusterID;
				newFilePath->entryInDirData = i;
				newFilePath->valueOfentryInDirIndex1 = 0;
				availableEntriesInDir->putOnEnd(newFilePath);
			}
		}
		else {
			++index1iterator;
			if (index1iterator == ClusterSize / 4) return 0; //**greska nema mesta vise;

			newIndex2ClusterID = bitVector->getNextAvailableCluster();
			if (newIndex2ClusterID < 0) return 0;
			index1[index1iterator * 4 + 3] = (newIndex2ClusterID & 0xFF000000) >> 3 * 8;
			index1[index1iterator * 4 + 2] = (newIndex2ClusterID & 0x00FF0000) >> 2 * 8;
			index1[index1iterator * 4 + 1] = (newIndex2ClusterID & 0x0000FF00) >> 1 * 8;
			index1[index1iterator * 4 + 0] = (newIndex2ClusterID & 0x000000FF);

			newDataClusterID = bitVector->getNextAvailableCluster();
			if (newDataClusterID < 0) return 0;
			buffIndex2[3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
			buffIndex2[2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
			buffIndex2[1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
			buffIndex2[0] = (newDataClusterID & 0x000000FF);

			for (int i = 0; i < ClusterSize / 20; i++) {
				DirEntryPath* newFilePath = new DirEntryPath();
				if (!newFilePath) return 0;
				newFilePath->entryInDirIndex1 = index1iterator;
				newFilePath->valueOfentryInDirIndex1 = newIndex2ClusterID;
				newFilePath->entryInDirIndex2 = 0;
				newFilePath->valueOfentryInDirIndex2 = newDataClusterID;
				newFilePath->entryInDirData = i;
				newFilePath->valueOfentryInDirData = 0;
				availableEntriesInDir->putOnEnd(newFilePath);
			}
			dirtyIndex1 = true;
		}
	}

	//upis u particiju
	if (dirtyIndex1) {
		if (!partition->writeCluster(1, index1)) return 0; //ne mora uvek
	}
	if (!partition->writeCluster(newIndex2ClusterID, buffIndex2)) return 0;
	if (!partition->writeCluster(newDataClusterID, buffDataEntries)) return 0;
	delete[] buffIndex2;
	delete[] buffDataEntries;
	return 1;
}

char Directory::initialize()
{
	//prolazi kroz sve ulaze dir-a i inicijalizuje cacheDir, listu slobodnih ulaza dir-a i broj fajlova 
	unsigned long index1iterator;
	ClusterNo index1entryValue = 0;
	unsigned long index2iterator;
	ClusterNo index2entryValue = 0;
	char* buffIndex2 = new char[ClusterSize];
	if (!buffIndex2) return 0;
	unsigned long dataEntriesIterator;
	char* buffDataEntries = new char[ClusterSize];
	if (!buffDataEntries) return 0;
	for (index1iterator = 0; index1iterator < ClusterSize / 4; ++index1iterator) { //prolazak kroz indeks1
		index1entryValue = 0;
		index1entryValue |= (index1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
		index1entryValue |= (index1[index1iterator * 4] & 0x000000FF);
		if (index1entryValue == 0) break;

		if (!partition->readCluster(index1entryValue, buffIndex2)) return 0;
		for (index2iterator = 0; index2iterator < ClusterSize / 4; ++index2iterator) { // prolazak kroz index2
			index2entryValue = 0;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 3] & 0x000000FF) << 3;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 2] & 0x000000FF) << 2;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 1] & 0x000000FF) << 1;
			index2entryValue |= (buffIndex2[index2iterator * 4] & 0x000000FF);
			if (index2entryValue == 0) continue;

			if (!partition->readCluster(index2entryValue, buffDataEntries)) return 0;
			for (dataEntriesIterator = 0; dataEntriesIterator < ClusterSize / 20; ++dataEntriesIterator) { // prolazak kroz klaster s ulazima dir-a
				//fname
				char* fname = new char[13];
				if (!fname) return 0;
				int nameiterator = 0;
				int entryiterator = 7;
				while (buffDataEntries[dataEntriesIterator * 20 + entryiterator] == ' ') --entryiterator;
				while (entryiterator >= 0) {
					fname[nameiterator] = buffDataEntries[dataEntriesIterator* 20 + entryiterator];
					++nameiterator;
					--entryiterator;
				}
				fname[nameiterator] = '.';
				++nameiterator;
				entryiterator = 10;
				while (buffDataEntries[dataEntriesIterator * 20 + entryiterator] == ' ' && entryiterator > 7) --entryiterator;
				while (entryiterator > 7) {
					fname[nameiterator] = buffDataEntries[dataEntriesIterator * 20 + entryiterator];
					++nameiterator;
					--entryiterator;
				}
				fname[nameiterator] = '\0';
				//
				DirEntryPath* entryPath = new DirEntryPath();
				if (!entryPath) return 0;
				entryPath->entryInDirIndex1 = index1iterator;
				entryPath->valueOfentryInDirIndex1 = index1entryValue;
				entryPath->entryInDirIndex2 = index2iterator;
				entryPath->valueOfentryInDirIndex2 = index2entryValue;
				entryPath->entryInDirData = dataEntriesIterator;
				entryPath->valueOfentryInDirData = 0;
				if (fname[0] == 0x00) { // ulaz je slobodan i treba ga ubaciti u listu slobodnih ulaza
					availableEntriesInDir->putOnEnd(entryPath);
				}
				else { // ulaz je zauzet i treba ga ubaciti u cacheDir u i povecati br fajlova
					ClusterNo clusterNo = 0;
					clusterNo |= (buffDataEntries[dataEntriesIterator * 20 + 15] & 0x000000FF) << 3;
					clusterNo |= (buffDataEntries[dataEntriesIterator * 20 + 14] & 0x000000FF) << 2;
					clusterNo |= (buffDataEntries[dataEntriesIterator * 20 + 13] & 0x000000FF) << 1;
					clusterNo |= (buffDataEntries[dataEntriesIterator * 20 + 12] & 0x000000FF);
					entryPath->valueOfentryInDirData = clusterNo;
					cacheDir[std::string(fname)] = entryPath;
					++numberOfFiles;
				}
				delete[] fname;
			}
		}
	}
	delete[] buffIndex2;
	delete[] buffDataEntries;
	//kraj
	return 1;
}

FileCnt Directory::getNumberOfFiles()
{
	return numberOfFiles;
}

char Directory::doesExist(char* fname)
{
	if (cacheDir.find(std::string(fname)) != cacheDir.end()) // da li radi ?
		return 1;
	else return 0;
}

ClusterNo Directory::getFileIndex1ID(char* fname)
{
	return cacheDir[std::string(fname)]->valueOfentryInDirData;
}

char* Directory::getEntry(char* fname)
{
	DirEntryPath* path = cacheDir[std::string(fname)];
	if (!path) return 0;
	char* buff = new char[ClusterSize];
	if (!partition->readCluster(path->valueOfentryInDirIndex2, buff)) return 0;
	char* entry = new char[20];
	if (!entry) return 0;
	for (int i = 0; i < 20; ++i) {
		entry[i] = buff[path->entryInDirData * 20 + i];

	}
	delete[]buff;
	return entry;
}

char Directory::updateEntry(FCB* fcb )
{
	DirEntryPath* dirEntryPath = cacheDir[fcb->getName()]; //
	char* buff = new char[ClusterSize];
	if (!partition->readCluster(dirEntryPath->valueOfentryInDirIndex2, buff)) return 0;
	
	//size
	BytesCnt newSize = fcb->getFileSize();
	char* size = new char[4];
	size[0] = (newSize & 0x000000FF);
	size[1] = (newSize & 0x0000FF00) >> 1 * 8;
	size[2] = (newSize & 0x00FF0000) >> 2 * 8;
	size[3] = (newSize & 0xFF000000) >> 3 * 8;
	for (int i = 0; i < 4; ++i) buff[dirEntryPath->entryInDirData * 20 + 16 + i] = size[i];

	//index1clusterID
	ClusterNo index1id = fcb->getIndex1ClusterID();
	char* id = new char[4];
	id[0] = (index1id & 0x000000FF);
	id[1] = (index1id & 0x0000FF00) >> 1 * 8;
	id[2] = (index1id & 0x00FF0000) >> 2 * 8;
	id[3] = (index1id & 0xFF000000) >> 3 * 8;
	for (int i = 0; i < 4; ++i) buff[dirEntryPath->entryInDirData * 20 + 12 + i] = id[i];

	if (!partition->writeCluster(dirEntryPath->valueOfentryInDirIndex2, buff)) return 0;

	return 1;
}

