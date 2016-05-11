// SerialBuffer.cpp: implementation of the SerialBuffer class.
//
//////////////////////////////////////////////////////////////////////

#include "SerialBuffer.h"
#include "SerialDevice.h"
#include "Errors.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

#ifdef _DEBUG

#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SerialBuffer::SerialBuffer()
{
	Init();
}

void SerialBuffer::Init()
{
	InitializeCriticalSection(&criticalSectionLock);
	lockAlways = true;
	currentPos = 0;
	bytesUnread = 0;
	internalBuffer.erase();

}
SerialBuffer::~SerialBuffer()
{
	DeleteCriticalSection(&criticalSectionLock);
}
void SerialBuffer::AddData(char ch)
{
	internalBuffer += ch;
	bytesUnread += 1;
}

void SerialBuffer::AddData(std::string& data, int iLen)
{
	//dbgprint(("SerialBuffer : (tid:%d) AddData(%s,%d) called "), GetCurrentThreadId(), data.c_str(), iLen);
	internalBuffer.append(data.c_str(), iLen);
	bytesUnread += iLen;
}

void SerialBuffer::AddData(char *strData, int iLen)
{
	internalBuffer.append(strData, iLen);
	bytesUnread += iLen;
}

void SerialBuffer::AddData(std::string &data)
{
	internalBuffer += data;
	bytesUnread += data.size();

}
void SerialBuffer::Flush()
{
	LockBuffer(); // enter critical section
	internalBuffer.erase();
	bytesUnread = 0;
	currentPos = 0;
	UnLockBuffer(); // leave critical section
}

long SerialBuffer::ReadByNumber(std::string &data, long  count, HANDLE & eventToReset)
{
	//ASSERT(eventToReset != INVALID_HANDLE_VALUE);

	LockBuffer();
	long tempCount = min(count, bytesUnread);
	long alActualSize = GetSize();

	data.append(internalBuffer, currentPos, tempCount);

	currentPos += tempCount;

	bytesUnread -= tempCount;
	if (bytesUnread == 0)
	{
		ClearAndReset(eventToReset);
	}

	UnLockBuffer();
	return tempCount;
}


bool SerialBuffer::ReadAvailable(std::string &data, HANDLE & eventToReset)
{

	LockBuffer();
	data += internalBuffer;

	ClearAndReset(eventToReset);

	UnLockBuffer();

	return (data.size() > 0);
}


void SerialBuffer::ClearAndReset(HANDLE& eventToReset)
{
	internalBuffer.erase();
	bytesUnread = 0;
	currentPos = 0;
	ResetEvent(eventToReset);

}

bool SerialBuffer::ReadTerminated(std::string &data, char terminatorChar, long  &bytesRead, HANDLE & eventToReset)
{
	LockBuffer();
	bytesRead = 0;

	bool found = false;
	if (bytesUnread > 0)
	{//if there are some bytes un-read...

		int actualSize = GetSize();
		int incrementPos = 0;
		for (int i = currentPos; i < actualSize; ++i)
		{
			data += internalBuffer[i];
			bytesUnread -= 1;
			if (internalBuffer[i] == terminatorChar) // the term char has been found
			{
				incrementPos++; // so stop the loop
				found = true;
				break;
			}
			incrementPos++;
		}
		currentPos += incrementPos;
		if (bytesUnread == 0) // all bytes in the buffer have been read
		{
			ClearAndReset(eventToReset);
		}
	}
	UnLockBuffer();
	return found;
}