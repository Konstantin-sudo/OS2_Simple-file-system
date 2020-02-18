#pragma once
#include "fs.h"
#include "part.h"
#include <unordered_map>
using namespace std;

typedef struct DirEntryPath {
	unsigned short entryInDirIndex1; //od 0 do 512
	ClusterNo valueOfentryInDirIndex1; //16 bita je dovoljno - 16384 klastera postoji na particiji (po bitvektoru)
	unsigned short entryInDirIndex2; //od 0 do 512
	ClusterNo valueOfentryInDirIndex2;
	unsigned short entryInDirData; // od 0 do 101 - 102 ulaza od 20 B u klasteru velicine 2048 B (2048 / 20 = 102)
	ClusterNo valueOfentryInDirData; // samo br klustera koji predstavlja index1 fajla
}; // = 192 B

class FCB;
class BitVector;
class ListOfAvailableEntries;
class Directory {
public:
	Directory(Partition* p);
	~Directory();

	void unmount();
	void format();
	char* getIndex1();

	char createNewFile(char* fileName, BitVector*); //povratna vrednost
	char deleteFileEntry(char* fname, BitVector*);
	char* createEntry(char* fileName); //alocira 20 B
	char expand(BitVector* bitVector);
	char initialize();
	FileCnt getNumberOfFiles();
	char doesExist(char* fname);
	ClusterNo getFileIndex1ID(char* fname);
	char* getEntry(char* fname);
	char updateEntry(FCB* fcb);

private:
	Partition* partition;
	char* index1;
	FileCnt numberOfFiles;
	unordered_map<std::string, DirEntryPath*> cacheDir; //naziv fajla --> putanju do indeksa 1 tog fajla, nije bas kes mem jer se tu nalaze svi fajlovi koji su trenutno u dir-u, ok za ovaj projekat
	ListOfAvailableEntries* availableEntriesInDir;
};