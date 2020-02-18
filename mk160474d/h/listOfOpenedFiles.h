#pragma once
#include "fcb.h"
#include <iostream>

class ListOfOpenedFiles {
public:
	class ElemFile {
	public:
		FCB* fcb;
		ElemFile* next;
		ElemFile(FCB* f, ElemFile* n = 0) {
			fcb = f;
			next = n;
		}
		~ElemFile() {
			//delete fcb;
			//delete next;
		}
	};

	ListOfOpenedFiles();
	~ListOfOpenedFiles();

	void put(FCB* fcb);
	FCB* getfcb(char* fname);
	void deleteElem(std::string fname);
	unsigned int getCnt();

private:
	ElemFile* head;
	ElemFile* tail;
	unsigned int cnt;

};