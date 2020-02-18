#pragma once
#include "fs.h"
#include <windows.h>

const int MAX_THREAD_CNT = 32;
#define signal(x) ReleaseSemaphore(x,1,NULL)
#define signalCnt(x,y) ReleaseSemaphore(x,y,NULL)
#define wait(x) WaitForSingleObject(x,INFINITE)


class Cache;
class Partition;
class BitVector;
class Directory;
class ListOfOpenedFiles;
class KernelFS {
public:
	KernelFS();
	~KernelFS();

	char mount(Partition* partition);
	char unmount();
	char format();
	FileCnt readRootDir();
	char doesExist(char* fname);
	File* open(char* fname, char mode);
	char deleteFile(char* fname);

	char initialize();
	BitVector* getBitVector();
	Directory* getDirectory();
private:
	friend class KernelFile;
	Partition* partition;
	BitVector* bitVector;
	Directory* directory;
	ListOfOpenedFiles* globOpenFilesTable;
	bool isFormatted;
	bool isInitialized; // da li je fajl sistem inicijalizovao potrebne podatke da bi mogao da radi sa paricijom koja vec ima Fsistem

	Cache* cache;
	bool CACHE_MOD;

	//sinhronizacija:
	bool openable;
	HANDLE semMount;
	HANDLE semUnmount;
	HANDLE semFormat;
	HANDLE mutexFS;
	int semMountBlockedCnt;
	int semUnmountBlockedCnt;
	int semFormatBlockedCnt;
};