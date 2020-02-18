#pragma once

const unsigned long CACHE_SIZE = 10; // broj klastera koji se cuvaju u kesu  -  5 * 2048 (B) --> cache = 10KB
struct CACHE_ENTRY {
	unsigned long id;
	bool dirty;
	char* buffer;
};

class LRUList;
class Partition;
class Cache {
public:
	Cache();
	~Cache();
	
	void setPartition(Partition*);
	void put(unsigned  long id, bool dirty, char* buff);
	char* get(unsigned long id);
	void setDirty(unsigned long id, bool d);
	void update();
	void format();
	void deleteEntry(unsigned long id);

private:
	CACHE_ENTRY** cache;
	int cnt;
	int next;
	Partition* partition;
	LRUList *lruList;
};