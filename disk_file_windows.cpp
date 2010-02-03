// Windows files

void windowsError(const char* where = NULL)
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);
	if (where)
		error(format("%s: %s", where, lpMsgBuf));
	else
		error((LPCSTR)lpMsgBuf);
}

template<class NODE>
class Stream
{
protected:
	HANDLE archive;

public:
	Stream() : archive(0) {}

	bool isOpen() const { return archive != 0; }

	uint64_t size()
	{
		ULARGE_INTEGER li;
		li.LowPart = GetFileSize(archive, &li.HighPart);
		return li.QuadPart / sizeof(NODE);
	}

	uint64_t position()
	{
		LARGE_INTEGER n;
		DWORD error;
		n.QuadPart = 0;
		n.LowPart = SetFilePointer(archive, n.LowPart, &n.HighPart, FILE_CURRENT);
		if (n.LowPart == INVALID_SET_FILE_POINTER && (error=GetLastError()) != NO_ERROR)
			windowsError("Seek error");
		return n.QuadPart / sizeof(NODE);
	}

	void seek(uint64_t pos)
	{
		LARGE_INTEGER li;
		DWORD error;
		li.QuadPart = pos * sizeof(NODE);
		li.LowPart = SetFilePointer(archive, li.LowPart, &li.HighPart, FILE_BEGIN);
		if (li.LowPart == INVALID_SET_FILE_POINTER && (error=GetLastError()) != NO_ERROR)
			windowsError("Seek error");
	}

	void close()
	{
		if (archive)
		{
			CloseHandle(archive);
			archive = 0;
		}
	}

	~Stream()
	{
		close();
	}
};

template<class NODE>
class OutputStream : virtual public Stream<NODE>
{
private:
	BYTE sectorBuffer[512]; // currently assumes 512 byte sectors; maybe use GetDiskFreeSpace() in the future for increased portability
	WORD sectorBufferUse;
	WORD sectorBufferFlushed;
	char filenameOpened[MAX_PATH];

public:
	OutputStream() : sectorBufferUse(0), sectorBufferFlushed(0) {}

	OutputStream(const char* filename, bool resume=false) : sectorBufferUse(0), sectorBufferFlushed(0)
	{
		open(filename, resume);
	}

	~OutputStream()
	{
		close();
	}

	void close()
	{
		flush(false);
		//Stream::close();
	}

	void open(const char* filename, bool resume=false)
	{
		strcpy(filenameOpened, filename);
		archive = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, resume ? OPEN_EXISTING : CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING , NULL);
		if (archive == INVALID_HANDLE_VALUE)
			windowsError(format("File creation failure (%s)", filename));
		if (resume)
		{
			LARGE_INTEGER li;
			li.QuadPart = 0;
			BOOL b = SetFilePointerEx(archive, li, NULL, FILE_END);
			if (!b)
				windowsError("Append error");
		}
	}

	void write(const NODE* p, size_t n)
	{
		assert(archive, "File not open");
		size_t total = n * sizeof(NODE);
		size_t bytes = 0;
		const BYTE* data = (const BYTE*)p;
		while (bytes < total) // write in 256 KB chunks
		{
			size_t left = total-bytes;
			DWORD chunk = left > 256*1024 ? 256*1024 : (DWORD)left;
			DWORD r;
			BOOL b;
			if (sectorBufferUse)
			{
				if (chunk > sizeof(sectorBuffer) - sectorBufferUse)
					chunk = sizeof(sectorBuffer) - sectorBufferUse;
				memcpy(sectorBuffer + sectorBufferUse, data + bytes, chunk);
				sectorBufferUse += (WORD)chunk;
				if (sectorBufferUse == sizeof(sectorBuffer))
				{
					b = WriteFile(archive, sectorBuffer, sizeof(sectorBuffer), &r, NULL);
					if (!b)
						windowsError("Write error");
					if (r != sizeof(sectorBuffer))
						windowsError("Out of disk space?");
					sectorBufferUse = 0;
					sectorBufferFlushed = 0;
				}
				bytes += chunk;
				continue;
			}
			if (chunk % sizeof(sectorBuffer) != 0)
			{
				if (chunk > sizeof(sectorBuffer))
					chunk &= -(int)sizeof(sectorBuffer);
				else
				{
					memset(sectorBuffer, 0, sizeof(sectorBuffer));
					memcpy(sectorBuffer, data + bytes, chunk);
					sectorBufferUse = (WORD)chunk;
					return;
				}
			}
			b = WriteFile(archive, data + bytes, chunk, &r, NULL);
			if (!b)
				windowsError("Write error");
			if (r == 0)
				windowsError("Out of disk space?");
			bytes += r;
		}
	}

	void flush(bool reopen=true)
	{
		if (!archive)
			return;

		if (sectorBufferFlushed == sectorBufferUse) // nothing to flush?
		{
			if (!reopen)
			{
				CloseHandle(archive);
				archive = 0;
			}
			return;
		}
		assert(sectorBufferFlushed < sectorBufferUse);

		BOOL b;
		DWORD r;
		ULARGE_INTEGER size;
		size.LowPart = GetFileSize(archive, &size.HighPart);

		b = WriteFile(archive, sectorBuffer, sizeof(sectorBuffer), &r, NULL);
		if (!b)
			windowsError("Write error");
		if (r != sizeof(sectorBuffer))
			windowsError("Out of disk space?");
		CloseHandle(archive);

		archive = CreateFile(filenameOpened, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH, NULL);
		if (archive == INVALID_HANDLE_VALUE)
			windowsError(format("File creation failure upon reopening (%s) buffered", filenameOpened));
		b = SetFilePointer(archive, sectorBufferUse - sizeof(sectorBuffer), NULL, FILE_END);
		if (!b)
			windowsError("SetFilePointer() #1 error");
		b = SetEndOfFile(archive);
		if (!b)
			windowsError("SetEndOfFile() error");
		//FlushFileBuffers(archive);
		CloseHandle(archive);
		sectorBufferFlushed = sectorBufferUse;
		if (!reopen)
		{
			archive = 0;
			return;
		}

		archive = CreateFile(filenameOpened, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING, NULL);
		if (archive == INVALID_HANDLE_VALUE)
			windowsError(format("File creation failure upon reopening (%s) unbuffered", filenameOpened));
		b = SetFilePointerEx(archive, (LARGE_INTEGER&)size, NULL, FILE_BEGIN);
		if (!b)
			windowsError("SetFilePointer() #2 error");
	}
};

template<class NODE>
class InputStream : virtual public Stream<NODE>
{
private:
	BYTE sectorBuffer[512]; // currently assumes 512 byte sectors; maybe use GetDiskFreeSpace() in the future for increased portability
	unsigned sectorBufferPos;
	bool endOfFileReached;
	uint64_t filePosition;

public:
	InputStream() : sectorBufferPos(0), endOfFileReached(false), filePosition(0) {}

	InputStream(const char* filename) : sectorBufferPos(0), endOfFileReached(false), filePosition(0)
	{
		open(filename);
	}

	void open(const char* filename)
	{
		archive = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING, NULL);
		if (archive == INVALID_HANDLE_VALUE)
			windowsError(format("File open failure (%s)", filename));
	}

	uint64_t position()
	{
		return filePosition / sizeof(NODE);
	}

	void seek(uint64_t pos)
	{
		filePosition = pos * sizeof(NODE);
		sectorBufferPos = (unsigned)(pos % sizeof(sectorBuffer));

		LARGE_INTEGER li;
		DWORD error;
		li.QuadPart = filePosition & -(int64_t)sizeof(sectorBuffer);
		li.LowPart = SetFilePointer(archive, li.LowPart, &li.HighPart, FILE_BEGIN);
		if (li.LowPart == INVALID_SET_FILE_POINTER && (error=GetLastError()) != NO_ERROR)
			windowsError("Seek error");
		endOfFileReached = false;

		if (sectorBufferPos)
		{
			DWORD r;
			BOOL b = ReadFile(archive, sectorBuffer, sizeof(sectorBuffer), &r, NULL);
			if (b && r<sectorBufferPos)
				endOfFileReached = true;
			if (!b || r==0)
				windowsError(format("Read error %d", GetLastError()));
		}
	}

	size_t read(NODE* p, size_t n)
	{
		assert(archive, "File not open");
		size_t total = n * sizeof(NODE);
		size_t bytes = 0;
		char* data = (char*)p;
		while (bytes < total) // read in 256 KB chunks
		{
			if (endOfFileReached)
				error("Read error, end of file");
			size_t left = total-bytes;
			DWORD chunk = left > 256*1024 ? 256*1024 : (DWORD)left;
			DWORD r = 0;
			BOOL b;
			if (sectorBufferPos)
			{
				if (chunk > sizeof(sectorBuffer) - sectorBufferPos)
					chunk = sizeof(sectorBuffer) - sectorBufferPos;
				memcpy(data + bytes, sectorBuffer + sectorBufferPos, chunk);
				sectorBufferPos += (WORD)chunk;
				if (sectorBufferPos == sizeof(sectorBuffer))
					sectorBufferPos = 0;
				filePosition += chunk;
				bytes += chunk;
				continue;
			}
			if (chunk % sizeof(sectorBuffer) != 0)
			{
				if (chunk > sizeof(sectorBuffer))
					chunk &= -(int)sizeof(sectorBuffer);
				else
				{
					b = ReadFile(archive, sectorBuffer, sizeof(sectorBuffer), &r, NULL);
					if (b && r!=sizeof(sectorBuffer))
					{
						endOfFileReached = true;
						if (r<chunk)
						{
							memcpy(data + bytes, sectorBuffer, r);
							bytes += r;
							assert(bytes % sizeof(NODE) == 0, "Unaligned EOF");
							return bytes / sizeof(NODE);
						}
					}
					if (!b || r==0)
						windowsError(format("Read error %d", GetLastError()));
					memcpy(data + bytes, sectorBuffer, chunk);
					sectorBufferPos = (WORD)chunk;
					filePosition += chunk;
					return n;
				}
			}
			b = ReadFile(archive, data + bytes, chunk, &r, NULL);
			if (b && r<chunk)
			{
				endOfFileReached = true;
				bytes += r;
				assert(bytes % sizeof(NODE) == 0, "Unaligned EOF");
				return bytes / sizeof(NODE);
			}
			if (!b || r==0)
				windowsError(format("Read error %d", GetLastError()));
			filePosition += r;
			bytes += r;
		}
		return n;
	}
};

// For in-place filtering. Written nodes must be <= read nodes.
template<class NODE>
class RewriteStream : public InputStream<NODE>, public OutputStream<NODE>
{
	uint64_t readpos, writepos;
public:
	RewriteStream(){}

	RewriteStream(const char* filename)
	{
		open(filename);
	}
	
	void open(const char* filename)
	{
		archive = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (archive == INVALID_HANDLE_VALUE)
			windowsError("File creation failure");
		readpos = writepos = 0;
	}

	size_t read(NODE* p, size_t n)
	{
		assert(readpos >= writepos, "Write position overwritten");
		seek(readpos);
		size_t r = InputStream::read(p, n);
		readpos += r;
		return r;
	}

	void write(const NODE* p, size_t n)
	{
		seek(writepos);
		OutputStream::write(p, n);
		writepos += n;
	}

	void truncate()
	{
		seek(writepos);
		SetEndOfFile(archive);
	}
};

void deleteFile(const char* filename)
{
	BOOL b = DeleteFile(filename);
	if (!b)
		windowsError(format("Error deleting file %s", filename));
}

void renameFile(const char* from, const char* to)
{
	DeleteFile(to); // ignore error
	BOOL b = MoveFile(from, to);
	if (!b)
		windowsError(format("Error moving file from %s to %s", from, to));
}

bool fileExists(const char* filename)
{
	return GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES;
}

uint64_t getFreeSpace()
{
	char dir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, dir);
	ULARGE_INTEGER li;
	if (!GetDiskFreeSpaceEx(dir, &li, NULL, NULL))
		windowsError("GetDiskFreeSpaceEx error");
	return li.QuadPart;
}
