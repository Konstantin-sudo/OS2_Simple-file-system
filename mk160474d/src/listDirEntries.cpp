#include "listDirEntries.h"
#include "directory.h"

ListOfAvailableEntries::ListOfAvailableEntries()
{
	head = tail = nullptr;
}

ListOfAvailableEntries::~ListOfAvailableEntries()
{
	while (head) {
		ElemDir* old = head;
		head = head->next;
		delete old;
	}
}

void ListOfAvailableEntries::put(DirEntryPath* p)
{
	ElemDir* newE = new ElemDir(p);
	if (head == nullptr) {
		head = tail = newE;
	}
	else {
		ElemDir* curr = head;
		ElemDir* prev = nullptr;
		while ((newE->myPath->entryInDirIndex1 > curr->myPath->entryInDirIndex1
			|| newE->myPath->entryInDirIndex2 > curr->myPath->entryInDirIndex2
			|| newE->myPath->entryInDirData > curr->myPath->entryInDirData) && curr != nullptr)
		{
			prev = curr;
			curr = curr->next;
		}
		if (prev) {
			prev->next = newE;
			newE->next = curr;
		}
		else {
			newE->next = head;
			head = newE;
		}
	}
}

void ListOfAvailableEntries::putOnEnd(DirEntryPath* p)
{
	ElemDir* newE = new ElemDir(p);
	if (head) {
		tail->next = newE;
		tail = newE;
	}
	else head = tail = newE;
}

void ListOfAvailableEntries::putOnHead(DirEntryPath* p)
{
	ElemDir* newE = new ElemDir(p);
	if (!head) head = tail = newE;
	else {
		newE->next = head;
		head = newE;
	}
}

DirEntryPath* ListOfAvailableEntries::get()
{
	if (!head) return nullptr;
	ElemDir* ret = head;
	head = head->next;
	ret->next = nullptr;
	return ret->myPath;
}

void ListOfAvailableEntries::deleteAllElemFromOneCluster(DirEntryPath* p)
{
	ElemDir* curr = head;
	ElemDir* prev = nullptr;
	while (curr->myPath->entryInDirIndex1 != p->entryInDirIndex1 && curr->myPath->entryInDirIndex2 != p->entryInDirIndex2 && curr) {
		prev = curr;
		curr = curr->next;
	}
	if (curr) {
		if (!prev) head = head->next;
		else prev->next = curr->next;
		curr->next = nullptr;
		delete curr->myPath;
	}
}
