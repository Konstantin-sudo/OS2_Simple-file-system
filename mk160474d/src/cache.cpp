#include "cache.h"
#include "part.h"
#include "listLRU.h"
#include <iostream>

Cache::Cache(){
	partition = nullptr;
	cache = new CACHE_ENTRY*[CACHE_SIZE];
	lruList = new LRUList();
	for (int i = 0; i < CACHE_SIZE; ++i) {
		cache[i] = nullptr;
		lruList->put(CACHE_SIZE - i - 1);
	}
	cnt = 0;
	next = 0;
}

Cache::~Cache()
{
	update();
	partition = nullptr;
	for (int i = 0; i < CACHE_SIZE; ++i) {
		if(cache[i]) delete[] cache[i]->buffer;
		delete cache[i];
	}
	delete[] cache;
	delete lruList;
}

void Cache::setPartition(Partition* p)
{
	partition = p;
}

void Cache::put(unsigned long id, bool dirty, char* buff)
{
	next = lruList->getNext();

	if (cache[next]){
		if (cache[next]->dirty == 1) {
			if (!partition->writeCluster(cache[next]->id, cache[next]->buffer)) {
				std::cout << "\n***PARTITION ERROR***\n";
				exit(-1);
			}
			delete[] cache[next]->buffer;
			delete  cache[next];
		}
	}
	else {
		++cnt;
	}
	cache[next] = new CACHE_ENTRY();
	cache[next]->id = id;
	cache[next]->dirty = dirty;
	cache[next]->buffer = buff;
	//next = (next + 1) % CACHE_SIZE;
}

char* Cache::get(unsigned long id)
{
	if (id < 0 || cnt == 0) return 0;
	int i = 0;
	
	while( i < CACHE_SIZE) {
		int j = lruList->getIndex(i);
		if (cache[j]) {
			if (cache[j]->id == id) {
				lruList->upgradeElem(i);
				return cache[j]->buffer;
			}
		}
		++i;
	}
	return 0;
}

void Cache::setDirty(unsigned long id, bool d)
{
	int i = 0;
	for ( i = 0; i < CACHE_SIZE; ++i) {
		if (cache[i]) {
			if (cache[i]->id == id) break;
		}
	}
	cache[i]->dirty = d;
}

void Cache::update()
{
	for (int i = 0; i < CACHE_SIZE; ++i) {
		if (cache[i]) {
			if (cache[i]->buffer) {
				if (cache[i]->dirty) {
					if (!partition->writeCluster(cache[i]->id, cache[i]->buffer)) {
						std::cout << "\n***PARTITION ERROR***\n";
						exit(-1);
					}
				}
			}
		}
	}
}

void Cache::format()
{
	delete lruList;
	lruList = new LRUList();
	for (int i = 0; i < CACHE_SIZE; ++i) {
		if(cache[i]) delete[] cache[i]->buffer;
		delete cache[i];
		cache[i] = nullptr;
		lruList->put(CACHE_SIZE - i - 1);
	}
	cnt = 0;
	next = 0;

}

void Cache::deleteEntry(unsigned long id)
{
	int i = 0;
	for (i = 0; i < CACHE_SIZE; ++i) {
		if (cache[i]) {
			if (cache[i]->id = id) break;
		}
	}
	delete cache[i]->buffer;
	delete cache[i];
	cache[i] = nullptr;
}
