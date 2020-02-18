#pragma once
#include "part.h"

class Partition;
class BitVector {
public:
	BitVector(Partition*);
	~BitVector();

	ClusterNo getNextAvailableCluster();
	char freeCluster(ClusterNo clusterNo, bool save = true);
	char* getVector();
	void format();
	char save();

private:
	Partition* myPartition;
	char* vector;
	ClusterNo nextAvailableCluster;
	unsigned long numberOfAvailableClusters;
	unsigned long MAX_NUMBER_OF_CLUSTERS;



};