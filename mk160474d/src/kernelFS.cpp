#include "kernelFS.h"
#include "bitVector.h"
#include "file.h"
#include "kernelFile.h"
#include "part.h"
#include "listOfopenedFiles.h"
#include "directory.h"
#include "cache.h"

KernelFS::KernelFS()
{
	partition = nullptr;
	bitVector = nullptr;
	directory = nullptr;
	globOpenFilesTable = new ListOfOpenedFiles();
	isFormatted = false;
	isInitialized = false;
	
	CACHE_MOD = true;
	if(CACHE_MOD) cache = new Cache();

	openable = true;
	semMount = CreateSemaphore(NULL, 0, MAX_THREAD_CNT, NULL); // (security, initial count, maximum count, named semaphore)
	semMountBlockedCnt = 0;
	semUnmount = CreateSemaphore(NULL, 0, MAX_THREAD_CNT, NULL);
	semUnmountBlockedCnt = 0;
	semFormat = CreateSemaphore(NULL, 0, MAX_THREAD_CNT, NULL);
	semFormatBlockedCnt = 0;
	mutexFS = CreateSemaphore(NULL, 1, 1, NULL);
}

KernelFS::~KernelFS()
{
	if (partition) unmount();
	partition = nullptr;
	delete bitVector;
	delete directory;
	CloseHandle(semMount);
	CloseHandle(semFormat);
	CloseHandle(mutexFS);
}

char KernelFS::mount(Partition* partition)
{
	wait(mutexFS);
	if (!partition) { 
		signal(mutexFS);
		return 0;
	}
	while (this->partition) {
		++semMountBlockedCnt;
		//cout << "\n*** BLOKIRANA NIT NA MONTIRANJU ***\n";
		signal(mutexFS);
		wait(semMount);
		wait(mutexFS);
	}
	this->partition = partition;
	if (CACHE_MOD) cache->setPartition(partition);
	bitVector = new BitVector(partition);
	directory = new Directory(partition);
	if (!bitVector || !directory) {
		signal(mutexFS);
		return 0;
	}
	if (!partition->readCluster(0, bitVector->getVector())) {
		signal(mutexFS);
		return 0;
	}
	if (!partition->readCluster(1, directory->getIndex1())) {
		signal(mutexFS);
		return 0;
	}
	signal(mutexFS);
	return 1;
}

char KernelFS::unmount()
{
	wait(mutexFS);
	if (!partition) {
		signal(mutexFS);
		return 0;
	}
	while (globOpenFilesTable->getCnt() != 0) {
		//openable = false; // da li treba?
		++semUnmountBlockedCnt;
		signal(mutexFS);
		wait(semUnmount);
		wait(mutexFS);
		if (!partition) return 0; // ovo se nikad ne desava?
	}
	partition = nullptr;
	if (CACHE_MOD) delete cache;
	delete bitVector;
	delete directory;
	isFormatted = false;
	isInitialized = false;
	signalCnt(semMount, semMountBlockedCnt);
	semMountBlockedCnt = 0;
	signal(mutexFS);
	return 1;
}

char KernelFS::format()
{
	wait(mutexFS);
	if (!partition) {
		signal(mutexFS);
		return 0;
	}
	while (globOpenFilesTable->getCnt() != 0) {
		openable = false;
		++semFormatBlockedCnt;
		signal(mutexFS);
		wait(semFormat);
		wait(mutexFS);
	}
	isFormatted = true;
	isInitialized = false; // da li ovo treba?
	if (CACHE_MOD) cache->format();
	bitVector->format();
	directory->format();
	if (!partition->writeCluster(0, bitVector->getVector())) {
		signal(mutexFS);
		return 0;
	}
	if (!partition->writeCluster(1, directory->getIndex1())) {
		signal(mutexFS);
		return 0;
	}
	signal(mutexFS);
	return 1;
}

FileCnt KernelFS::readRootDir()
{
	wait(mutexFS);
	if (!partition) {
		signal(mutexFS);
		return -1; // nije namontirana ni jedna particija
	}
	if (!isFormatted && !isInitialized) {// namontirana je particija s postojecim fajl sistemom (ne prazna)
		if (!initialize()) {
			signal(mutexFS);
			return -1;
		}
	}
	FileCnt ret = directory->getNumberOfFiles();;
	signal(mutexFS);
	return  ret;
}

char KernelFS::doesExist(char* fname)
{
	wait(mutexFS);
	if (!partition) {
		signal(mutexFS);
		return 0; // nije namontirana ni jedna particija
	}
	if (!isFormatted && !isInitialized) { // namontirana je particija s postojecim fajl sistemom (ne prazna)
		if (!initialize()) {
			signal(mutexFS);
			return 0;
		}
	}
	char ret = directory->doesExist(fname);
	signal(mutexFS);
	return ret;
}

File* KernelFS::open(char* fname, char mode)
{
	wait(mutexFS);
	if (!partition) {
		signal(mutexFS);
		return 0;
	}
	if (!isFormatted && !isInitialized) { // namontirana je particija s postojecim fajl sistemom (ne prazna)
		if (!initialize()) {
			cout << endl << "*** GRESKA POKUSAJ OTVARANJA FAJLA: " << std::string(fname) << " NAKON POZIVA FORMATIRANJA ***" << endl;
			exit(-1); //greska
			//signal(mutexFS);
			//return 0; 
		}
	}
	if (!openable) {
		signal(mutexFS);
		return nullptr;
	}
	FCB* fcb = globOpenFilesTable->getfcb(fname);
	if (fcb) {
		signal(mutexFS);
		wait(fcb->getSem());
		wait(mutexFS);
	}
	if (mode == 'w') {
		if (directory->doesExist(fname)) {
			if (!deleteFile(fname)) {
				signal(mutexFS);
				return 0;
			}
		}
		if (!directory->createNewFile(fname, bitVector)) {
			signal(mutexFS);
			return 0;
		}
	}
	else {
		if (!directory->doesExist(fname) || (mode != 'r' && mode != 'a')) {
			//cout << endl << "*** GRESKA POKUSAJ OTVARANJA NEPOSTOJECEG FAJLA: "<< std::string(fname)<<" U MODU 'a' ili 'r' ***" << endl;
			//exit(-2); //greska
			signal(mutexFS); 
			return 0; // ? sta treba ovde ?
		}
	}
	fcb = new FCB(directory->getEntry(fname), mode);
	globOpenFilesTable->put(fcb);
	File* f = new File();
	if (!f) {
		signal(mutexFS);
		return 0;
	}
	f->myImpl->setKernelFS(this); // mora pre setfcb
	if (!f->myImpl->setFCB(fcb)) {
		signal(mutexFS);
		return 0;
	}
	if (mode == 'a') f->seek(f->getFileSize());
	signal(mutexFS);
	return f;
}

char KernelFS::deleteFile(char* fname)
{
	wait(mutexFS);
	if (!partition) {
		signal(mutexFS);
		return 0;
	}
	if (!isFormatted && !isInitialized) { // namontirana je particija s postojecim fajl sistemom (ne prazna)
		if (!initialize()) {
			signal(mutexFS);
			return 0;
		}
	}
	//ako je otvoren

	//ako fajl ne postoji
	if (!directory->doesExist(fname)) {
		signal(mutexFS);
		return 0;
	}

	//obrisi sve klastere koje fajl zauzima
	ClusterNo findex1ID = directory->getFileIndex1ID(fname);
	char* index1 = new char[ClusterSize];
	if (!index1) return 0;
	if (!partition->readCluster(findex1ID, index1)) {
		signal(mutexFS);
		return 0;
	}
	unsigned long index1iterator;
	ClusterNo index1entryValue = 0;
	unsigned long index2iterator;
	ClusterNo index2entryValue = 0;
	char* buffIndex2 = new char[ClusterSize];
	if (!buffIndex2) {
		signal(mutexFS);
		return 0;
	}
	for (index1iterator = 0; index1iterator < ClusterSize / 4; ++index1iterator) { //prolazak kroz indeks1
		index1entryValue = 0;
		index1entryValue |= (index1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
		index1entryValue |= (index1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
		index1entryValue |= (index1[index1iterator * 4] & 0x000000FF);
		if (index1entryValue == 0) break;

		if (!partition->readCluster(index1entryValue, buffIndex2)) {
			signal(mutexFS);
			return 0;
		}
		for (index2iterator = 0; index2iterator < ClusterSize / 4; ++index2iterator) { // prolazak kroz index2
			index2entryValue = 0;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
			index2entryValue |= (buffIndex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
			index2entryValue |= (buffIndex2[index2iterator * 4] & 0x000000FF);
			if (index2entryValue == 0) continue; // mozda nije potrebno
			bitVector->freeCluster(index2entryValue, false);
			if (CACHE_MOD) cache->deleteEntry(index2entryValue);
		}
		bitVector->freeCluster(index1entryValue, false);
	}
	bitVector->freeCluster(findex1ID, false);
	if (!bitVector->save()) {
		signal(mutexFS);
		return 0;
	}

	//obrisi ulaz u dir-u
	if (directory->deleteFileEntry(fname, bitVector)) {
		signal(mutexFS);
		return 0;
	}
	delete[]index1;
	delete[] buffIndex2;
	signal(mutexFS);
	return 1;
}

char KernelFS::initialize()
{
	if (!partition) {
		return 0;
	}
	// ovo se radi ako je namontirana particija s vec postojecim fajl sistemom
	isInitialized = true;
	if (!directory->initialize()) {
		return 0;
	}
	return 1;
}

BitVector* KernelFS::getBitVector()
{
	return bitVector;
}

Directory* KernelFS::getDirectory()
{
	return directory;
}


