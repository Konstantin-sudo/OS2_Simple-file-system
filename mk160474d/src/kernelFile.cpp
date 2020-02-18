#include "kernelFile.h"
#include "fcb.h"
#include "kernelFS.h"
#include "bitVector.h"
#include "listOfopenedFiles.h"
#include "file.h"
#include "kernelFS.h"
#include "directory.h"
#include "cache.h"


KernelFile::KernelFile(File* f)
{
	kernelFS = nullptr;
	myFile = f;
	myFCB = nullptr;
	mySeek = 0;
	currClusterID = 0;
	myIndex1 = new char[ClusterSize];
}

KernelFile::~KernelFile()
{
	myFile = nullptr;
	delete[] myIndex1;

	kernelFS->globOpenFilesTable->deleteElem(myFCB->getName());
	kernelFS->getDirectory()->updateEntry(myFCB);
	signal(myFCB->getSem()); // trebalo bi da oslobodi sve niti a ne jednu ?
	delete myFCB;
	if (kernelFS->globOpenFilesTable->getCnt() == 0 && kernelFS->openable == false) {
		int cnt = kernelFS->semFormatBlockedCnt;
		kernelFS->semFormatBlockedCnt = 0;
		signal(kernelFS->semFormat,cnt);
	}
	else {
		if (kernelFS->globOpenFilesTable->getCnt() == 0) {
			int cnt = kernelFS->semUnmountBlockedCnt;
			kernelFS->semUnmountBlockedCnt = 0;
			signal(kernelFS->semUnmount, cnt);
		}
	}
	kernelFS = nullptr;
}

char KernelFile::write(BytesCnt bytesCnt, char* buffer)
{
	if (kernelFS->CACHE_MOD) {
		if (myFCB->getMode() == 'r' || !buffer) {
			return 0;
		}
		if ((myFCB->getFileSize() - mySeek) < bytesCnt) { // da li ce morati da se prosiruje
			if ((MAX_FILE_SIZE - mySeek) < bytesCnt) {
				return 0; //ne moze da prosiri fajl - nema dovoljno mesta
			}
		}//ima mesta, odradi upis
		bool dirtyIndex1 = false;
		bool dirtyIndex2 = false;
		ClusterNo newDataClusterID;
		ClusterNo newindex2clusterID;
		ClusterNo index1iterator = mySeek / ((ClusterSize / 4) * ClusterSize); // mySeek / 1048575(broj bajtova na koje pokazuje 1 ulaz u indeksu 1) - od 0 do 511
		ClusterNo index2iterator = (mySeek - (ClusterSize * ClusterSize / 4) * index1iterator) / ClusterSize; //od 0 do 511  
		ClusterNo dataiterator = mySeek - (index1iterator * (ClusterSize / 4) * ClusterSize + index2iterator * ClusterSize); //od 0 do 2047
		ClusterNo currIndex1entryValue = 0;
		ClusterNo currIndex2entryValue = 0;
		char* buffindex2 = new char[ClusterSize];
		char* buffdata; // = new char[ClusterSize];
		if (!buffindex2) {
			return 0;
		}
		BytesCnt fsize = myFCB->getFileSize();

		currIndex1entryValue |= (myIndex1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
		currIndex1entryValue |= (myIndex1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
		currIndex1entryValue |= (myIndex1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
		currIndex1entryValue |= (myIndex1[index1iterator * 4] & 0x000000FF);
		if (mySeek % ClusterSize == 0) { // fajl je prazan ili mozda mora da se prosiri pre upisa ili je seek bio rucno pomeran na pocetak nekog data klastera
			if (!currIndex1entryValue) {
				dirtyIndex1 = true;
				dirtyIndex2 = true;
				newindex2clusterID = kernelFS->getBitVector()->getNextAvailableCluster();
				if (newindex2clusterID < 0) {
					return 0;
				}
				myIndex1[index1iterator * 4 + 3] = (newindex2clusterID & 0xFF000000) >> 3 * 8;
				myIndex1[index1iterator * 4 + 2] = (newindex2clusterID & 0x00FF0000) >> 2 * 8;
				myIndex1[index1iterator * 4 + 1] = (newindex2clusterID & 0x0000FF00) >> 1 * 8;
				myIndex1[index1iterator * 4] = (newindex2clusterID & 0x000000FF);
				currIndex1entryValue = newindex2clusterID;
				newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
				if (newDataClusterID < 0) {
					return 0;
				}
				for (int i = 0; i < ClusterSize; ++i) buffindex2[i] = 0x00;
				buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
				buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
				buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
				buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
				currIndex2entryValue = newDataClusterID;
				//
				currClusterID = newDataClusterID;
			}
			else {
				if (!kernelFS->partition->readCluster(currIndex1entryValue, buffindex2)) {
					return 0;
				}
				currIndex2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
				currIndex2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
				currIndex2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
				currIndex2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
				if (!currIndex2entryValue) {
					dirtyIndex2 = true;
					newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
					if (newDataClusterID < 0) {
						return 0;
					}
					buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
					buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
					buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
					buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
					currIndex2entryValue = newDataClusterID;
					//
					currClusterID = newDataClusterID;
				}
				else {
					currClusterID = currIndex2entryValue; //ovde dodje samo kada je seek bio rucno pomeran
					//if (!kernelFS->partition->readCluster(currIndex2entryValue, buffdata)) { 
					//	return 0;
					//}
				}
			}
		}
		else {
			if (currClusterID == 0) { // seek je rucno pomeran
				if (!kernelFS->partition->readCluster(currIndex1entryValue, buffindex2)) {
					return 0;
				}
				currIndex2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
				currIndex2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
				currIndex2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
				currIndex2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
				//
				currClusterID = currIndex2entryValue;
				//if (!kernelFS->partition->readCluster(currIndex2entryValue, buffdata)) {
				//	return 0;
				//}
			}
		}
		//
		buffdata = kernelFS->cache->get(currClusterID);
		if (!buffdata) {
			buffdata = new char[ClusterSize];
			//if (!kernelFS->partition->readCluster(currIndex2entryValue, buffdata)) {
				//return 0;
			//}
			kernelFS->cache->put(currClusterID, true, buffdata);
		}
		else {
			kernelFS->cache->setDirty(currClusterID, true);
		}
		//provarei da li se data cluster nalazi u kesu 
		for (BytesCnt i = 0; i < bytesCnt; ++i) {
			if (dataiterator == ClusterSize) { //mora da se prosiri fajl;
				//if (!kernelFS->partition->writeCluster(currIndex2entryValue, buffdata)) {
				//	return 0;
				//}
				dataiterator = 0;
				if (++index2iterator == ClusterSize / 4) { // da li je i index2 popunjen
					if (!kernelFS->partition->writeCluster(currIndex1entryValue, buffindex2)) {
						return 0;
					}
					dirtyIndex1 = true;
					dirtyIndex2 = false; //??
					index2iterator = 0;
					if (++index1iterator == ClusterSize / 4) {
						return 0; // nema mesta (ne bi trablo da dodje do ovoga al neka ga - gore proverava ovaj uslov)
					}
					newindex2clusterID = kernelFS->getBitVector()->getNextAvailableCluster();
					if (newindex2clusterID < 0) {
						return 0;
					}
					myIndex1[index1iterator * 4 + 3] = (newindex2clusterID & 0xFF000000) >> 3 * 8;
					myIndex1[index1iterator * 4 + 2] = (newindex2clusterID & 0x00FF0000) >> 2 * 8;
					myIndex1[index1iterator * 4 + 1] = (newindex2clusterID & 0x0000FF00) >> 1 * 8;
					myIndex1[index1iterator * 4] = (newindex2clusterID & 0x000000FF);
					newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
					if (newDataClusterID < 0) {
						return 0;
					}
					for (int i = 0; i < ClusterSize; ++i) buffindex2[i] = 0x00;
					buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
					buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
					buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
					buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
					currIndex1entryValue = newindex2clusterID;
					currIndex2entryValue = newDataClusterID;
					//
					currClusterID = newDataClusterID;
					buffdata = kernelFS->cache->get(currClusterID);
					if (!buffdata) {
						buffdata = new char[ClusterSize];
						//if (!kernelFS->partition->readCluster(currIndex2entryValue, buffdata)) {
						//	return 0;
						//}
						kernelFS->cache->put(currClusterID, true, buffdata);
					}
					else {
						kernelFS->cache->setDirty(currClusterID, true);
					}

				}
				else {
					dirtyIndex2 = true;
					newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
					if (newDataClusterID < 0) {
						return 0;
					}
					buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
					buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
					buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
					buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
					currIndex2entryValue = newDataClusterID;
					//
					currClusterID = newDataClusterID;
					buffdata = kernelFS->cache->get(currClusterID);
					if (!buffdata) {
						buffdata = new char[ClusterSize];
						//if (!kernelFS->partition->readCluster(currIndex2entryValue, buffdata)) {
						//	return 0;
						//}
						kernelFS->cache->put(currClusterID, true, buffdata);
					}
					else {
						kernelFS->cache->setDirty(currClusterID, true);
					}
				}
			}
			buffdata[dataiterator] = buffer[i];
			++dataiterator;
			++fsize;
			++mySeek;
			if (mySeek % ClusterSize == 0) currClusterID = 0;
		}
		if (dirtyIndex1) {
			if (!kernelFS->partition->writeCluster(myFCB->getIndex1ClusterID(), myIndex1)) {
				return 0;
			}
		}
		if (dirtyIndex2) {
			if (!kernelFS->partition->writeCluster(currIndex1entryValue, buffindex2)) {
				return 0;
			}
		}
		//if (!kernelFS->partition->writeCluster(currIndex2entryValue, buffdata)) {
		//	return 0;
		//}
		myFCB->setSize(fsize);
		buffdata = nullptr;
		delete buffdata;
		delete[]buffindex2;
		return 0;
	}
	else {
	if (myFCB->getMode() == 'r' || !buffer) {
		return 0;
	}
	if ((myFCB->getFileSize() - mySeek) < bytesCnt) { // da li ce morati da se prosiruje
		if ((MAX_FILE_SIZE - mySeek) < bytesCnt) {
			return 0; //ne moze da prosiri fajl - nema dovoljno mesta
		}
	}//ima mesta, odradi upis
	bool dirtyIndex1 = false;
	bool dirtyIndex2 = false;
	ClusterNo newDataClusterID;
	ClusterNo newindex2clusterID;
	ClusterNo index1iterator = mySeek / ((ClusterSize / 4) * ClusterSize); // mySeek / 1048575(broj bajtova na koje pokazuje 1 ulaz u indeksu 1) - od 0 do 511
	ClusterNo index2iterator = (mySeek - (ClusterSize * ClusterSize / 4) * index1iterator) / ClusterSize; //od 0 do 511  
	ClusterNo dataiterator = mySeek - (index1iterator * (ClusterSize / 4) * ClusterSize + index2iterator * ClusterSize); //od 0 do 2047
	ClusterNo currIndex1entryValue = 0;
	ClusterNo currIndex2entryValue = 0;
	char* buffindex2 = new char[ClusterSize];
	char* buffdata = new char[ClusterSize];
	if (!buffdata || !buffindex2) {
		return 0;
	}
	BytesCnt fsize = myFCB->getFileSize();
	//
	currIndex1entryValue |= (myIndex1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
	currIndex1entryValue |= (myIndex1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
	currIndex1entryValue |= (myIndex1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
	currIndex1entryValue |= (myIndex1[index1iterator * 4] & 0x000000FF);
	if (mySeek % ClusterSize == 0) { // fajl je prazan ili mozda mora da se prosiri pre upisa
		if (!currIndex1entryValue) {
			dirtyIndex1 = true;
			dirtyIndex2 = true;
			newindex2clusterID = kernelFS->getBitVector()->getNextAvailableCluster();
			if (newindex2clusterID < 0) {
				return 0;
			}
			myIndex1[index1iterator * 4 + 3] = (newindex2clusterID & 0xFF000000) >> 3 * 8;
			myIndex1[index1iterator * 4 + 2] = (newindex2clusterID & 0x00FF0000) >> 2 * 8;
			myIndex1[index1iterator * 4 + 1] = (newindex2clusterID & 0x0000FF00) >> 1 * 8;
			myIndex1[index1iterator * 4] = (newindex2clusterID & 0x000000FF);
			currIndex1entryValue = newindex2clusterID;
			newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
			if (newDataClusterID < 0) {
				return 0;
			}
			for (int i = 0; i < ClusterSize; ++i) buffindex2[i] = 0x00;
			buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
			buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
			buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
			buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
			currIndex2entryValue = newDataClusterID;
		}
		else {
			if (!kernelFS->partition->readCluster(currIndex1entryValue, buffindex2)) {
				return 0;
			}
			currIndex2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
			currIndex2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
			currIndex2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
			currIndex2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
			if (!currIndex2entryValue) {
				dirtyIndex2 = true;
				newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
				if (newDataClusterID < 0) {
					return 0;
				}
				buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
				buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
				buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
				buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
				currIndex2entryValue = newDataClusterID;
			}
			else {
				if (!kernelFS->partition->readCluster(currIndex2entryValue, buffdata)) {
					return 0;
				}
			}
		}
	}
	else {
		if (!kernelFS->partition->readCluster(currIndex1entryValue, buffindex2)) {
			return 0;
		}
		currIndex2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
		currIndex2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
		currIndex2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
		currIndex2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
		if (!kernelFS->partition->readCluster(currIndex2entryValue, buffdata)) {
			return 0;
		}
	}
	//provarei da li se data cluster nalazi u kesu 
	for (BytesCnt i = 0; i < bytesCnt; ++i) {
		if (dataiterator == ClusterSize) { //mora da se prosiri fajl
			if (!kernelFS->partition->writeCluster(currIndex2entryValue, buffdata)) {
				return 0;
			}
			dataiterator = 0;
			if (++index2iterator == ClusterSize / 4) { // da li je i index2 popunjen
				if (!kernelFS->partition->writeCluster(currIndex1entryValue, buffindex2)) {
					return 0;
				}
				dirtyIndex1 = true;
				dirtyIndex2 = false; //??
				index2iterator = 0;
				if (++index1iterator == ClusterSize / 4) {
					return 0; // nema mesta (ne bi trablo da dodje do ovoga al neka ga - gore proverava ovaj uslov)
				}
				newindex2clusterID = kernelFS->getBitVector()->getNextAvailableCluster();
				if (newindex2clusterID < 0) {
					return 0;
				}
				myIndex1[index1iterator * 4 + 3] = (newindex2clusterID & 0xFF000000) >> 3 * 8;
				myIndex1[index1iterator * 4 + 2] = (newindex2clusterID & 0x00FF0000) >> 2 * 8;
				myIndex1[index1iterator * 4 + 1] = (newindex2clusterID & 0x0000FF00) >> 1 * 8;
				myIndex1[index1iterator * 4] = (newindex2clusterID & 0x000000FF);
				newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
				if (newDataClusterID < 0) {
					return 0;
				}
				for (int i = 0; i < ClusterSize; ++i) buffindex2[i] = 0x00;
				buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
				buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
				buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
				buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
				currIndex1entryValue = newindex2clusterID;
				currIndex2entryValue = newDataClusterID;
			}
			else {
				dirtyIndex2 = true;
				newDataClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
				if (newDataClusterID < 0) {
					return 0;
				}
				buffindex2[index2iterator * 4 + 3] = (newDataClusterID & 0xFF000000) >> 3 * 8;
				buffindex2[index2iterator * 4 + 2] = (newDataClusterID & 0x00FF0000) >> 2 * 8;
				buffindex2[index2iterator * 4 + 1] = (newDataClusterID & 0x0000FF00) >> 1 * 8;
				buffindex2[index2iterator * 4] = (newDataClusterID & 0x000000FF);
				currIndex2entryValue = newDataClusterID;
			}
		}
		buffdata[dataiterator] = buffer[i];
		++dataiterator;
		++fsize;
		++mySeek;
	}
	if (dirtyIndex1) {
		if (!kernelFS->partition->writeCluster(myFCB->getIndex1ClusterID(), myIndex1)) {
			return 0;
		}
	}
	if (dirtyIndex2) {
		if (!kernelFS->partition->writeCluster(currIndex1entryValue, buffindex2)) {
			return 0;
		}
	}
	if (!kernelFS->partition->writeCluster(currIndex2entryValue, buffdata)) {
		return 0;
	}
	myFCB->setSize(fsize);
	delete[] buffdata;
	delete[]buffindex2;
	return 0;
	}
}

BytesCnt KernelFile::read(BytesCnt bytesCnt, char* buffer)
{
	if (kernelFS->CACHE_MOD) {
		if (!buffer || (mySeek == myFCB->getFileSize())) {
			return 0;
		}
		BytesCnt ret = 0;
		ClusterNo index1iterator = mySeek / ((ClusterSize / 4) * ClusterSize);
		ClusterNo index2iterator = (mySeek - (ClusterSize * ClusterSize / 4) * index1iterator) / ClusterSize;
		ClusterNo dataiterator = mySeek - (index1iterator * (ClusterSize / 4) * ClusterSize + index2iterator * ClusterSize); //od 0 do 2047
		ClusterNo index1entryValue = 0;
		ClusterNo index2entryValue = 0;
		char* buffindex2 = new char[ClusterSize];
		char* buffdata; // = new char[ClusterSize];
		if (!buffindex2) {
			return 0;
		}
		//
		if (currClusterID == 0) {
			index1entryValue |= (myIndex1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
			index1entryValue |= (myIndex1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
			index1entryValue |= (myIndex1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
			index1entryValue |= (myIndex1[index1iterator * 4] & 0x000000FF);
			if (!kernelFS->partition->readCluster(index1entryValue, buffindex2)) {
				return 0;
			}
			index2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
			index2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
			index2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
			index2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
			currClusterID = index2entryValue;
		}
		//provarei da li se data cluster nalazi u kesu 
		//if (!kernelFS->partition->readCluster(index2entryValue, buffdata)) {
		//	return 0;
		//}
		buffdata = kernelFS->cache->get(currClusterID);
		if (!buffdata) {
			buffdata = new char[ClusterSize];
			if (!kernelFS->partition->readCluster(currClusterID, buffdata)) {
				return 0;
			}
			kernelFS->cache->put(currClusterID, false, buffdata);
		}
		int i = 0;
		while (bytesCnt > 0 && mySeek != myFCB->getFileSize()) {
			if (dataiterator == ClusterSize) {
				dataiterator = 0;
				if (++index2iterator == ClusterSize / 4) {
					index2iterator = 0;
					++index1iterator;
					index1entryValue = 0;
					index1entryValue |= (myIndex1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
					index1entryValue |= (myIndex1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
					index1entryValue |= (myIndex1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
					index1entryValue |= (myIndex1[index1iterator * 4] & 0x000000FF);
					if (!kernelFS->partition->readCluster(index1entryValue, buffindex2)) {
						return 0;
					}
					index2entryValue = 0;
					index2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
					index2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
					index2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
					index2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
					//if (!kernelFS->partition->readCluster(index2entryValue, buffdata)) {
					//	return 0;
					//}
					currClusterID = index2entryValue;
					buffdata = kernelFS->cache->get(currClusterID);
					if (!buffdata) {
						buffdata = new char[ClusterSize];
						if (!kernelFS->partition->readCluster(currClusterID, buffdata)) {
							return 0;
						}
						kernelFS->cache->put(currClusterID, false, buffdata);
					}
				}
				else {
					index2entryValue = 0;
					index2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
					index2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
					index2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
					index2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
					//if (!kernelFS->partition->readCluster(index2entryValue, buffdata)) {
					//	return 0;
					//}
					currClusterID = index2entryValue;
					buffdata = kernelFS->cache->get(currClusterID);
					if (!buffdata) {
						buffdata = new char[ClusterSize];
						if (!kernelFS->partition->readCluster(currClusterID, buffdata)) {
							return 0;
						}
						kernelFS->cache->put(currClusterID, false, buffdata);
					}
				}
			}
			buffer[i] = buffdata[dataiterator];
			++dataiterator;
			++i;
			++mySeek;
			if (mySeek % ClusterSize == 0) currClusterID = 0;
			--bytesCnt;
			++ret;
		}

		buffdata = nullptr;
		delete buffdata;
		delete[]buffindex2;
		return ret;
	}
	else {
	if (!buffer || (mySeek == myFCB->getFileSize())) {
		return 0;
	}
	BytesCnt ret = 0;
	ClusterNo index1iterator = mySeek / ((ClusterSize / 4) * ClusterSize);
	ClusterNo index2iterator = (mySeek - (ClusterSize * ClusterSize / 4) * index1iterator) / ClusterSize;
	ClusterNo dataiterator = mySeek - (index1iterator * (ClusterSize / 4) * ClusterSize + index2iterator * ClusterSize); //od 0 do 2047
	ClusterNo index1entryValue = 0;
	ClusterNo index2entryValue = 0;
	char* buffindex2 = new char[ClusterSize];
	char* buffdata = new char[ClusterSize];
	if (!buffdata || !buffindex2) {
		return 0;
	}
	//
	index1entryValue |= (myIndex1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
	index1entryValue |= (myIndex1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
	index1entryValue |= (myIndex1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
	index1entryValue |= (myIndex1[index1iterator * 4] & 0x000000FF);
	if (!kernelFS->partition->readCluster(index1entryValue, buffindex2)) {
		return 0;
	}
	index2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
	index2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
	index2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
	index2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
	//provarei da li se data cluster nalazi u kesu 
	if (!kernelFS->partition->readCluster(index2entryValue, buffdata)) {
		return 0;
	}
	int i = 0;
	while (bytesCnt > 0 && mySeek != myFCB->getFileSize()) {
		if (dataiterator == ClusterSize) {
			dataiterator = 0;
			if (++index2iterator == ClusterSize / 4) {
				index2iterator = 0;
				++index1iterator;
				index1entryValue = 0;
				index1entryValue |= (myIndex1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
				index1entryValue |= (myIndex1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
				index1entryValue |= (myIndex1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
				index1entryValue |= (myIndex1[index1iterator * 4] & 0x000000FF);
				if (!kernelFS->partition->readCluster(index1entryValue, buffindex2)) {
					return 0;
				}
				index2entryValue = 0;
				index2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
				index2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
				index2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
				index2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
				if (!kernelFS->partition->readCluster(index2entryValue, buffdata)) {
					return 0;
				}
			}
			else {
				index2entryValue = 0;
				index2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
				index2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
				index2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
				index2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
				if (!kernelFS->partition->readCluster(index2entryValue, buffdata)) {
					return 0;
				}
			}
		}
		buffer[i] = buffdata[dataiterator];
		++dataiterator;
		++i;
		++mySeek;
		--bytesCnt;
		++ret;
	}

	delete[]buffdata;
	delete[]buffindex2;
	return ret;
	}
}

char KernelFile::seek(BytesCnt bytesCnt)
{
	if (bytesCnt < 0 || bytesCnt > MAX_FILE_SIZE || myFCB->getFileSize() < bytesCnt) {
		return 0;
	}
	mySeek = bytesCnt;
	currClusterID = 0;;
	return 1;
}

BytesCnt KernelFile::filePos()
{
	return mySeek;
}

char KernelFile::eof()
{
	BytesCnt size = myFCB->getFileSize();
	if (!size) {
		return 1; //prazan fajl
	}
	if (mySeek == size) {
		return 2;
	}
	else {
		return 0;
	}
}

BytesCnt KernelFile::getFileSize()
{
	return myFCB->getFileSize();
}

char KernelFile::truncate()
{
	if (mySeek == myFCB->getFileSize()) {
		return 0; // ako je fajl prazan ili je myseek na kraju fajla
	}
	ClusterNo index1iterator = mySeek / ((ClusterSize / 4) * ClusterSize);
	ClusterNo index2iterator = (mySeek - (ClusterSize * ClusterSize / 4) * index1iterator) / ClusterSize;
	ClusterNo startindex1iterator = index1iterator;
	ClusterNo startindex2iterator = index2iterator;
	ClusterNo currIndex1entryValue = 0;
	ClusterNo currIndex2entryValue = 0;
	char* buffindex2 = new char[ClusterSize];
	if (!buffindex2) {
		return 0;
	}
	if (mySeek % ClusterSize != 0) {
		++index2iterator;
		if (index2iterator == ClusterSize / 4) {
			index2iterator = 0;
			++index1iterator;
		}
		startindex2iterator = index2iterator;
	}
	//
	for (index1iterator; index1iterator < ClusterSize / 4; ++index1iterator) {
		currIndex1entryValue = 0;
		currIndex1entryValue |= (myIndex1[index1iterator * 4 + 3] & 0x000000FF) << 3 * 8;
		currIndex1entryValue |= (myIndex1[index1iterator * 4 + 2] & 0x000000FF) << 2 * 8;
		currIndex1entryValue |= (myIndex1[index1iterator * 4 + 1] & 0x000000FF) << 1 * 8;
		currIndex1entryValue |= (myIndex1[index1iterator * 4] & 0x000000FF);
		if (currIndex1entryValue == 0) break;
		if (!kernelFS->partition->readCluster(currIndex1entryValue, buffindex2)) {
			return 0;
		}
		//
		for (index2iterator; index2iterator < ClusterSize / 4; ++index2iterator) {
			currIndex2entryValue = 0;
			currIndex2entryValue |= (buffindex2[index2iterator * 4 + 3] & 0x000000FF) << 3 * 8;
			currIndex2entryValue |= (buffindex2[index2iterator * 4 + 2] & 0x000000FF) << 2 * 8;
			currIndex2entryValue |= (buffindex2[index2iterator * 4 + 1] & 0x000000FF) << 1 * 8;
			currIndex2entryValue |= (buffindex2[index2iterator * 4] & 0x000000FF);
			if (currIndex2entryValue == 0) break;
			kernelFS->getBitVector()->freeCluster(currIndex2entryValue, false);
			if (kernelFS->CACHE_MOD) kernelFS->cache->deleteEntry(currIndex2entryValue);
			if (startindex2iterator != 0 && index1iterator == startindex1iterator) { // ako se trenutni index2 nece obrisati
				buffindex2[index2iterator * 4 + 0] = 0x00;
				buffindex2[index2iterator * 4 + 1] = 0x00;
				buffindex2[index2iterator * 4 + 2] = 0x00;
				buffindex2[index2iterator * 4 + 3] = 0x00;
			}
		}
		if (startindex2iterator != 0 && index1iterator == startindex1iterator) //ako smo poceli od polovine nekog indexa2.nivoa treba ga azurirati
			if (!kernelFS->partition->writeCluster(currIndex1entryValue, buffindex2)) {
				return 0;
			}
			else { // ako vise nismo unutar prvog indexa2.nivoa treba obrisati taj index2
				kernelFS->getBitVector()->freeCluster(currIndex1entryValue, false);
			}
		index2iterator = 0;
	}
	myFCB->setSize(mySeek);
	if (!kernelFS->getBitVector()->save()) {
		return 0;
	}
	delete[] buffindex2;
	return 1;
}

void KernelFile::setKernelFS(KernelFS* k)
{
	if (!kernelFS)
		kernelFS = k;
}

KernelFS* KernelFile::getKernelFS()
{
	return kernelFS;
}

char KernelFile::setFCB(FCB* fcb)
{
	myFCB = fcb;
	if (!myIndex1 || !kernelFS) return 0;
	if (fcb->getIndex1ClusterID() != 0) {
		if (!kernelFS->partition->readCluster(myFCB->getIndex1ClusterID(), myIndex1)) return 0;
	}
	else {
		ClusterNo newIndex1ClusterID = kernelFS->getBitVector()->getNextAvailableCluster();
		fcb->setIndex1ClusterID(newIndex1ClusterID);
		if (newIndex1ClusterID < 0) return 0;
		for (int i = 0; i < ClusterSize; ++i) myIndex1[i] = 0x00;
		return 1;
	}
}

void KernelFile::setSeek(BytesCnt s)
{
	mySeek = s;
}



