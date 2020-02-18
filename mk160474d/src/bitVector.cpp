#include "bitVector.h"


BitVector::BitVector(Partition* p)
{
	vector = new char[ClusterSize];
	vector[0] = 0x3F;
	for (unsigned long i = 1; i < ClusterSize; i++) {
		vector[i] = 0xFF;
	}
	nextAvailableCluster = 2;
	myPartition = p;
	numberOfAvailableClusters = myPartition->getNumOfClusters();
	MAX_NUMBER_OF_CLUSTERS = myPartition->getNumOfClusters();
}

BitVector::~BitVector()
{
	myPartition = nullptr;
	delete[] vector;
}

ClusterNo BitVector::getNextAvailableCluster()
{
	if (numberOfAvailableClusters == 0) return -1;

	//dohvatanje sl klustera
	ClusterNo ret = nextAvailableCluster;
	--numberOfAvailableClusters;

	//azuriranje bit vektora
	unsigned long vectorIterator = ret / 8;
	char mask = 0x01;
	mask <<= 8 - (ret % 8) - 1;
	mask = ~mask;
	vector[vectorIterator] &= mask;
	if (!myPartition->writeCluster(0, vector)) return -1;

	//azuriranje nextAvailableCluster
	nextAvailableCluster = (nextAvailableCluster + 1) % MAX_NUMBER_OF_CLUSTERS;
	unsigned long i = 0;
	bool condition = true;
	while (i < ClusterSize && condition) {
		vectorIterator = nextAvailableCluster / 8;
		char currByte = vector[vectorIterator];
		if (currByte) { //ima slobodnih klustera u tekucem bajtu vectora
			mask = 0x80;
			for (int j = 0; j < 8; j++) { //prolazi kroz tekuci bajt i trazi slobodan klaster (1)
				char old = currByte;
				old &= mask;
				if (old) {
					nextAvailableCluster = vectorIterator * 8 + j;
					condition = false;
					break;
				}
				else mask >>= 1;
			}
		}
		else {
			nextAvailableCluster = (nextAvailableCluster + (8 - nextAvailableCluster % 8)) % MAX_NUMBER_OF_CLUSTERS;
			i++;
		}
	}

	return ret;
}

char BitVector::freeCluster(ClusterNo clusterNo, bool save)
{
	unsigned long vectorIterator = clusterNo / 8;
	char mask = 0x01;
	mask <<= 8 - (clusterNo % 8) - 1;
	vector[vectorIterator] |= mask;
	numberOfAvailableClusters++;
	if (save) if (!myPartition->writeCluster(0, vector)) return 0;
}

char* BitVector::getVector()
{
	return vector;
}

void BitVector::format()
{
	vector[0] = 0x3F;
	for (int i = 1; i < ClusterSize; ++i) {
		vector[i] = 0xFF;
	}
	numberOfAvailableClusters = MAX_NUMBER_OF_CLUSTERS;
	nextAvailableCluster = 2;
}

char BitVector::save()
{
	if (!myPartition->writeCluster(0, vector)) return 0;
}
