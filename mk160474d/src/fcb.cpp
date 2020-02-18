#include "fcb.h"
#include "kernelFile.h"
#include "directory.h"

FCB::FCB(char* entry, char mode)
{
	this->mode = mode;
	sem = CreateSemaphore(NULL, 0, 1, NULL);
	
	//fname
	int nameiterator = 0;
	int entryiterator = 7;
	char* fname = new char[13];
	while (entry[entryiterator] == ' ') --entryiterator;
	while (entryiterator >= 0) {
		fname[nameiterator] = entry[entryiterator];
		++nameiterator;
		--entryiterator;
	}
	fname[nameiterator] = '.';
	++nameiterator;
	entryiterator = 10;
	while (entry[entryiterator] == ' ' && entryiterator > 7) --entryiterator;
	while (entryiterator > 7) {
		fname[nameiterator] = entry[entryiterator];
		++nameiterator;
		--entryiterator;
	}
	fname[nameiterator] = '\0';
	++nameiterator;
	filename = std::string(fname);


	//index1
	index1ClusterID = 0;
	entryiterator = 12;
	index1ClusterID |= (entry[entryiterator + 3] & 0x000000FF) << 3 * 8;
	index1ClusterID |= (entry[entryiterator + 2] & 0x000000FF) << 2 * 8;
	index1ClusterID |= (entry[entryiterator + 1] & 0x000000FF) << 1 * 8;
	index1ClusterID |= (entry[entryiterator] & 0x000000FF);
	
	//size
	fileSize = 0;
	entryiterator = 16;
	fileSize |= (entry[entryiterator + 3] & 0x000000FF) << 3 * 8;
	fileSize |= (entry[entryiterator + 2] & 0x000000FF) << 2 * 8;
	fileSize |= (entry[entryiterator + 1] & 0x000000FF) << 1 * 8;
	fileSize |= (entry[entryiterator] & 0x000000FF);
}

FCB::~FCB()
{
	CloseHandle(sem);
}

std::string FCB::getName()
{
	return filename;
}

ClusterNo FCB::getIndex1ClusterID()
{
	return index1ClusterID;
}

BytesCnt FCB::getFileSize()
{
	return fileSize;
}

char FCB::getMode()
{
	return mode;
}

HANDLE FCB::getSem()
{
	return sem;
}

void FCB::setSize(BytesCnt s)
{
	fileSize = s;
}

void FCB::setIndex1ClusterID(ClusterNo c)
{
	index1ClusterID = c;
}


