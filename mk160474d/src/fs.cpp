#include "fs.h"
#include "kernelFS.h"

KernelFS* FS::myImpl = new KernelFS();

FS::~FS()
{
	delete myImpl;
}

char FS::mount(Partition* partition)
{
	return myImpl->mount(partition);
}

char FS::unmount()
{
	return myImpl->unmount();
}

char FS::format()
{
	return myImpl->format();
}

FileCnt FS::readRootDir()
{
	return myImpl->readRootDir();
}

char FS::doesExist(char* fname)
{
	return myImpl->doesExist(fname);
}

File* FS::open(char* fname, char mode)
{
	return myImpl->open(fname, mode);
}

char FS::deleteFile(char* fname)
{
	return myImpl->deleteFile(fname);
}

FS::FS()
{
}
