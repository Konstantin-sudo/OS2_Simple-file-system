#include "listOfopenedFiles.h"
#include "kernelFS.h"
ListOfOpenedFiles::ListOfOpenedFiles()
{
	head = nullptr;
	tail = nullptr;
	cnt = 0;
}

ListOfOpenedFiles::~ListOfOpenedFiles()
{
	while (head) {
		ElemFile* old = head;
		head = head->next;
		delete old;
	}
}

void ListOfOpenedFiles::put(FCB* fcb)
{
	ElemFile* newE = new ElemFile(fcb);
	if (!head) head = tail = newE;
	else {
		tail->next = newE;
		tail = newE;
	}
	++cnt;
}

FCB* ListOfOpenedFiles::getfcb(char* fname)
{
	ElemFile* curr = head;
	while (curr) {
		if (!curr->fcb->getName().compare(fname)) break; //vraca 0 ako su jednaki
		curr = curr->next;
	}
	if (curr) return curr->fcb;
	else return nullptr;
}

void ListOfOpenedFiles::deleteElem(std::string fname)
{
	ElemFile* curr = head;
	ElemFile* prev = nullptr;
	while (curr) {
		if (!curr->fcb->getName().compare(fname)) break; //vraca 0 ako su jednaki
		prev = curr;
		curr = curr->next;
	}
	if (!curr) return;
	if (prev) prev->next = curr->next;
	else head = head->next;
	curr->next = nullptr;
	--cnt;
	delete curr;
	//mora signal semopen da se odradi, ili ovde ili napolju
}

unsigned int ListOfOpenedFiles::getCnt()
{
	return cnt;
}



