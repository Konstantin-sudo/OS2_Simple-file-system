#pragma once
class LRUList {
public:
	class ElemLRU {
	public:
		int i;
		ElemLRU* next;
		ElemLRU* prev;
	
		 ElemLRU(int n) {
			 i = n;
			 next = prev = nullptr;
		 }
		 ~ElemLRU() {
			 next = prev = nullptr;
		 }
	};

	LRUList();
	~LRUList();
	void put(int i);
	int getNext();
	int getIndex(int i); //vraca "i" (vrednost tog elementa) i-tog elementa iz liste
	void upgradeElem(int i); // postavlja i-ti elem liste na pocetak liste (postaje najskorije pristupan)

private:
	ElemLRU* head;
	ElemLRU* tail;

};