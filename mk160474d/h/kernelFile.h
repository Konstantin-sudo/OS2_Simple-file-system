#pragma once
#include "kernelFS.h"
#include "part.h"

const BytesCnt MAX_FILE_SIZE = (ClusterSize / 4) * (ClusterSize / 4) * ClusterSize;

class FCB;
class KernelFile {
public:
	KernelFile(File* f);
	~KernelFile(); // // zatvara fajl

	char write(BytesCnt, char* buffer); //koliko bajtova treba upisati na tekucu poziciju i odakle
	BytesCnt read(BytesCnt, char* buffer); //
	char seek(BytesCnt); //
	BytesCnt filePos();
	char eof();
	BytesCnt getFileSize();
	char truncate(); //

	void setKernelFS(KernelFS* k);
	KernelFS* getKernelFS();
	char setFCB(FCB* fcb);
	void setSeek(BytesCnt);

private:
	KernelFS* kernelFS;
	File* myFile;
	FCB* myFCB; //name,index1,size,mode 
	BytesCnt mySeek; // od 0 do max_file_size   --> mySeek - 1 = posl bitan bajt (redni brojevi bajtova krecu od 0)
	ClusterNo currClusterID; // 0 znaci da ne sadrzi info bitno ako radi u cachemod u
	char* myIndex1;

};