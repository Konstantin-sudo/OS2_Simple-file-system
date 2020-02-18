#include "file.h"
#include "kernelFile.h"
File::~File()
{
	delete myImpl;
}

char File::write(BytesCnt b, char* buffer)
{
	return myImpl->write(b, buffer);
}

BytesCnt File::read(BytesCnt cnt, char* buffer)
{
	return myImpl->read(cnt, buffer);
}

char File::seek(BytesCnt cnt)
{
	return myImpl->seek(cnt);
}

BytesCnt File::filePos()
{
	return myImpl->filePos();
}

char File::eof()
{
	return myImpl->eof();
}

BytesCnt File::getFileSize()
{
	return myImpl->getFileSize();
}

char File::truncate()
{
	return myImpl->truncate();
}

File::File() {
	myImpl = new KernelFile(this);
}