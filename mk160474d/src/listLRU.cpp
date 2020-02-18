#include "listLRU.h"

LRUList::LRUList()
{
	head = tail = nullptr;
}

LRUList::~LRUList()
{
	while (head) {
		ElemLRU* old = head;
		head = head->next;
		delete old;
	}
}

void LRUList::put(int i)
{
	ElemLRU* newE = new ElemLRU(i);
	if (head) {
		tail->next = newE;
		newE->prev = tail;
		tail = newE;
	}
	else {
		head = tail = newE;
	}
}

int LRUList::getNext()
{
	int ret = tail->i;
	ElemLRU* curr = tail;
	tail = curr->prev;
	tail->next = nullptr;
	curr->prev = nullptr;
	curr->next = head;
	head->prev = curr;
	head = curr;
	
	return ret;
}

int LRUList::getIndex(int i)
{
	ElemLRU* curr = head;
	int j = 0;
	while (curr && j < i) {
		curr = curr->next;
		++j;
	}
	if (!curr) return 0;
	return curr->i;
}

void LRUList::upgradeElem(int i)
{
	ElemLRU* curr = head;
	int j = 0;
	while (curr && j < i) {
		curr = curr->next;
		++j;
	}
	if (!curr) return;
	if (curr != head) {
		ElemLRU* prev = curr->prev;
		prev->next = curr->next;
		curr->prev = nullptr;
		if (curr != tail) 
			curr->next->prev = prev;
		curr->next = head;
		head->prev = curr;
		head = curr;
	}
}
