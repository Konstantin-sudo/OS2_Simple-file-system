#pragma once

struct DirEntryPath;
class ListOfAvailableEntries {
public:

	class ElemDir {
	public:
		DirEntryPath* myPath;
		ElemDir* next;
		ElemDir(DirEntryPath* f, ElemDir* n = nullptr) {
			myPath = f;
			next = n;
		}
		~ElemDir() {
			//delete myPath;
			//delete next;
		}
	};

	ListOfAvailableEntries();
	~ListOfAvailableEntries();

	void put(DirEntryPath* p);
	void putOnEnd(DirEntryPath* p);
	void putOnHead(DirEntryPath* p);
	DirEntryPath* get();
	void deleteAllElemFromOneCluster(DirEntryPath* p);

private:
	ElemDir* head;
	ElemDir* tail;
};