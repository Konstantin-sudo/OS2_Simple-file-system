#pragma once
#include "fs.h"
#include "part.h"
#include <iostream>
#include <windows.h>

class FCB {
public:
	FCB(char* entry, char mode);
	~FCB();

	std::string getName();
	ClusterNo getIndex1ClusterID();
	BytesCnt getFileSize();
	char getMode();
	HANDLE getSem();

	void setSize(BytesCnt s);
	void setIndex1ClusterID(ClusterNo c);

private:
	std::string filename;
	ClusterNo index1ClusterID;
	BytesCnt fileSize;
	char mode;
	HANDLE sem;
};