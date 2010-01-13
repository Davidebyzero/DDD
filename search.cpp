#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include "config.h"

#include <time.h>
#include <sys/timeb.h>
//#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __GNUC__
#include <stdint.h>
#else
#include "pstdint.h"
#endif

#ifdef _WIN32
#define SLEEP(x) Sleep(x)
#else
#include <unistd.h>
#define SLEEP(x) usleep((x)*1000)
#endif

#ifdef MULTITHREADING
#if defined(THREAD_BOOST)
#include "thread_boost.cpp"
#elif defined(THREAD_WINAPI)
#include "thread_winapi.cpp"
#else
#error Thread plugin not set
#endif

#if defined(SYNC_BOOST)
#include "sync_boost.cpp"
#elif defined(SYNC_WINAPI)
#include "sync_winapi.cpp"
#elif defined(SYNC_WINAPI_SPIN)
#include "sync_winapi_spin.cpp"
#elif defined(SYNC_INTEL_SPIN)
#include "sync_intel_spin.cpp"
#else
#error Sync plugin not set
#endif
// TODO: look into user-mode scheduling
#endif

// ******************************************** Utility code ********************************************

void error(const char* message = NULL)
{
	if (message)
		throw message;
	else
		throw "Unspecified error";
}

char* getTempString()
{
	static char buffers[64][1024];
	static int bufIndex = 0;
	int index;
	{
#ifdef MULTITHREADING
		static MUTEX mutex;
		SCOPED_LOCK lock(mutex);
#endif
		index = bufIndex++ % 64;
	}
		
	return buffers[index];
}

const char* format(const char *fmt, ...)
{    
	va_list argptr;
	va_start(argptr,fmt);
	//static char buf[1024];
	//char* buf = (char*)malloc(1024);
	char* buf = getTempString();
	vsprintf(buf, fmt, argptr);
	va_end(argptr);
	return buf;
}

const char* defaultstr(const char* a, const char* b = NULL) { return b ? b : a; }

// enforce - check condition in both DEBUG/RELEASE, error() on fail
// assert - check condition in DEBUG builds, try to instruct compiler to assume the condition is true in RELEASE builds
// debug_assert - check condition in DEBUG builds, do nothing in RELEASE builds (classic ASSERT)

#define enforce(expr,...) while(!(expr)){error(defaultstr(format("Check failed at %s:%d", __FILE__,  __LINE__), __VA_ARGS__));throw "Unreachable";}

#undef assert
#ifdef DEBUG
#define assert enforce
#define debug_assert enforce
#define INLINE
#define DEBUG_ONLY(x) x
#else
#if defined(_MSC_VER)
#define assert(expr,...) __assume((expr)!=0)
#define INLINE __forceinline
#elif defined(__GNUC__)
#define assert(expr,...) __builtin_expect(!(expr),0)
#define INLINE inline
#else
#error Unknown compiler
#endif
#define debug_assert(...) do{}while(0)
#define DEBUG_ONLY(x) do{}while(0)
#endif

const char* hexDump(const void* data, size_t size)
{
	char* buf = getTempString();
	enforce(size*3+1 < 1024);
	const uint8_t* s = (const uint8_t*)data;
	for (size_t x=0; x<size; x++)
		sprintf(buf+x*3, "%02X ", s[x]);
	return buf;
}

void printTime()
{
	time_t t;
	time(&t);
	char* tstr = ctime(&t);
	tstr[strlen(tstr)-1] = 0;
	printf("[%s] ", tstr);
}

#define DO_STRINGIZE(x) #x
#define STRINGIZE(x) DO_STRINGIZE(x)

// *********************************************** Types ************************************************

typedef int32_t FRAME;
typedef int32_t FRAME_GROUP;
#if (MAX_FRAMES<65536)
typedef int16_t PACKED_FRAME;
#else
typedef int32_t PACKED_FRAME;
#endif

// ********************************************** Problem ***********************************************

#include STRINGIZE(PROBLEM/PROBLEM.cpp)

// ************************************* CompressedState comparison *************************************

#ifndef COMPRESSED_BYTES
#define COMPRESSED_BYTES (((COMPRESSED_BITS) + 7) / 8)
#endif

// It is very important that these comparison operators are as fast as possible.
// TODO: relax ranges for !GROUP_FRAMES

#if   (!defined(USE_MEMCMP) && COMPRESSED_BITS >  24 && COMPRESSED_BITS <=  32) // 4 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint32_t*)&a)[0] == ((const uint32_t*)&b)[0]; }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint32_t*)&a)[0] != ((const uint32_t*)&b)[0]; }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint32_t*)&a)[0] <  ((const uint32_t*)&b)[0]; }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint32_t*)&a)[0] <= ((const uint32_t*)&b)[0]; }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  32 && COMPRESSED_BITS <=  40) // 5 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x000000FFFFFFFFFFLL) == (((const uint64_t*)&b)[0]&0x000000FFFFFFFFFFLL); }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x000000FFFFFFFFFFLL) != (((const uint64_t*)&b)[0]&0x000000FFFFFFFFFFLL); }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x000000FFFFFFFFFFLL) <  (((const uint64_t*)&b)[0]&0x000000FFFFFFFFFFLL); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x000000FFFFFFFFFFLL) <= (((const uint64_t*)&b)[0]&0x000000FFFFFFFFFFLL); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  40 && COMPRESSED_BITS <=  48) // 6 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x0000FFFFFFFFFFFFLL) == (((const uint64_t*)&b)[0]&0x0000FFFFFFFFFFFFLL); }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x0000FFFFFFFFFFFFLL) != (((const uint64_t*)&b)[0]&0x0000FFFFFFFFFFFFLL); }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x0000FFFFFFFFFFFFLL) <  (((const uint64_t*)&b)[0]&0x0000FFFFFFFFFFFFLL); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x0000FFFFFFFFFFFFLL) <= (((const uint64_t*)&b)[0]&0x0000FFFFFFFFFFFFLL); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  48 && COMPRESSED_BITS <=  56) // 7 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x00FFFFFFFFFFFFFFLL) == (((const uint64_t*)&b)[0]&0x00FFFFFFFFFFFFFFLL); }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x00FFFFFFFFFFFFFFLL) != (((const uint64_t*)&b)[0]&0x00FFFFFFFFFFFFFFLL); }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x00FFFFFFFFFFFFFFLL) <  (((const uint64_t*)&b)[0]&0x00FFFFFFFFFFFFFFLL); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return (((const uint64_t*)&a)[0]&0x00FFFFFFFFFFFFFFLL) <= (((const uint64_t*)&b)[0]&0x00FFFFFFFFFFFFFFLL); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  56 && COMPRESSED_BITS <=  64) // 8 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0]; }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0]; }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0]; }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <= ((const uint64_t*)&b)[0]; }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  64 && COMPRESSED_BITS <=  72) // 9 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint8_t*)&a)[8] == ((const uint8_t*)&b)[8]; }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || ((const uint8_t*)&a)[8] != ((const uint8_t*)&b)[8]; }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint8_t*)&a)[8] <  ((const uint8_t*)&b)[8]); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint8_t*)&a)[8] <= ((const uint8_t*)&b)[8]); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  72 && COMPRESSED_BITS <=  80) // 10 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint16_t*)&a)[4] == ((const uint16_t*)&b)[4]; }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || ((const uint16_t*)&a)[4] != ((const uint16_t*)&b)[4]; }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint16_t*)&a)[4] <  ((const uint16_t*)&b)[4]); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint16_t*)&a)[4] <= ((const uint16_t*)&b)[4]); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  80 && COMPRESSED_BITS <=  88) // 11 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint32_t*)&a)[2]&0x00FFFFFF) == (((const uint32_t*)&b)[2]&0x00FFFFFF); }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || (((const uint32_t*)&a)[2]&0x00FFFFFF) != (((const uint32_t*)&b)[2]&0x00FFFFFF); }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint32_t*)&a)[2]&0x00FFFFFF) <  (((const uint32_t*)&b)[2]&0x00FFFFFF)); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint32_t*)&a)[2]&0x00FFFFFF) <= (((const uint32_t*)&b)[2]&0x00FFFFFF)); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  88 && COMPRESSED_BITS <=  96) // 12 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint32_t*)&a)[2] == ((const uint32_t*)&b)[2]; }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || ((const uint32_t*)&a)[2] != ((const uint32_t*)&b)[2]; }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint32_t*)&a)[2] <  ((const uint32_t*)&b)[2]); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint32_t*)&a)[2] <= ((const uint32_t*)&b)[2]); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS >  96 && COMPRESSED_BITS <= 104) // 13 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x000000FFFFFFFFFFLL) == (((const uint64_t*)&b)[1]&0x000000FFFFFFFFFFLL); }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || (((const uint64_t*)&a)[1]&0x000000FFFFFFFFFFLL) != (((const uint64_t*)&b)[1]&0x000000FFFFFFFFFFLL); }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x000000FFFFFFFFFFLL) <  (((const uint64_t*)&b)[1]&0x000000FFFFFFFFFFLL)); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x000000FFFFFFFFFFLL) <= (((const uint64_t*)&b)[1]&0x000000FFFFFFFFFFLL)); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS > 104 && COMPRESSED_BITS <= 112) // 14 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x0000FFFFFFFFFFFFLL) == (((const uint64_t*)&b)[1]&0x0000FFFFFFFFFFFFLL); }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || (((const uint64_t*)&a)[1]&0x0000FFFFFFFFFFFFLL) != (((const uint64_t*)&b)[1]&0x0000FFFFFFFFFFFFLL); }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x0000FFFFFFFFFFFFLL) <  (((const uint64_t*)&b)[1]&0x0000FFFFFFFFFFFFLL)); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x0000FFFFFFFFFFFFLL) <= (((const uint64_t*)&b)[1]&0x0000FFFFFFFFFFFFLL)); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS > 112 && COMPRESSED_BITS <= 120) // 15 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x00FFFFFFFFFFFFFFLL) == (((const uint64_t*)&b)[1]&0x00FFFFFFFFFFFFFFLL); }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || (((const uint64_t*)&a)[1]&0x00FFFFFFFFFFFFFFLL) != (((const uint64_t*)&b)[1]&0x00FFFFFFFFFFFFFFLL); }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x00FFFFFFFFFFFFFFLL) <  (((const uint64_t*)&b)[1]&0x00FFFFFFFFFFFFFFLL)); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && (((const uint64_t*)&a)[1]&0x00FFFFFFFFFFFFFFLL) <= (((const uint64_t*)&b)[1]&0x00FFFFFFFFFFFFFFLL)); }
#elif (!defined(USE_MEMCMP) && COMPRESSED_BITS > 120 && COMPRESSED_BITS <= 128) // 16 bytes
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint64_t*)&a)[1] == ((const uint64_t*)&b)[1]; }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] != ((const uint64_t*)&b)[0] || ((const uint64_t*)&a)[1] != ((const uint64_t*)&b)[1]; }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint64_t*)&a)[1] <  ((const uint64_t*)&b)[1]); }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return ((const uint64_t*)&a)[0] <  ((const uint64_t*)&b)[0] || 
                                                                                   (((const uint64_t*)&a)[0] == ((const uint64_t*)&b)[0] && ((const uint64_t*)&a)[1] <= ((const uint64_t*)&b)[1]); }
#else
#pragma message("Performance warning: using memcmp for CompressedState comparison")
#define SLOW_COMPARE
INLINE bool operator==(const CompressedState& a, const CompressedState& b) { return memcmp(&a, &b, COMPRESSED_BYTES)==0; }
INLINE bool operator!=(const CompressedState& a, const CompressedState& b) { return memcmp(&a, &b, COMPRESSED_BYTES)!=0; }
INLINE bool operator< (const CompressedState& a, const CompressedState& b) { return memcmp(&a, &b, COMPRESSED_BYTES)< 0; }
INLINE bool operator<=(const CompressedState& a, const CompressedState& b) { return memcmp(&a, &b, COMPRESSED_BYTES)<=0; }
#endif

INLINE bool operator> (const CompressedState& a, const CompressedState& b) { return b< a; }
INLINE bool operator>=(const CompressedState& a, const CompressedState& b) { return b<=a; }

// ******************************************** Frame groups ********************************************

#ifdef GROUP_FRAMES

#define GET_FRAME(frameGroup, cs) ((frameGroup) * FRAMES_PER_GROUP + (cs).subframe)
#define SET_SUBFRAME(cs, frame) (cs).subframe = (frame) % FRAMES_PER_GROUP

#define GROUP_STR "-group"
#if (FRAMES_PER_GROUP == 10)
#define GROUP_FORMAT "%ux"
#else
#define GROUP_FORMAT "g%u"
#endif

#else // GROUP_FRAMES

#define FRAMES_PER_GROUP 1
#define GET_FRAME(frameGroup, cs) (frameGroup)
#define SET_SUBFRAME(cs, frame)
#define GROUP_STR ""
#define GROUP_FORMAT "%u"

#endif

// ************************************************ Disk ************************************************

typedef CompressedState Node;

#if defined(DISK_WINFILES)
#include "disk_file_windows.cpp"
#elif defined(DISK_POSIX)
#include "disk_file_posix.cpp"
#else
#error Disk plugin not set
#endif

// *********************************************** Memory ***********************************************

// Allocate RAM at start, use it for different purposes depending on what we're doing
// Even if we won't use all of it, most OSes shouldn't reserve physical RAM for the entire amount
void* ram = malloc(RAM_SIZE);

struct CacheNode
{
	CompressedState state;
	PACKED_FRAME frame;
};

const size_t CACHE_HASH_SIZE = RAM_SIZE / sizeof(CacheNode) / NODES_PER_HASH;
CacheNode (*cache)[NODES_PER_HASH] = (CacheNode (*)[NODES_PER_HASH]) ram;

const size_t BUFFER_SIZE = RAM_SIZE / sizeof(Node);
Node* buffer = (Node*) ram;

// ****************************************** Buffered streams ******************************************

#ifndef STANDARD_BUFFER_SIZE
#define STANDARD_BUFFER_SIZE (1024*1024 / sizeof(Node)) // 1 MB
#endif
#ifndef ALL_FILE_BUFFER_SIZE
#define ALL_FILE_BUFFER_SIZE (1024*1024 / sizeof(Node)) // 1 MB
#endif

class Buffer
{
public:
	Node* buf;
	uint32_t size;

	Buffer(uint32_t size) : size(size), buf(NULL)
	{
	}

	void allocate()
	{
		if (!buf)
			buf = new Node[size];
	}

	// When we need to use the default constructor (e.g. arrays)
	void setSize(uint32_t newSize)
	{
		assert(!buf, "Trying to set the buffer size after the buffer was allocated");
		size = newSize;
	}

	~Buffer()
	{
		delete[] buf;
	}
};

template<class STREAM>
class BufferedStreamBase
{
protected:
	STREAM s;

public:
	uint64_t size() { return s.size(); }
	bool isOpen() { return s.isOpen(); }
	void close() { return s.close(); }
};

template<class STREAM>
class WriteBuffer : virtual public BufferedStreamBase<STREAM>
{
	uint32_t pos;
protected:
	Buffer buffer;
public:
	WriteBuffer(uint32_t size) : buffer(size), pos(0) {}

	void write(const Node* p, bool verify=false)
	{
		buffer.buf[pos++] = *p;
#ifdef DEBUG
		if (verify && pos > 1)
			assert(buffer.buf[pos-1] > buffer.buf[pos-2], "Output is not sorted");
#endif
		if (pos == buffer.size)
			flushBuffer();
	}

	uint64_t size()
	{
		return s.size() + pos;
	}

	void flushBuffer()
	{
		if (pos)
		{
			s.write(buffer.buf, pos);
			pos = 0;
		}
	}

	void flush()
	{
		flushBuffer();
#ifndef NO_DISK_FLUSH
		s.flush();
#endif
	}

	void close()
	{
		flushBuffer();
		BufferedStreamBase<STREAM>::close();
	}

	~WriteBuffer()
	{
		flushBuffer();
	}
};

template<class STREAM>
class ReadBuffer : virtual public BufferedStreamBase<STREAM>
{
	uint32_t pos, end;
protected:
	Buffer buffer;
public:
	ReadBuffer(uint32_t size) : buffer(size), pos(0), end(0) {}

	const Node* read()
	{
		if (pos == end)
		{
			fillBuffer();
			if (pos == end)
				return NULL;
		}
#ifdef DEBUG
		if (pos > 0) 
			assert(buffer.buf[pos-1] < buffer.buf[pos], "Input is not sorted");
#endif
		return &buffer.buf[pos++];
	}

	void fillBuffer()
	{
		pos = 0;
		uint64_t left = s.size() - s.position();
		end = (uint32_t)s.read(buffer.buf, (size_t)(left < buffer.size ? left : buffer.size));
	}
};

class BufferedInputStream : public ReadBuffer<InputStream>
{
public:
	BufferedInputStream(uint32_t size = STANDARD_BUFFER_SIZE) : ReadBuffer(size) {}
	BufferedInputStream(const char* filename, uint32_t size = STANDARD_BUFFER_SIZE) : ReadBuffer(size) { open(filename); }
	void open(const char* filename) { s.open(filename); buffer.allocate(); }
	void setBufferSize(uint32_t size) { buffer.setSize(size); }
};

class BufferedOutputStream : public WriteBuffer<OutputStream>
{
public:
	BufferedOutputStream(uint32_t size = STANDARD_BUFFER_SIZE) : WriteBuffer(size) {}
	BufferedOutputStream(const char* filename, bool resume=false, uint32_t size = STANDARD_BUFFER_SIZE) : WriteBuffer(size) { open(filename, resume); }
	void open(const char* filename, bool resume=false) { s.open(filename, resume); buffer.allocate(); }
	void setBufferSize(uint32_t size) { buffer.setSize(size); }
};

class BufferedRewriteStream : public ReadBuffer<RewriteStream>, public WriteBuffer<RewriteStream>
{
public:
	BufferedRewriteStream(uint32_t size = STANDARD_BUFFER_SIZE) : ReadBuffer(size), WriteBuffer(size) {}
	BufferedRewriteStream(const char* filename, uint32_t size = STANDARD_BUFFER_SIZE) : ReadBuffer(size), WriteBuffer(size) { open(filename); }
	void open(const char* filename) { s.open(filename); ReadBuffer<RewriteStream>::buffer.allocate(); WriteBuffer<RewriteStream>::buffer.allocate(); }
	void truncate() { s.truncate(); }
	void setBufferSize(uint32_t size) { ReadBuffer<RewriteStream>::buffer.setSize(size); WriteBuffer<RewriteStream>::buffer.setSize(size); }
};

void copyFile(const char* from, const char* to)
{
	InputStream input(from);
	OutputStream output(to);
	uint64_t amount = input.size();
	if (amount > BUFFER_SIZE)
		amount = BUFFER_SIZE;
	size_t records;
	while (records = input.read(buffer, (size_t)amount))
		output.write(buffer, records);
	output.flush(); // force disk flush
}

// ***************************************** Stream operations ******************************************

template<class INPUT>
class InputHeap
{
	struct HeapNode
	{
		const CompressedState* state;
		INPUT* input;
		INLINE bool operator<(const HeapNode& b) const { return *this->state < *b.state; }
	};

	HeapNode *heap, *head;
	int size;

public:
	InputHeap(INPUT inputs[], int count)
	{
		if (count==0)
			error("No inputs");
		heap = new HeapNode[count];
		size = 0;
		for (int i=0; i<count; i++)
		{
			if (inputs[i].isOpen())
			{
				heap[size].input = &inputs[i];
				heap[size].state = inputs[i].read();
				if (heap[size].state)
					size++;
			}
		}
		std::sort(heap, heap+size);
		head = heap;
		heap--; // heap[0] is now invalid, use heap[1] to heap[size] inclusively; head == heap[1]
		if (size==0 && count>0)
			head->state = NULL;
		test();
	}

	~InputHeap()
	{
		heap++;
		delete[] heap;
	}

	const CompressedState* getHead() const { return head->state; }
	INPUT* getHeadInput() const { return head->input; }

	bool next()
	{
		test();
		if (size == 0)
			return false;
		head->state = head->input->read();
		if (head->state == NULL)
		{
			*head = heap[size];
			size--;
			if (size==0)
				return false;
		}
		bubbleDown();
		test();
		return true;
	}

	bool scanTo(const CompressedState& target)
	{
		test();
		if (size == 0)
			return false;
		if (*head->state >= target) // TODO: this check is not always needed
			return true;
		if (size>1)
		{
			do
			{
				CompressedState readUntil = target;
				const CompressedState* minChild = heap[2].state;
				if (size>2 && *minChild > *heap[3].state)
					minChild = heap[3].state;
				if (readUntil > *minChild)
					readUntil = *minChild;
				
				do
					head->state = head->input->read();
				while (head->state && *head->state < readUntil);

				if (head->state == NULL)
				{
					*head = heap[size];
					size--;
				}
				else
					if (*head->state <= *minChild)
						continue;
				bubbleDown();
				test();
				if (size==1)
					if (*head->state < target)
						goto size1;
					else
						return true;
			} while (*head->state < target);
		}
		else
		{
		size1:
			do
				head->state = head->input->read();
			while (head->state && *head->state < target);
			if (head->state == NULL)
			{
				size = 0;
				return false;
			}
		}
		test();
		return true;
	}

	void bubbleDown()
	{
		// Force local variables
		intptr_t c = 1;
		intptr_t size = this->size;
		HeapNode* heap = this->heap;
		HeapNode* pp = head; // pointer to parent
		while (1)
		{
			c = c*2;
			if (c > size)
				return;
			HeapNode* pc = &heap[c];
			if (c < size) // if (c+1 <= size)
			{
				HeapNode* pc2 = pc+1;
				if (*pc2->state < *pc->state)
				{
					pc = pc2;
					c++;
				}
			}
			if (*pp->state <= *pc->state)
				return;
			HeapNode t = *pp;
			*pp = *pc;
			*pc = t;
			pp = pc;
		}
	}

	void test() const
	{
#ifdef DEBUG
		for (int p=1; p<size; p++)
		{
			assert(p*2   > size || *heap[p].state <= *heap[p*2  ].state);
			assert(p*2+1 > size || *heap[p].state <= *heap[p*2+1].state);
		}
#endif
	}
};

// BufferedInputStream-compatible wrapper for InputHeap
template<class INPUT>
class InputHeapReader : InputHeap<INPUT>
{
private:
	bool first;

public:
	InputHeapReader(INPUT inputs[], int count) : InputHeap(inputs, count), first(true) {}

	const CompressedState* read()
	{
		if (!first)
		{
			if (!next())
				return NULL;
		}
		else
			first = false;
		return getHead();
	}
};

void mergeStreams(BufferedInputStream inputs[], int inputCount, BufferedOutputStream* output)
{
	InputHeap<BufferedInputStream> heap(inputs, inputCount);

	const CompressedState* first = heap.getHead();
	if (!first)
		return;
	CompressedState cs = *first;
	
	while (heap.next())
	{
		CompressedState cs2 = *heap.getHead();
		debug_assert(cs2 >= cs);
		if (cs == cs2) // CompressedState::operator== does not compare subframe
		{
#ifdef GROUP_FRAMES
			if (cs.subframe > cs2.subframe) // in case of duplicate frames, pick the one from the smallest frame
				cs.subframe = cs2.subframe;
#endif
		}
		else
		{
			output->write(&cs, true);
			cs = cs2;
		}
	}
	output->write(&cs, true);
}

template <class FILTERED_NODE_HANDLER>
void filterStream(BufferedInputStream* source, BufferedInputStream inputs[], int inputCount, BufferedOutputStream* output)
{
	const CompressedState* sourceState = source->read();
	if (inputCount == 0)
	{
		while (sourceState)
		{
			output->write(sourceState, true);
			FILTERED_NODE_HANDLER::handle(sourceState);
			sourceState = source->read();
		}
		return;
	}
	
	InputHeap<BufferedInputStream> heap(inputs, inputCount);

	while (sourceState)
	{
		bool b = heap.scanTo(*sourceState);
		if (!b) // EOF of heap sources
		{
			do {
				output->write(sourceState, true);
				FILTERED_NODE_HANDLER::handle(sourceState);
				sourceState = source->read();
			} while (sourceState);
			return;
		}
		const CompressedState* head = heap.getHead();
		assert(sourceState);
		while (sourceState && *sourceState < *head)
		{
			output->write(sourceState, true);
			FILTERED_NODE_HANDLER::handle(sourceState);
			sourceState = source->read();
		}
		while (sourceState && *sourceState == *head)
			sourceState = source->read();
	}
}

template <class FILTERED_NODE_HANDLER, class INPUT2>
void mergeTwoStreams(BufferedInputStream* input1, INPUT2* input2, BufferedOutputStream* output, BufferedOutputStream* output1)
{
	// output <= merged
	// output1 <= only in input1
	
	const CompressedState* states[2];
	states[0] = input1->read();
	states[1] = input2->read();
	
	int c;
	while (*states[0] == *states[1])
	{
		output->write(states[0]);
		states[0] = input1->read();
		states[1] = input2->read();
		if (states[0] == NULL) { c = 0; goto eof; }
		if (states[1] == NULL) { c = 1; goto eof; }
	}

	c = *states[0] < *states[1] ? 0 : 1;
	while (true)
	{
		const CompressedState* cc = states[c];
		const CompressedState* co = states[c^1];
		debug_assert(*cc < *co);
		do
		{
			output->write(cc, true);
			if (c==0)
			{
				output1->write(cc, true);
				FILTERED_NODE_HANDLER::handle(cc);
				cc = input1->read();
			}
			else
				cc = input2->read();
			if (cc==NULL)
				goto eof;
		} while (*cc < *co);
		if (*cc == *co)
		{
			states[0] = cc;
			do 
			{
				output->write(states[0]);
				states[0] = input1->read();
				states[1] = input2->read();
				if (states[0] == NULL) { c = 0; goto eof; }
				if (states[1] == NULL) { c = 1; goto eof; }
			} while (*states[0] == *states[1]);
			c = *states[0] < *states[1] ? 0 : 1;
		}
		else
		{
			states[c] = cc;
			c ^= 1;
			states[c] = co;
		}
	}
eof:
	c ^= 1;
	const CompressedState* cc = states[c];
	while (cc)
	{
		output->write(cc, true);
		if (c==0)
		{
			output1->write(cc, true);
			FILTERED_NODE_HANDLER::handle(cc);
			cc = input1->read();
		}
		else
			cc = input2->read();
	}
}

// In-place deduplicate sorted nodes in memory. Return new number of nodes.
size_t deduplicate(CompressedState* start, size_t records)
{
	if (records==0)
		return 0;
	CompressedState *read=start+1, *write=start+1;
	CompressedState *end = start+records;
	while (read < end)
	{
		debug_assert(*read >= *(read-1));
		if (*read == *(write-1)) // CompressedState::operator== does not compare subframe
		{
#ifdef GROUP_FRAMES
			if ((write-1)->subframe > read->subframe)
				(write-1)->subframe = read->subframe;
#endif
		}
		else
		{
			*write = *read;
            write++;
		}
        read++;
	}
	return write-start;
}

// ********************************************* File names *********************************************

const char* formatFileName(const char* name)
{
	return formatProblemFileName(name, NULL, "bin");
}

const char* formatFileName(const char* name, FRAME_GROUP g)
{
	return formatProblemFileName(name, format(GROUP_FORMAT, g), "bin");
}

const char* formatFileName(const char* name, FRAME_GROUP g, unsigned chunk)
{
	return formatProblemFileName(name, format(GROUP_FORMAT "-%u", g, chunk), "bin");
}

// ************************************** Disk queue (open nodes) ***************************************

#define MAX_FRAME_GROUPS ((MAX_FRAMES+(FRAMES_PER_GROUP-1))/FRAMES_PER_GROUP)

BufferedOutputStream* queue[MAX_FRAME_GROUPS];
bool noQueue[MAX_FRAME_GROUPS];
#ifdef MULTITHREADING
MUTEX queueMutex[MAX_FRAME_GROUPS];
#endif

#ifdef MULTITHREADING
#define PARTITIONS (CACHE_HASH_SIZE/256)
MUTEX cacheMutex[PARTITIONS];
#endif

void writeOpenState(CompressedState* state, FRAME frame)
{
	FRAME_GROUP group = frame/FRAMES_PER_GROUP;
	if (group >= MAX_FRAME_GROUPS)
		return;
	if (noQueue[group])
		return;
	SET_SUBFRAME(*state, frame);
#ifdef MULTITHREADING
	SCOPED_LOCK lock(queueMutex[group]);
#endif
	if (!queue[group])
		queue[group] = new BufferedOutputStream(formatFileName("open", group));
	queue[group]->write(state);
}

void flushOpen()
{
	for (FRAME_GROUP g=0; g<MAX_FRAME_GROUPS; g++)
		if (queue[g])
			queue[g]->flush();
}

// *********************************************** Cache ************************************************

INLINE uint32_t hashState(const CompressedState* state)
{
	// Based on MurmurHash ( http://murmurhash.googlepages.com/MurmurHash2.cpp )
	
	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	unsigned int h = sizeof(CompressedState);

	const unsigned char* data = (const unsigned char *)state;

	for (int i=0; i<sizeof(CompressedState)/4; i++) // should unroll
	{
		unsigned int k = *(unsigned int *)data;

		k *= m; 
		k ^= k >> r; 
		k *= m; 
		
		h *= m; 
		h ^= k;

		data += 4;
	}
	
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}

void addState(const State* state, FRAME frame)
{
	CompressedState cs;
	state->compress(&cs);
#ifdef DEBUG
	State test;
	test.decompress(&cs);
	if (!(test == *state))
	{
		puts("");
		puts(hexDump(state, sizeof(State)));
		puts(state->toString());
		puts(hexDump(&cs, sizeof(CompressedState)));
		puts(cs.toString());
		puts(hexDump(&test, sizeof(State)));
		puts(test.toString());
		error("Compression/decompression failed");
	}
#endif
	uint32_t hash = hashState(&cs) % CACHE_HASH_SIZE;
	
	{
#ifdef MULTITHREADING
		SCOPED_LOCK lock(cacheMutex[hash % PARTITIONS]);
#endif
		CacheNode* nodes = cache[hash];
		for (int i=0; i<NODES_PER_HASH; i++)
			if (nodes[i].state == cs)
			{
				if (nodes[i].frame > frame)
					writeOpenState(&cs, frame);
				// pop to front
				if (i>0)
				{
					memmove(nodes+1, nodes, i * sizeof(CacheNode));
					nodes[0].state = cs;
				}
				nodes[0].frame = (PACKED_FRAME)frame;
				return;
			}
		
		// new node
		memmove(nodes+1, nodes, (NODES_PER_HASH-1) * sizeof(CacheNode));
		nodes[0].frame = (PACKED_FRAME)frame;
		nodes[0].state = cs;
	}
	writeOpenState(&cs, frame);
}

// ****************************************** Processing queue ******************************************

#ifdef MULTITHREADING

#define WORKERS (THREADS-1)
#define PROCESS_QUEUE_SIZE 0x100000
CompressedState processQueue[PROCESS_QUEUE_SIZE]; // circular buffer
size_t processQueueHead=0, processQueueTail=0;
MUTEX processQueueMutex; // for head/tail
CONDITION processQueueReadCondition, processQueueWriteCondition, processQueueExitCondition;
int runningWorkers = 0;
bool stopWorkers = false;

void queueState(const CompressedState* state)
{
	SCOPED_LOCK lock(processQueueMutex);

	while (processQueueHead == processQueueTail+PROCESS_QUEUE_SIZE) // while full
		CONDITION_WAIT(processQueueReadCondition, lock);
	processQueue[processQueueHead++ % PROCESS_QUEUE_SIZE] = *state;
	CONDITION_NOTIFY(processQueueWriteCondition, lock);
}

bool dequeueState(CompressedState* state)
{
	SCOPED_LOCK lock(processQueueMutex);
		
	while (processQueueHead == processQueueTail) // while empty
	{
		if (stopWorkers)
			return false;
		CONDITION_WAIT(processQueueWriteCondition, lock);
	}
	*state = processQueue[processQueueTail++ % PROCESS_QUEUE_SIZE];
	CONDITION_NOTIFY(processQueueReadCondition, lock);
	return true;
}

template<void (*STATE_HANDLER)(const CompressedState*)>
void worker()
{
	CompressedState cs;
	while (dequeueState(&cs))
		STATE_HANDLER(&cs);
	
    /* LOCK */
	{
		SCOPED_LOCK lock(processQueueMutex);
		runningWorkers--;
		CONDITION_NOTIFY(processQueueExitCondition, lock);
	}
}

template<void (*STATE_HANDLER)(const CompressedState*)>
void startWorkers()
{
	runningWorkers = WORKERS;
	void (*myWorker)() = &worker<STATE_HANDLER>;
	for (int i=0; i<WORKERS; i++)
		THREAD_CREATE(myWorker);
}

void flushProcessingQueue()
{
	SCOPED_LOCK lock(processQueueMutex);

	stopWorkers = true;
	CONDITION_NOTIFY(processQueueWriteCondition, lock);

	while (runningWorkers)
		CONDITION_WAIT(processQueueExitCondition, lock);
	
	stopWorkers = false;
}

#endif

// ******************************************** Exit tracing ********************************************

State exitSearchState     , exitSearchStateParent     ;
FRAME exitSearchStateFrame, exitSearchStateParentFrame;
Step exitSearchStateStep;
bool exitSearchStateFound = false;
int exitSearchFrameGroup; // allow negative
#ifdef MULTITHREADING
MUTEX exitSearchStateMutex;
#endif
#ifdef DEBUG
int statesQueued, statesDequeued;
#endif

class FinishCheckChildHandler
{
public:
	static INLINE void handleChild(const State* parent, FRAME parentFrame, Step step, const State* state, FRAME frame)
	{
		if (*state==exitSearchState && frame==exitSearchStateFrame)
		{
#ifdef MULTITHREADING
			SCOPED_LOCK lock(exitSearchStateMutex);
#endif
			exitSearchStateFound       = true;
			exitSearchStateStep        = step;
			exitSearchStateParent      = *parent;
			exitSearchStateParentFrame = parentFrame;
		}
	}
};

INLINE void processExitState(const CompressedState* cs)
{
	State state;
	state.decompress(cs);
	FRAME frame = GET_FRAME(exitSearchFrameGroup, *cs);
	expandChildren<FinishCheckChildHandler>(frame, &state);

#ifdef DEBUG
#ifdef MULTITHREADING
	static MUTEX mutex;
	SCOPED_LOCK lock(mutex);
#endif
	statesDequeued++;
#endif
}

void traceExit(const State* exitState, FRAME exitFrame)
{
	Step steps[MAX_STEPS];
	int stepNr = 0;
	
	if (fileExists(formatFileName("solution")))
	{
		printf("Resuming exit trace...\n");
		FILE* f = fopen(formatFileName("solution"), "rb");
		fread(&exitSearchFrameGroup, sizeof(exitSearchFrameGroup), 1, f);
		fread(&exitSearchState     , sizeof(exitSearchState)     , 1, f);
		fread(&stepNr              , sizeof(stepNr)              , 1, f);
		fread(steps, sizeof(Step), stepNr, f);
		fclose(f);
	}
	else
	if (exitState)
	{
		exitSearchState      = *exitState;
		exitSearchStateFrame =  exitFrame;
		exitSearchFrameGroup =  exitFrame / FRAMES_PER_GROUP;
	}
	else
		error("Can't resume exit tracing - partial trace solution file not found");
	
	while (exitSearchFrameGroup >= 0)
	{
		// save trace-exit progress
		{
			FILE* f = fopen(formatFileName("solution"), "wb");
			fwrite(&exitSearchFrameGroup, sizeof(exitSearchFrameGroup), 1, f);
			fwrite(&exitSearchState     , sizeof(exitSearchState)     , 1, f);
			fwrite(&stepNr              , sizeof(stepNr)              , 1, f);
			fwrite(steps, sizeof(Step), stepNr, f);
			fclose(f);
		}

		exitSearchStateFound = false;
		exitSearchFrameGroup--;
		if (fileExists(formatFileName("closed", exitSearchFrameGroup)))
		{
			printf("Frame" GROUP_STR " " GROUP_FORMAT "... \r", exitSearchFrameGroup);
				
#ifdef MULTITHREADING
			startWorkers<&processExitState>();
#endif

			BufferedInputStream input(formatFileName("closed", exitSearchFrameGroup));
			const CompressedState *cs;
			DEBUG_ONLY(statesQueued = statesDequeued = 0);
			while (cs = input.read())
			{
				DEBUG_ONLY(statesQueued++);
#ifdef MULTITHREADING
				queueState(cs);
#else
				processExitState(cs);
#endif
				if (exitSearchStateFound)
					break;
			}
			
#ifdef MULTITHREADING
			flushProcessingQueue();
#endif
			debug_assert(statesQueued == statesDequeued, format("Queued %d states but dequeued only %d!", statesQueued, statesDequeued));

			if (exitSearchStateFound)
			{
				printTime(); printf("Found (at %d)!          \n", exitSearchStateParentFrame);
				steps[stepNr++]      = exitSearchStateStep;
				exitSearchState      = exitSearchStateParent;
				exitSearchStateFrame = exitSearchStateParentFrame;
				if (exitSearchFrameGroup == 0)
					goto found;
			}
		}
	}
	error("Lost parent node!");
found:
    
	writeSolution(&exitSearchState, steps, stepNr);
    deleteFile(formatFileName("solution"));
}

// **************************************** Common runmode code *****************************************

size_t ramUsed;

void sortAndMerge(FRAME_GROUP g)
{
	// Step 1: read chunks of BUFFER_SIZE nodes, sort+dedup them in RAM and write them to disk
	int chunks = 0;
	ramUsed = 0;
	printf("Sorting... "); fflush(stdout);
	{
		InputStream input(formatFileName("open", g));
		uint64_t amount = input.size();
		if (amount > BUFFER_SIZE)
			amount = BUFFER_SIZE;
		size_t records;
		while (records = input.read(buffer, (size_t)amount))
		{
			if (ramUsed < records * sizeof(Node))
				ramUsed = records * sizeof(Node);
			std::sort(buffer, buffer + records);
			records = deduplicate(buffer, records);
			OutputStream output(formatFileName("chunk", g, chunks));
			output.write(buffer, records);
			chunks++;
		}
	}

	// Step 2: merge + dedup chunks
	printf("Merging... "); fflush(stdout);
	if (chunks>1)
	{
		BufferedInputStream* chunkInput = new BufferedInputStream[chunks];
		for (int i=0; i<chunks; i++)
		{
			chunkInput[i].setBufferSize(MERGING_BUFFER_SIZE);
			chunkInput[i].open(formatFileName("chunk", g, i));
		}
		BufferedOutputStream* output = new BufferedOutputStream(formatFileName("merging", g));
		mergeStreams(chunkInput, chunks, output);
		delete[] chunkInput;
		output->flush();
		delete output;
		renameFile(formatFileName("merging", g), formatFileName("merged", g));
		for (int i=0; i<chunks; i++)
			deleteFile(formatFileName("chunk", g, i));
	}
	else
	{
		renameFile(formatFileName("chunk", g, 0), formatFileName("merged", g));
	}
}

bool checkStop()
{
	if (fileExists(formatProblemFileName("stop", NULL, "txt")))
	{
		deleteFile(formatProblemFileName("stop", NULL, "txt"));
		printTime(); printf("Stop file found.\n");
		return true;
	}
	return false;
}

FRAME_GROUP lastAll()
{
	for (int g=MAX_FRAME_GROUPS-1; g>=0; g--)
		if (fileExists(formatFileName("all", g)))
			return g;
	error("All file not found!");
	return 0; // compiler complains
}

enum // exit reasons
{
	EXIT_OK,
	EXIT_STOP,
	EXIT_NOTFOUND,
	EXIT_ERROR
};

// *********************************************** Search ***********************************************

FRAME_GROUP firstFrameGroup, maxFrameGroups;
FRAME_GROUP currentFrameGroup;

bool exitFound;
FRAME exitFrame;
State exitState;
#ifdef MULTITHREADING
MUTEX finishMutex;
#endif

INLINE bool finishCheck(const State* s, FRAME frame)
{
	if (s->isFinish())
	{
#ifdef MULTITHREADING
		SCOPED_LOCK lock(finishMutex);
#endif
		if (exitFound)
		{
			if (exitFrame > frame)
			{
				exitFrame = frame;
				exitState = *s;
			}
		}
		else
		{
			exitFound = true;
			exitFrame = frame;
			exitState = *s;
		}
		return true;
	}
	return false;
}

void processState(const CompressedState* cs)
{
	State s;
	s.decompress(cs);
#ifdef DEBUG
	CompressedState test;
	s.compress(&test);
	if (test != *cs)
	{
		puts("");
		puts(hexDump(cs, sizeof(CompressedState)));
		puts(cs->toString());
		puts(hexDump(&s, sizeof(State)));
		puts(s.toString());
		puts(hexDump(&test, sizeof(CompressedState)));
		puts(test.toString());
		error("Compression/decompression failed");
	}
#endif
	FRAME currentFrame = GET_FRAME(currentFrameGroup, *cs);
	if (finishCheck(&s, currentFrame))
		return;

	class AddStateChildHandler
	{
	public:
		static INLINE void handleChild(const State* parent, FRAME parentFrame, Step step, const State* state, FRAME frame)
		{
			addState(state, frame);
		}
	};

	expandChildren<AddStateChildHandler>(currentFrame, &s);
	assert(currentFrame/FRAMES_PER_GROUP == currentFrameGroup, format("Run-away currentFrameGroup: currentFrame=%u, currentFrameGroup=%u", currentFrame, currentFrameGroup));
}

INLINE void processFilteredState(const CompressedState* state)
{
#ifdef MULTITHREADING
	queueState(state);
#else
	processState(state);
#endif
}

class ProcessStateHandler
{
public:
	INLINE static void handle(const CompressedState* state)
	{
		processFilteredState(state);
	}
};

int search()
{
	firstFrameGroup = 0;

	if (fileExists(formatFileName("solution")))
	{
		printf("Partial trace solution file present, resuming exit trace...\n");
		traceExit(NULL, 0);
		return EXIT_OK;
	}

	for (FRAME_GROUP g=MAX_FRAME_GROUPS; g>0; g--)
		if (fileExists(formatFileName("closed", g)))
		{
			printf("Resuming from frame" GROUP_STR " " GROUP_FORMAT "\n", g+1);
			firstFrameGroup = g+1;
			break;
	    }

	for (FRAME_GROUP g=firstFrameGroup; g<MAX_FRAME_GROUPS; g++)
		if (fileExists(formatFileName("open", g)))
		{
			printTime(); printf("Reopening queue for frame" GROUP_STR " " GROUP_FORMAT "\n", g);
			queue[g] = new BufferedOutputStream(formatFileName("open", g), true);
		}

	if (firstFrameGroup==0 && !queue[0])
	{
		for (int i=0; i<initialStateCount; i++)
		{
			State s = initialStates[i];
			CompressedState c;
			s.compress(&c);
			writeOpenState(&c, 0);
		}
	}

	for (currentFrameGroup=firstFrameGroup; currentFrameGroup<maxFrameGroups; currentFrameGroup++)
	{
		if (!queue[currentFrameGroup])
			continue;
		delete queue[currentFrameGroup];
		queue[currentFrameGroup] = NULL;

		printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT "/" GROUP_FORMAT ": ", currentFrameGroup, maxFrameGroups); fflush(stdout);

		if (fileExists(formatFileName("merged", currentFrameGroup)))
		{
			printf("(reopening merged)    ");
		}
		else
		{
			// Step 1 & 2: sort and merge open nodes
			sortAndMerge(currentFrameGroup);
		}

		printf("Clearing... "); fflush(stdout);
		memset(ram, 0, ramUsed); // clear cache

		// Step 3: dedup against previous frames, while simultaneously processing filtered nodes
		printf("Processing... "); fflush(stdout);

#ifdef MULTITHREADING
		startWorkers<&processState>();
#endif

#ifdef USE_ALL
		if (currentFrameGroup==0)
		{
			copyFile(formatFileName("merged", currentFrameGroup), formatFileName("closing", currentFrameGroup));
			renameFile(formatFileName("merged", currentFrameGroup), formatFileName("allnew", currentFrameGroup));
			
			BufferedInputStream input(formatFileName("closing", currentFrameGroup));
			const CompressedState* cs;
			while (cs = input.read())
				processFilteredState(cs);
		}
		else
		{
			{
				BufferedInputStream source(formatFileName("merged", currentFrameGroup));
				FRAME_GROUP allFrameGroup = lastAll();
				BufferedInputStream all(formatFileName("all", allFrameGroup), ALL_FILE_BUFFER_SIZE);
				BufferedOutputStream allnew(formatFileName("allnew", currentFrameGroup), false, ALL_FILE_BUFFER_SIZE);
				BufferedOutputStream closing(formatFileName("closing", currentFrameGroup));
				
				BufferedInputStream* inputs = new BufferedInputStream[MAX_FRAME_GROUPS+1];
				int additionalInputs = 0;
				for (FRAME_GROUP g=allFrameGroup+1; g<currentFrameGroup; g++)
					if (fileExists(formatFileName("closed", g)))
					{
						inputs[g].open(formatFileName("closed", g));
						if (inputs[g].size())
							additionalInputs++;
						else
							inputs[g].close();
					}
				if (additionalInputs==0)
					mergeTwoStreams<ProcessStateHandler>(&source, &all , &allnew, &closing);
				else
				{
					inputs[MAX_FRAME_GROUPS] = all;
					InputHeapReader<BufferedInputStream> heap(inputs, MAX_FRAME_GROUPS+1);
					mergeTwoStreams<ProcessStateHandler>(&source, &heap, &allnew, &closing);
				}
				delete[] inputs;
				allnew.flush();
				closing.flush();
			}
			deleteFile(formatFileName("merged", currentFrameGroup));
		}
#else
		{
			BufferedInputStream* source = new BufferedInputStream(formatFileName("merged", currentFrameGroup));
			BufferedInputStream* inputs = new BufferedInputStream[MAX_FRAME_GROUPS];
			for (FRAME_GROUP g=0; g<currentFrameGroup; g++)
				if (fileExists(formatFileName("closed", g)))
					inputs[g].open(formatFileName("closed", g));
			if (fileExists(formatFileName("closing", currentFrameGroup)))
			{
				//printf("Overwriting %s\n", formatFileName("closing", currentFrameGroup));
				deleteFile(formatFileName("closing", currentFrameGroup));
			}
			BufferedOutputStream* output = new BufferedOutputStream(formatFileName("closing", currentFrameGroup));
			filterStream<ProcessStateHandler>(source, inputs, MAX_FRAME_GROUPS, output);
			delete source;
			output->flush(); // force disk flush
			delete output;
			delete[] inputs;
			deleteFile(formatFileName("merged", currentFrameGroup));
		}
#endif

#ifdef MULTITHREADING
		flushProcessingQueue();
#endif

		printf("Flushing... "); fflush(stdout);
		flushOpen();

		if (exitFound)
		{
			assert(currentFrameGroup == exitFrame / FRAMES_PER_GROUP);
			printf("\nExit found (at frame %u), tracing path...\n", exitFrame);
			traceExit(&exitState, exitFrame);
			return EXIT_OK;
		}

		deleteFile(formatFileName("open", currentFrameGroup));
		renameFile(formatFileName("closing", currentFrameGroup), formatFileName("closed", currentFrameGroup));
#ifdef USE_ALL
		if (currentFrameGroup>0)
			deleteFile(formatFileName("all", lastAll()));
		renameFile(formatFileName("allnew", currentFrameGroup), formatFileName("all", currentFrameGroup));
#endif
		
		printf("Done.\n");

		if (checkStop())
			return EXIT_STOP;

#ifdef FREE_SPACE_THRESHOLD
		if (getFreeSpace() < FREE_SPACE_THRESHOLD)
		{
			int filterOpen();
			int sortOpen();
			printf("Low disk space detected. Sorting open nodes...\n");
			sortOpen();
			printf("Done. Filtering open nodes...\n");
			filterOpen();
			if (getFreeSpace() < FREE_SPACE_THRESHOLD)
				error("Open node filter failed to produce sufficient free space");
			printf("Done, resuming search...\n");

		}
#endif
	}
	
	printf("Exit not found.\n");
	return EXIT_NOTFOUND;
}

// ********************************************* Pack-open **********************************************

int packOpen()
{
	for (FRAME_GROUP g=firstFrameGroup; g<maxFrameGroups; g++)
    	if (fileExists(formatFileName("open", g)))
    	{
			printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT ": ", g);

			{
				InputStream input(formatFileName("open", g));
				OutputStream output(formatFileName("openpacked", g));
				uint64_t amount = input.size();
				if (amount > BUFFER_SIZE)
					amount = BUFFER_SIZE;
				size_t records;
				uint64_t read=0, written=0;
				while (records = input.read(buffer, (size_t)amount))
				{
					read += records;
					std::sort(buffer, buffer + records);
					records = deduplicate(buffer, records);
					written += records;
					output.write(buffer, records);
				}
				output.flush();

				if (read == written)
					printf("No improvement.\n");
				else
					printf("%llu -> %llu.\n", read, written);
			}
			deleteFile(formatFileName("open", g));
			renameFile(formatFileName("openpacked", g), formatFileName("open", g));
    	}
	return EXIT_OK;
}

// ************************************************ Dump ************************************************

int dump(FRAME_GROUP g)
{
	printf("Dumping frame" GROUP_STR " " GROUP_FORMAT ":\n", g);
	const char* fn = formatFileName("closed", g);
	if (!fileExists(fn))
		fn = formatFileName("open", g);
	if (!fileExists(fn))
		error(format("Can't find neither open nor closed node file for frame" GROUP_STR " " GROUP_FORMAT, g));
	
	BufferedInputStream in(fn);
	const CompressedState* cs;
	while (cs = in.read())
	{
#ifdef GROUP_FRAMES
		printf("Frame %u:\n", GET_FRAME(g, *cs));
#endif
		State s;
		s.decompress(cs);
		puts(s.toString());
	}
	return EXIT_OK;
}

// *********************************************** Sample ***********************************************

int sample(FRAME_GROUP g)
{
	printf("Sampling frame" GROUP_STR " " GROUP_FORMAT ":\n", g);
	const char* fn = formatFileName("closed", g);
	if (!fileExists(fn))
		fn = formatFileName("open", g);
	if (!fileExists(fn))
		error(format("Can't find neither open nor closed node file for frame" GROUP_STR " " GROUP_FORMAT, g));
	
	InputStream in(fn);
	srand((unsigned)time(NULL));
	in.seek(((uint64_t)rand() + ((uint64_t)rand()<<32)) % in.size());
	CompressedState cs;
	in.read(&cs, 1);
#ifdef GROUP_FRAMES
	printf("Frame %u:\n", GET_FRAME(g, cs));
#endif
	State s;
	s.decompress(&cs);
	puts(s.toString());
	return EXIT_OK;
}

// ********************************************** Compare ***********************************************

int compare(const char* fn1, const char* fn2)
{
	BufferedInputStream i1(fn1), i2(fn2);
	printf("%s: %llu states\n%s: %llu states\n", fn1, i1.size(), fn2, i2.size());
	const CompressedState *cs1, *cs2;
	cs1 = i1.read();
	cs2 = i2.read();
	uint64_t dups = 0;
	uint64_t switches = 0;
	int last=0, cur;
	while (cs1 && cs2)
	{
		if (*cs1 < *cs2)
			cs1 = i1.read(),
			cur = -1;
		else
		if (*cs1 > *cs2)
			cs2 = i2.read(),
			cur = 1;
		else
		{
			dups++;
			cs1 = i1.read();
			cs2 = i2.read();
			cur = 0;
		}
		if (cur != last)
			switches++;
		last = cur;
	}
	printf("%llu duplicate states\n", dups);
	printf("%llu interweaves\n", switches);
	return EXIT_OK;
}

// ********************************************** Convert ***********************************************

#ifdef GROUP_FRAMES

// This works only if the size of CompressedState is the same as the old version (without the subframe field).

// HACK: the following code uses pointer arithmetics with BufferedInputStream objects to quickly determine the subframe from which a CompressedState came from.

void convertMerge(BufferedInputStream inputs[], int inputCount, BufferedOutputStream* output)
{
	InputHeap<BufferedInputStream> heap(inputs, inputCount);
	//uint64_t* positions = new uint64_t[inputCount];
	//for (int i=0; i<inputCount; i++)
	//	positions[i] = 0;

	CompressedState cs = *heap.getHead();
	debug_assert(heap.getHeadInput() >= inputs && heap.getHeadInput() < inputs+FRAMES_PER_GROUP);
	cs.subframe = heap.getHeadInput() - inputs;
	bool oooFound = false, equalFound = false;
	while (heap.next())
	{
		CompressedState cs2 = *heap.getHead();
		debug_assert(heap.getHeadInput() >= inputs && heap.getHeadInput() < inputs+FRAMES_PER_GROUP);
		uint8_t subframe = (uint8_t)(heap.getHeadInput() - inputs);
		//positions[subframe]++;
		cs2.subframe = subframe;
		if (cs2 < cs) // work around flush bug in older versions
		{
			if (!oooFound)
			{
				printf("Unordered states found in subframe %d, skipping\n", subframe);
				oooFound = true;
			}
			continue;
		}
		if (cs == cs2) // CompressedState::operator== does not compare subframe
		{
			if (!equalFound)
			{
				//printf("Duplicate states in subframes %d at %lld and %d at %lld\n", cs.subframe, positions[cs.subframe], subframe, positions[subframe]);
				printf("Duplicate states found in subframes %d and %d\n", cs.subframe, subframe);
				equalFound = true;
			}
			if (cs.subframe > subframe) // in case of duplicate frames, pick the one from the smallest frame
				cs.subframe = subframe;
		}
		else
		{
			output->write(&cs, true);
			cs = cs2;
		}
	}
	output->write(&cs, true);
}

int convert()
{
	for (FRAME_GROUP g=firstFrameGroup; g<maxFrameGroups; g++)
	{
		bool haveClosed=false, haveOpen=false;
		BufferedInputStream inputs[FRAMES_PER_GROUP];
		for (FRAME f=g*FRAMES_PER_GROUP; f<(g+1)*FRAMES_PER_GROUP; f++)
			if (fileExists(formatProblemFileName("closed", format("%u", f), "bin")))
				inputs[f%FRAMES_PER_GROUP].open(formatProblemFileName("closed", format("%u", f), "bin")),
				haveClosed = true;
			else
			if (fileExists(formatProblemFileName("open", format("%u", f), "bin")))
				inputs[f%FRAMES_PER_GROUP].open(formatProblemFileName("open", format("%u", f), "bin")),
				haveOpen = true;
		if (haveOpen || haveClosed)
		{
			printf(GROUP_FORMAT "...\n", g);
			{
				BufferedOutputStream output(formatFileName("converting", g));
				convertMerge(inputs, FRAMES_PER_GROUP, &output);
			}
			renameFile(formatFileName("converting", g), formatFileName(haveOpen ? "open" : "closed", g));
		}
		free(inputs);
	}
	return EXIT_OK;
}

// *********************************************** Unpack ***********************************************

// Unpack a closed node frame group file to individual frames.

int unpack()
{
	for (FRAME_GROUP g=firstFrameGroup; g<maxFrameGroups; g++)
		if (fileExists(formatFileName("closed", g)))
		{
			printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT "\n", g); fflush(stdout);
			BufferedInputStream input(formatFileName("closed", g));
			BufferedOutputStream outputs[FRAMES_PER_GROUP];
			for (int i=0; i<FRAMES_PER_GROUP; i++)
				outputs[i].open(formatProblemFileName("closed", format("%u", g*FRAMES_PER_GROUP+i), "bin"));
			const CompressedState* cs;
			while (cs = input.read())
			{
				CompressedState cs2 = *cs;
				cs2.subframe = 0;
				outputs[cs->subframe].write(&cs2);
			}
		}
	return EXIT_OK;
}

// *********************************************** Count ************************************************

int count()
{
	for (FRAME_GROUP g=firstFrameGroup; g<maxFrameGroups; g++)
		if (fileExists(formatFileName("closed", g)))
		{
			printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT ":\n", g);
			BufferedInputStream input(formatFileName("closed", g));
			const CompressedState* cs;
			uint64_t counts[FRAMES_PER_GROUP] = {0};
			while (cs = input.read())
				counts[cs->subframe]++;
			for (int i=0; i<FRAMES_PER_GROUP; i++)
				if (counts[i])
					printf("Frame %u: %llu\n", g*FRAMES_PER_GROUP+i, counts[i]);
			fflush(stdout);
		}
	return EXIT_OK;
}

#endif // GROUP_FRAMES

// *********************************************** Verify ***********************************************

int verify(const char* filename)
{
	BufferedInputStream input(filename);
	CompressedState cs = *input.read();
	bool equalFound=false, oooFound=false;
	uint64_t pos = 0;
	while (1)
	{
		const CompressedState* cs2 = input.read();
		pos++;
		if (cs2==NULL)
			return EXIT_OK;
		if (cs == *cs2)
			if (!equalFound)
			{
				printf("Equal states found: %lld\n", pos);
				equalFound = true;
			}
		if (cs > *cs2)
			if (!oooFound)
			{
				printf("Unordered states found: %lld\n", pos);
				oooFound = true;
			}
#ifdef GROUP_FRAMES
		if (cs2->subframe >= FRAMES_PER_GROUP)
			error("Invalid subframe (corrupted data?)");
#endif
		cs = *cs2;
		if (equalFound && oooFound)
			return EXIT_OK;
	}
}

// ********************************************* Sort-open **********************************************

int sortOpen()
{
	for (FRAME_GROUP currentFrameGroup=maxFrameGroups-1; currentFrameGroup>=firstFrameGroup; currentFrameGroup--)
	{
		if (!fileExists(formatFileName("open", currentFrameGroup)))
			continue;
		if (fileExists(formatFileName("merged", currentFrameGroup)))
			error("Merged file present");
		uint64_t initialSize, finalSize;
		{
			InputStream s(formatFileName("open", currentFrameGroup));
			initialSize = s.size();
		}
		if (initialSize==0)
			continue;

		printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT "/" GROUP_FORMAT ": ", currentFrameGroup, maxFrameGroups); fflush(stdout);

		sortAndMerge(currentFrameGroup);
		
		deleteFile(formatFileName("open", currentFrameGroup));
		renameFile(formatFileName("merged", currentFrameGroup), formatFileName("open", currentFrameGroup));

		{
			InputStream s(formatFileName("open", currentFrameGroup));
			finalSize = s.size();
		}

		printf("Done: %lld -> %lld.\n", initialSize, finalSize);

		if (checkStop())
			return EXIT_STOP;
	}
	return EXIT_OK;
}

// ****************************************** Seq-filter-open *******************************************

// Filters open node lists without expanding nodes.

int seqFilterOpen()
{
	// redeclare currentFrameGroup
	for (FRAME_GROUP currentFrameGroup=firstFrameGroup; currentFrameGroup<maxFrameGroups; currentFrameGroup++)
	{
		if (!fileExists(formatFileName("open", currentFrameGroup)))
			continue;

		printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT "/" GROUP_FORMAT ": ", currentFrameGroup, maxFrameGroups); fflush(stdout);

		uint64_t initialSize, finalSize;

		if (fileExists(formatFileName("merged", currentFrameGroup)))
		{
			printf("(reopening merged)    ");
		}
		else
		{
			{
				InputStream s(formatFileName("open", currentFrameGroup));
				initialSize = s.size();
			}

			// Step 1 & 2: sort and merge open nodes
			sortAndMerge(currentFrameGroup);
		}

		// Step 3: dedup against previous frames
		printf("Filtering... "); fflush(stdout);
		{
			class NullStateHandler
			{
			public:
				INLINE static void handle(const CompressedState* state) {}
			};

			BufferedInputStream* source = new BufferedInputStream(formatFileName("merged", currentFrameGroup));
			BufferedInputStream* inputs = new BufferedInputStream[MAX_FRAME_GROUPS+1];
			int inputCount = 0;
			for (FRAME_GROUP g=0; g<currentFrameGroup; g++)
			{
#ifdef USE_ALL
				if (fileExists(formatFileName("all", g)))
				{
					inputs[inputCount  ].setBufferSize(ALL_FILE_BUFFER_SIZE);
					inputs[inputCount++].open(formatFileName("all", g));
					break;
				}
#endif
				const char* fn = formatFileName("open", g);
				if (!fileExists(fn))
					fn = formatFileName("closed", g);
				if (fileExists(fn))
				{
					inputs[inputCount].open(fn);
					if (inputs[inputCount].size())
						inputCount++;
					else
						inputs[inputCount].close();
				}
			}
			BufferedOutputStream* output = new BufferedOutputStream(formatFileName("filtering", currentFrameGroup));
			filterStream<NullStateHandler>(source, inputs, inputCount, output);
			delete source;
			output->flush(); // force disk flush
			delete output;
			delete[] inputs;
			deleteFile(formatFileName("merged", currentFrameGroup));
		}

		deleteFile(formatFileName("open", currentFrameGroup));
		renameFile(formatFileName("filtering", currentFrameGroup), formatFileName("open", currentFrameGroup));

		{
			InputStream s(formatFileName("open", currentFrameGroup));
			finalSize = s.size();
		}

		printf("Done: %lld -> %lld.\n", initialSize, finalSize);

		if (checkStop())
			return EXIT_STOP;
	}
	return EXIT_OK;
}

// ******************************************** Filter-open *********************************************

void filterStreams(BufferedInputStream closed[], int closedCount, BufferedRewriteStream open[], int openCount)
{
	InputHeap<BufferedInputStream> closedHeap(closed, closedCount);
	InputHeap<BufferedRewriteStream> openHeap(open, openCount);

	bool done = false;
	while (!done)
	{
		CompressedState o = *openHeap.getHead();
		FRAME lowestFrame = MAX_FRAMES;
		do
		{
			FRAME_GROUP group = (FRAME_GROUP)(openHeap.getHeadInput() - open);
			FRAME frame = GET_FRAME(group, *openHeap.getHead());
			if (lowestFrame > frame)
				lowestFrame = frame;
			if (!openHeap.next())
			{
				done = true;
				break;
			}
			if (o > *openHeap.getHead())
				error(format("Unsorted open node file for frame" GROUP_STR " " GROUP_FORMAT "/" GROUP_FORMAT, group, openHeap.getHeadInput() - open));
		} while (o == *openHeap.getHead());
		
		if (closedHeap.scanTo(o))
			if (*closedHeap.getHead() == o)
			{
				closedHeap.next();
				continue;
			}
		SET_SUBFRAME(o, lowestFrame);
		open[lowestFrame/FRAMES_PER_GROUP].write(&o, true);
	}
}

int filterOpen()
{
	BufferedRewriteStream* open = new BufferedRewriteStream[MAX_FRAME_GROUPS];
	for (FRAME_GROUP g=0; g<MAX_FRAME_GROUPS; g++)
		if (fileExists(formatFileName("open", g)))
		{
			enforce(!fileExists(formatFileName("closed", g)), format("Open and closed node files present for the same frame" GROUP_STR " " GROUP_FORMAT, g));
			open[g].open(formatFileName("open", g));
	    }

	BufferedInputStream* closed = new BufferedInputStream[MAX_FRAME_GROUPS];
	for (FRAME_GROUP g=0; g<MAX_FRAME_GROUPS; g++)
#ifdef USE_ALL
		if (fileExists(formatFileName("all", g)))
		{
			closed[g].setBufferSize(ALL_FILE_BUFFER_SIZE);
			closed[g].open(formatFileName("all", g));
			break;
		}
		else
#endif
		if (fileExists(formatFileName("closed", g)))
			closed[g].open(formatFileName("closed", g));

	filterStreams(closed, MAX_FRAME_GROUPS, open, MAX_FRAME_GROUPS);
	delete[] closed;
	
	for (FRAME_GROUP g=0; g<MAX_FRAME_GROUPS; g++) // on success, truncate open nodes manually
		if (open[g].isOpen())
			open[g].truncate();
	
	delete[] open;
	return EXIT_OK;
}

// ****************************************** Regenerate-open *******************************************

int regenerateOpen()
{
	for (FRAME_GROUP g=0; g<MAX_FRAME_GROUPS; g++)
		if (fileExists(formatFileName("closed", g)) || fileExists(formatFileName("open", g)))
			noQueue[g] = true;
	
	while (maxFrameGroups>0 && !fileExists(formatFileName("closed", maxFrameGroups-1)))
		maxFrameGroups--;

	uint64_t oldSize = 0;
	for (currentFrameGroup=firstFrameGroup; currentFrameGroup<maxFrameGroups; currentFrameGroup++)
		if (fileExists(formatFileName("closed", currentFrameGroup)))
		{
			printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT "/" GROUP_FORMAT ": ", currentFrameGroup, maxFrameGroups); fflush(stdout);
			
#ifdef MULTITHREADING
			startWorkers<&processState>();
#endif

			BufferedInputStream closed(formatFileName("closed", currentFrameGroup));
			const CompressedState* cs;
			while (cs = closed.read())
				processFilteredState(cs);
			
#ifdef MULTITHREADING
			flushProcessingQueue();
#endif
			
			printf("Flushing... "); fflush(stdout);
			flushOpen();
			
			uint64_t size = 0;
			for (FRAME_GROUP g=0; g<MAX_FRAME_GROUPS; g++)
				if (queue[g])
					size += queue[g]->size();

			printf("Done (%lld).\n", size - oldSize);
			oldSize = size;

			if (checkStop())
				return EXIT_STOP;
		}
	
	return EXIT_OK;
}

// ********************************************* Create-all *********************************************

int createAll()
{
	FRAME_GROUP maxClosed = 0;
	BufferedInputStream* closed = new BufferedInputStream[MAX_FRAME_GROUPS];
	for (FRAME_GROUP g=0; g<MAX_FRAME_GROUPS; g++)
		if (fileExists(formatFileName("closed", g)))
		{
			closed[g].open(formatFileName("closed", g));
			maxClosed = g;
		}

	{
		BufferedOutputStream all(formatFileName("allnew", maxClosed));
		mergeStreams(closed, MAX_FRAME_GROUPS, &all);
	}

	renameFile(formatFileName("allnew", maxClosed), formatFileName("all", maxClosed));
	return EXIT_OK;
}

// ********************************************* Find-exit **********************************************

int findExit()
{
	if (fileExists(formatFileName("solution")))
		error(format("Partial trace solution file (%s) present - if you want to resume exit tracing, run \"search\" instead, otherwise delete the file", formatFileName("solution")));
	
	// redeclare currentFrameGroup
	for (FRAME_GROUP currentFrameGroup=firstFrameGroup; currentFrameGroup<maxFrameGroups; currentFrameGroup++)
	{
		const char* fn = formatFileName("closed", currentFrameGroup);
		if (!fileExists(fn))
			fn = formatFileName("open", currentFrameGroup);
		if (fileExists(fn))
		{
			printTime(); printf("Frame" GROUP_STR " " GROUP_FORMAT "/" GROUP_FORMAT ": ", currentFrameGroup, maxFrameGroups); fflush(stdout);
			BufferedInputStream input(fn);
			const CompressedState* cs;
			while (cs = input.read())
			{
				State s;
				s.decompress(cs);
				if (s.isFinish())
				{
					FRAME exitFrame = GET_FRAME(currentFrameGroup, *cs);
					printf("Exit found (at frame %u), tracing path...\n", exitFrame);
					traceExit(&s, exitFrame);
					return EXIT_OK;
				}
			}
			printf("Done.\n");
		}
	}
	printf("Exit not found.\n");
	return EXIT_NOTFOUND;
}


// *************************************** Write-partial-solution ***************************************

int writePartialSolution()
{
	if (!fileExists(formatFileName("solution")))
		error(format("Partial trace solution file (%s) not found.", formatFileName("solution")));
	
	int stepNr;
	Step steps[MAX_STEPS];

	FILE* f = fopen(formatFileName("solution"), "rb");
	fread(&exitSearchFrameGroup, sizeof(exitSearchFrameGroup), 1, f);
	fread(&exitSearchState     , sizeof(exitSearchState)     , 1, f);
	fread(&stepNr              , sizeof(stepNr)              , 1, f);
	fread(steps, sizeof(Step), stepNr, f);
	fclose(f);

	writeSolution(&exitSearchState, steps, stepNr);

	return EXIT_OK;
}

// ***************************************** Win32 idle watcher *****************************************

// use background CPU and I/O priority when PC is not idle

#if defined(_WIN32)
#pragma comment(lib,"user32")
DWORD WINAPI idleWatcher(__in LPVOID lpParameter)
{
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    while (true)
    {
		do
		{
			Sleep(1000);
			GetLastInputInfo(&lii);
		}
		while (GetTickCount() - lii.dwTime > 60*1000);
		
		do
		{
			FILE* f = fopen("idle.txt", "rt");
			if (f)
			{
				int work, idle;
				fscanf(f, "%d %d", &work, &idle);
				fclose(f);
				
				Sleep(work);
				SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
				Sleep(idle);
				SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
			}
			else
				Sleep(1000);
			GetLastInputInfo(&lii);
		}
		while (GetTickCount() - lii.dwTime < 60*1000);
	}
}
#endif

// ******************************************************************************************************

timeb startTime;

void printExecutionTime()
{
	timeb endTime;
	ftime(&endTime);
	time_t ms = (endTime.time - startTime.time)*1000
	       + (endTime.millitm - startTime.millitm);
	printf("Time: %d.%03d seconds.\n", ms/1000, ms%1000);
}

// ***********************************************************************************

// Test the CompressedState comparison operators.
void testCompressedState()
{
	enforce(sizeof(CompressedState)%4 == 0);
#ifdef GROUP_FRAMES
	enforce(COMPRESSED_BITS <= (sizeof(CompressedState)-1)*8);
#else
	enforce(COMPRESSED_BITS <= sizeof(CompressedState)*8);
	enforce((COMPRESSED_BITS+31)/8 >= sizeof(CompressedState));
#endif

	CompressedState c1, c2;
	uint8_t *p1 = (uint8_t*)&c1, *p2 = (uint8_t*)&c2;
	memset(p1, 0, sizeof(CompressedState));
	memset(p2, 0, sizeof(CompressedState));
	
#ifdef GROUP_FRAMES
	int subframe;

	switch (COMPRESSED_BYTES % 4)
	{
		case 0: subframe = sizeof(CompressedState)-4; break;
		case 1: // align to word - fall through
		case 2: subframe = sizeof(CompressedState)-2; break;
		case 3: subframe = sizeof(CompressedState)-1; break;
	}

	p1[subframe] = 0xFF;
	enforce(c1 == c2, "Different subframe causes inequality");
	c2.subframe = 0xFF;
	enforce(c1.subframe == 0xFF && p2[subframe] == 0xFF && memcmp(p1, p2, sizeof(CompressedState))==0, format("Misaligned subframe!\n%s\n%s", hexDump(p1, sizeof(CompressedState)), hexDump(p2, sizeof(CompressedState))));
#endif
	
	for (int i=0; i<COMPRESSED_BITS; i++)
	{
		p1[i/8] |= (1<<(i%8));
		enforce(c1 != c2, format("Inequality expected!\n%s\n%s", hexDump(p1, sizeof(CompressedState)), hexDump(p2, sizeof(CompressedState))));
		p2[i/8] |= (1<<(i%8));
		enforce(c1 == c2, format(  "Equality expected!\n%s\n%s", hexDump(p1, sizeof(CompressedState)), hexDump(p2, sizeof(CompressedState))));
	}
}

// ***********************************************************************************

int parseInt(const char* str)
{
	int result;
	if (!sscanf(str, "%d", &result))
		error(format("'%s' is not a valid integer", str));
	return result;
}

void parseFrameRange(int argc, const char* argv[])
{
	if (argc==0)
		firstFrameGroup = 0, 
		maxFrameGroups  = MAX_FRAME_GROUPS;
	else
	if (argc==1)
		firstFrameGroup = parseInt(argv[0]),
		maxFrameGroups  = firstFrameGroup+1;
	else
	if (argc==2)
		firstFrameGroup = parseInt(argv[0]),
		maxFrameGroups  = parseInt(argv[1]);
	else
		error("Too many arguments");
}

const char* usage = "\
Generic C++ DDD solver\n\
(c) 2009-2010 Vladimir \"CyberShadow\" Panteleev\n\
Usage:\n\
	search <mode> <parameters>\n\
where <mode> is one of:\n\
	search [max-frame"GROUP_STR"]\n\
		Sorts, filters and expands open nodes. 	If no open node files\n\
		are present, starts a new search from the initial state.\n\
	dump <frame"GROUP_STR">\n\
		Dumps all states from the specified frame"GROUP_STR", which\n\
		can be either open or closed.\n\
	sample <frame"GROUP_STR">\n\
		Displays a random state from the specified frame"GROUP_STR", which\n\
		can be either open or closed.\n\
	compare <filename-1> <filename-2>\n\
		Counts the number of duplicate nodes in two files. The nodes in\n\
		the files must be sorted and deduplicated.\n"
#ifdef GROUP_FRAMES
"	convert [frame"GROUP_STR"-range]\n\
		Converts individual frame files to frame"GROUP_STR" files for the\n\
		specified frame"GROUP_STR" range.\n\
	unpack [frame"GROUP_STR"-range]\n\
		Converts frame"GROUP_STR" files back to individual frame files\n\
		(reverses the \"convert\" operation).\n\
	count [frame"GROUP_STR"-range]\n\
		Counts the number of nodes in individual frames for the\n\
		specified frame"GROUP_STR" files.\n"
#endif
"	verify <filename>\n\
		Verifies that the nodes in a file are correctly sorted and\n\
		deduplicated, as well as a few additional integrity checks.\n\
	pack-open [frame"GROUP_STR"-range]\n\
		Removes duplicates within each chunk for open node files in the\n\
		specified range. Reads and writes open nodes only once.\n\
	sort-open [frame"GROUP_STR"-range]\n\
		Sorts and removes duplicates for open node files in the\n\
		specified range. File are processed in reverse order.\n\
	filter-open\n\
		Filters all open node files. Requires that all open node files\n\
		be sorted and deduplicated (run sort-open before filter-open).\n\
		Filtering is performed in-place. An aborted run shouldn't cause\n\
		data loss, but will require re-sorting.\n\
	seq-filter-open [frame"GROUP_STR"-range]\n\
		Sorts, deduplicates and filters open node files in the\n\
		specified range, one by one. Specify the range cautiously,\n\
		as this function requires that previous open node files be\n\
		sorted and deduplicated (and filtered for best performance).\n\
	regenerate-open [frame"GROUP_STR"-range]\n\
		Re-expands closed nodes in the specified frame"GROUP_STR" range.\n\
		New (open) nodes are saved only for frame"GROUP_STR"s that don't\n\
		already have an open or closed node file. Use this when an open\n\
		node file has been accidentally deleted or corrupted. To\n\
		regenerate all open nodes, delete all open node files before\n\
		running regenerate-open (this is still faster than restarting\n\
		the search).\n\
	create-all\n\
		Creates the \"all\" file from closed node files. Use when\n\
		turning on USE_ALL, or when the \"all\" file was corrupted.\n\
	find-exit [frame"GROUP_STR"-range]\n\
		Searches for exit frames in the specified frame"GROUP_STR" range\n\
		(both closed an open node files). When a state is found which\n\
		satisfies the isFinish condition, it is traced back and the\n\
		solution is written, as during normal search.\n\
	write-partial-solution\n\
		Saves the partial solution, using the partial exit trace solution\n\
		file. Allows exit tracing inspection. Warning: uses the same code\n\
		as when writing the full solution, and may overwrite an existing\n\
		solution.\n\
A [frame"GROUP_STR"-range] is a space-delimited list of zero, one or two frame"GROUP_STR"\n\
numbers. If zero numbers are specified, the range is assumed to be all\n\
frame"GROUP_STR"s. If one number is specified, the range is set to only that\n\
frame"GROUP_STR" number. If two numbers are specified, the range is set to start\n\
from the first frame"GROUP_STR" number inclusively, and end at the second\n\
frame"GROUP_STR" number NON-inclusively.\n\
";

int run(int argc, const char* argv[])
{
	enforce(sizeof(intptr_t)==sizeof(size_t), "Bad intptr_t!");
	enforce(sizeof(int)==4, "Bad int!");
	enforce(sizeof(long long)==8, "Bad long long!");

	initProblem();

#ifdef DEBUG
	printf("Debug version\n");
#else
	printf("Optimized version\n");
#endif

#ifdef MULTITHREADING
#if defined(THREAD_BOOST)
	printf("Using %u Boost threads ", THREADS);
#elif defined(THREAD_WINAPI)
	printf("Using %u WinAPI threads ", THREADS);
#else
#error Thread plugin not set
#endif

#if defined(SYNC_BOOST)
	printf("with Boost sync\n");
#elif defined(SYNC_WINAPI)
	printf("with WinAPI sync\n");
#elif defined(SYNC_WINAPI_SPIN)
	printf("with WinAPI spinlock sync\n");
#elif defined(SYNC_INTEL_SPIN)
	printf("with Intel spinlock sync\n");
#else
#error Sync plugin not set
#endif
#endif // MULTITHREADING
	
	printf("Compressed state is %u bits (%u bytes data, %u bytes total)\n", COMPRESSED_BITS, COMPRESSED_BYTES, sizeof(CompressedState));
#ifdef SLOW_COMPARE
	printf("Using memcmp for CompressedState comparison\n");
#endif
	testCompressedState();

	enforce(ram, "RAM allocation failed");
	printf("Using %lld bytes of RAM for %lld cache nodes and %lld buffer nodes\n", (long long)RAM_SIZE, (long long)CACHE_HASH_SIZE, (long long)BUFFER_SIZE);

#if defined(DISK_WINFILES)
	printf("Using Windows API files\n");
#elif defined(DISK_POSIX)
	printf("Using POSIX files\n");
#else
#error Disk plugin not set
#endif

#ifdef USE_ALL
	printf("Using \"all\" files\n");
#endif

	if (fileExists(formatProblemFileName("stop", NULL, "txt")))
	{
		printf("Stop file present.\n");
		return EXIT_STOP;
	}

#if defined(_WIN32)
	CreateThread(NULL, 0, &idleWatcher, NULL, 0, NULL);
#endif

	printf("Command-line:");
	for (int i=0; i<argc; i++)
		printf(" %s", argv[i]);
	printf("\n");

	maxFrameGroups = MAX_FRAME_GROUPS;

	ftime(&startTime);
	atexit(&printExecutionTime);

	if (argc>1 && strcmp(argv[1], "search")==0)
	{
		if (argc>2)
			maxFrameGroups = parseInt(argv[2]);
		return search();
	}
	else
	if (argc>1 && strcmp(argv[1], "dump")==0)
	{
		enforce(argc==3, "Specify a frame"GROUP_STR" to dump");
		return dump(parseInt(argv[2]));
	}
	else
	if (argc>1 && strcmp(argv[1], "sample")==0)
	{
		enforce(argc==3, "Specify a frame"GROUP_STR" to sample");
		return sample(parseInt(argv[2]));
	}
	else
	if (argc>1 && strcmp(argv[1], "compare")==0)
	{
		enforce(argc==4, "Specify two files to compare");
		return compare(argv[2], argv[3]);
	}
	else
#ifdef GROUP_FRAMES
	if (argc>1 && strcmp(argv[1], "convert")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return convert();
	}
	else
	if (argc>1 && strcmp(argv[1], "unpack")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return unpack();
	}
	else
	if (argc>1 && strcmp(argv[1], "count")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return count();
	}
	else
#endif
	if (argc>1 && strcmp(argv[1], "verify")==0)
	{
		enforce(argc==3, "Specify a file to verify");
		return verify(argv[2]);
	}
	else
	if (argc>1 && strcmp(argv[1], "pack-open")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return packOpen();
	}
	else
	if (argc>1 && strcmp(argv[1], "sort-open")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return sortOpen();
	}
	else
	if (argc>1 && strcmp(argv[1], "filter-open")==0)
	{
		return filterOpen();
	}
	else
	if (argc>1 && strcmp(argv[1], "seq-filter-open")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return seqFilterOpen();
	}
	else
	if (argc>1 && strcmp(argv[1], "regenerate-open")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return regenerateOpen();
	}
	if (argc>1 && strcmp(argv[1], "create-all")==0)
	{
		return createAll();
	}
	if (argc>1 && strcmp(argv[1], "find-exit")==0)
	{
		parseFrameRange(argc-2, argv+2);
		return findExit();
	}
	else
	if (argc>1 && strcmp(argv[1], "write-partial-solution")==0)
	{
		return writePartialSolution();
	}
	else
	{
		printf("%s", usage);
		return EXIT_OK;
	}
}

// ***********************************************************************************

#ifdef NO_MAIN
// define main() in another file and #include "search.cpp"
#elif defined(PROBLEM_RELATED)
#include BOOST_PP_STRINGIZE(PROBLEM/PROBLEM_RELATED.cpp)
int main(int argc, const char* argv[]) { try { return run_related(argc, argv); } catch(const char* s) { printf("\n%s\n", s); return EXIT_ERROR; } }
#else
int main(int argc, const char* argv[]) { try { return run        (argc, argv); } catch(const char* s) { printf("\n%s\n", s); return EXIT_ERROR; } }
#endif
