//#include "stdafx.h"

#include "SerialDevice.h"
#include <Windows.h>
#include <Process.h>
#include <string>
#include "Errors.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


void SerialDevice::InvalidateHandle(HANDLE& handle)
{
	handle = INVALID_HANDLE_VALUE;
}


void SerialDevice::CloseAndCleanHandle(HANDLE& handle)
{

	BOOL ret = CloseHandle(handle);
	if (ret == 0)
	{
		//ASSERT(0);
	}
	InvalidateHandle(handle);

}
SerialDevice::SerialDevice()
{
	InvalidateHandle(threadTerm);
	InvalidateHandle(thread);
	InvalidateHandle(threadStarted);
	InvalidateHandle(port);
	InvalidateHandle(dataReady);

	InitLock();
	state = SERIALSTATE_UNINITIALISED;

}

SerialDevice::~SerialDevice()
{
	state = SERIALSTATE_UNKNOWN;
	DelLock();
}


HRESULT SerialDevice::Init()
{
	HRESULT hr = S_OK;
	try
	{
		dataReady = CreateEvent(0, 0, 0, 0);

		//open the COM Port
		port =CreateFile((const wchar_t*)portName,
			GENERIC_READ | GENERIC_WRITE, //access ( read and write)
			0,							  //(share) 0:cannot share the COM port						
			0,							  //security  (None)				
			OPEN_EXISTING,				  // creation : open_existing
			FILE_FLAG_OVERLAPPED,		  // we want overlapped operation
			0							  // no templates file for COM port...
			);
		if (port == INVALID_HANDLE_VALUE)
		{
			dbgprint("SerialDevice : Failed to open COM Port Reason: %d", GetLastError());
			//ASSERT(0);
			return E_FAIL;
		}

		dbgprint("SerialDevice : COM port opened successfully\n");
		//now start to read but first we need to set the COM port settings and the timeouts
		if (!SetCommMask(port, EV_RXCHAR | EV_TXEMPTY))
		{
			//ASSERT(0);
			dbgprint("SerialDevice : Failed to Set Comm Mask Reason: %d\n", GetLastError());
			return E_FAIL;
		}
		dbgprint("SerialDevice : SetCommMask() success\n");

		//now we need to set baud rate etc,
		DCB dcb = { 0 };

		dcb.DCBlength = sizeof(DCB);
		if (!GetCommState(port, &dcb))
		{
			dbgprint("SerialDevice : Failed to Get Comm State Reason: %d\n", GetLastError());
			return E_FAIL;
		}

		dcb.BaudRate = baudRate;
		dcb.ByteSize = byteSize;
		dcb.Parity = parity;
		dcb.StopBits = stopBits;
		
		dcb.fDsrSensitivity = 0;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fOutxDsrFlow = 0;


		if (SetCommState(port, &dcb) == 0)
		{
			//ASSERT(0);
			dbgprint("SerialDevice : Failed to Set Comm State Reason: %d\n", GetLastError());
			return E_FAIL;
		}
		fprintf(stdout,"SerialDevice : Current Settings, (Baud Rate %d; Parity %d; Byte Size %d; Stop Bits %d\n", dcb.BaudRate, dcb.Parity, dcb.ByteSize, dcb.StopBits);



		//now set the timeouts ( we control the timeout overselves using WaitForXXX()
		COMMTIMEOUTS timeouts;

		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 0;

		if (!SetCommTimeouts(port, &timeouts))
		{
			//ASSERT(0);
			dbgprint("SerialDevice :  Error setting time-outs. %d\n", GetLastError());
			return E_FAIL;
		}
		//create thread terminator event...
		threadTerm = CreateEvent(0, 0, 0, 0);
		threadStarted = CreateEvent(0, 0, 0, 0);

		isConnected = true;

	}
	catch (...)
	{
		//ASSERT(0);
		hr = E_FAIL;
	}
	if (SUCCEEDED(hr))
	{
		state = SERIALSTATE_INITIALISED;
	}
	return hr;

}

unsigned int __stdcall SerialDevice::startReadingLoopThread(void* This)
{
	SerialDevice* callingInstance = (SerialDevice*)This;
	ResetEvent(callingInstance->threadTerm);
	callingInstance->thread = (HANDLE)_beginthreadex(0, 0, SerialDevice::ReadingLoopThread, (void*)This, 0, 0);
	DWORD dwWait = WaitForSingleObject(callingInstance->threadStarted, INFINITE); // started?
	//ASSERT(dwWait == WAIT_OBJECT_0);
	CloseHandle(callingInstance->threadStarted);
	callingInstance->InvalidateHandle(callingInstance->threadStarted);

	return 0;
}

HRESULT SerialDevice::Start()
{
	state = SERIALSTATE_STARTED;
	_beginthreadex(0, 0, SerialDevice::startReadingLoopThread, (void*)this, 0, 0);
	return S_OK;

}

HRESULT SerialDevice::Stop()
{
	state = SERIALSTATE_STOPPED;
	SetEvent(threadTerm);
	
	return S_OK;
}

HRESULT SerialDevice::UnInit()
{
	HRESULT hr = S_OK;
	try
	{
		isConnected = false;
		SignalObjectAndWait(threadTerm, thread, INFINITE, FALSE);
		CloseAndCleanHandle(threadTerm);
		CloseAndCleanHandle(thread);
		CloseAndCleanHandle(dataReady);
		CloseAndCleanHandle(port);
	}
	catch (...)
	{
		//ASSERT(0);
		hr = E_FAIL;
	}
	if (SUCCEEDED(hr))
		state = SERIALSTATE_UNINITIALISED;
	return hr;
}


unsigned __stdcall SerialDevice::ReadingLoopThread(void*cInstance)
{
	SerialDevice* callingInstance = (SerialDevice*)cInstance;
	bool ok = true;
	DWORD dwEventMask = 0;

	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	ov.hEvent = CreateEvent(0, true, 0, 0);
	HANDLE handles[2];
	handles[0] = callingInstance->threadTerm;

	DWORD dwWait;
	SetEvent(callingInstance->threadStarted);
	while (ok)
	{

		BOOL ret =WaitCommEvent(callingInstance->port, &dwEventMask, &ov);
		if (!ret)
		{
			//ASSERT(GetLastError() == ERROR_IO_PENDING);
		}


		handles[1] = ov.hEvent;

		dwWait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
		/* there are two events which cause the thread to respond:
		 threadTerm, which means that the thread must terminate
		 ov.hEvent, which means that there is data to be read(?) */


		switch (dwWait)
		{
		case WAIT_OBJECT_0: // handles[0] was set
		{
			_endthreadex(1);
			// terminate the thread
		}
		break;
		case WAIT_OBJECT_0 + 1: // handles[1] was set
		{
			// data to be read?
			DWORD dwMask;
			if (GetCommMask(callingInstance->port, &dwMask))
			{
				if (dwMask == EV_TXEMPTY)
				{
					ResetEvent(ov.hEvent);
					continue;
				}

			}

			//read data here...
			int accum = 0;

			// data will go into the buffer; lock it first
			callingInstance->serialBuffer.LockBuffer();

			try
			{
				std::string szDebug;
				BOOL ret = false;

				DWORD bytesRead = 0;
				OVERLAPPED ovRead;
				memset(&ovRead, 0, sizeof(ovRead));
				ovRead.hEvent = CreateEvent(0, true, 0, 0);

				do
				{
					//if (WaitForSingleObject(callingInstance->threadTerm, 0) == WAIT_OBJECT_0)
					
					ResetEvent(ovRead.hEvent);
					char Tmp[1]; // Read one single char!
					int iSize = sizeof(Tmp);
					memset(Tmp, 0, sizeof Tmp);
					ret = ReadFile(callingInstance->port, Tmp, sizeof(Tmp), &bytesRead, &ovRead);
					if (ret == 0) // some error occured
					{
						ok = FALSE;
						break;
					}
					if (bytesRead > 0)
					{
						callingInstance->processData(Tmp, bytesRead);
						accum += bytesRead;
					}
				} while (bytesRead > 0);
				CloseHandle(ovRead.hEvent);
			}
			catch (...)
			{
				//ASSERT(0);
			}

			//if we are not in started state then we should flush the queue...( we would still read the data)
			if (callingInstance->GetCurrentState() != SERIALSTATE_STARTED)
			{
				accum = 0;
				callingInstance->serialBuffer.Flush();
			}

			callingInstance->serialBuffer.UnLockBuffer();

			if (accum > 0)
			{
				callingInstance->SetDataReadEvent();
			}
			ResetEvent(ov.hEvent);
		}
		break;
		}//switch
	}
	return 0;
}


HRESULT  SerialDevice::CanProcess()
{

	switch (state)
	{
	case SERIALSTATE_UNKNOWN://ASSERT(0); return E_FAIL;
	case SERIALSTATE_UNINITIALISED:return E_FAIL;
	case SERIALSTATE_STARTED:return S_OK;
	case SERIALSTATE_INITIALISED:
	case SERIALSTATE_STOPPED:
		return E_FAIL;

	}
	return E_FAIL;
}

HRESULT SerialDevice::Write(const char* data, DWORD size)
{
	HRESULT hr = CanProcess();
	if (FAILED(hr)) return hr;
	int iRet = 0;
	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	ov.hEvent = CreateEvent(0, true, 0, 0);
	DWORD bytesWritten = 0;
	//do
	{
		iRet = WriteFile(port, data, size, &bytesWritten, &ov);
		if (iRet == 0)
		{
			WaitForSingleObject(ov.hEvent, INFINITE);
		}

	}//	while ( ov.InternalHigh != size ) ;
	CloseHandle(ov.hEvent);
	std::string debugdata(data);
	dbgprint(("RCSerial:Writing:{%s} len:{%d}"), (debugdata).c_str(), debugdata.size());

	return S_OK;
}

void SerialDevice::processData(char* data, long length)
{
	if (length > 0)
		fprintf(stdout, "%c", data[0]);
	//serialBuffer.AddData(data, length); // send char to buffer
}

SerialDevice::SerialDevice(wchar_t* portName, long baudRate, int parity, int stopBits, int byteSize)
{
	this->portName = new wchar_t[wcslen(portName) + 1];
	wcscpy_s(this->portName, wcslen(portName) + 1, portName);
	this->baudRate = baudRate;
	this->parity = parity;
	this->stopBits = stopBits;
	this->byteSize = byteSize;
}

void SerialDevice::setPortName(wchar_t* name)
{
	if (portName != 0)
		delete[] portName;
	portName = new wchar_t[wcslen(portName) + 1];
	wcscpy_s(portName, wcslen(portName) + 1, name);
}

void SerialDevice::setBaudRate(long baud)
{
	baudRate = baud;
}

void SerialDevice::setParity(int parity)
{
	this->parity = parity;
}

void SerialDevice::setStopBit(int stopBits)
{
	this->stopBits = stopBits;
}

void SerialDevice::setDataSize(int byteSize)
{
	this->byteSize = byteSize;
}

/*
HRESULT SerialDevice::ReadTerminated(std::string& data, char terminator, long* count, long timeOut)
{
HRESULT hr = CanProcess();
if (FAILED(hr)) return hr;

try
{
std::string Tmp;
Tmp.erase();
long bytesRead;

bool found = serialBuffer.ReadTerminated(Tmp, terminator, bytesRead, dataReady);

if (found)
{
data = Tmp;
}
else
{//there are either none or less bytes...
long iRead = 0;
bool ok = true;
while (ok)
{
DWORD dwWait =WaitForSingleObject(dataReady, timeOut);

if (dwWait == WAIT_TIMEOUT)
{
data.erase();
hr = E_FAIL;
return hr;
}

bool found = serialBuffer.ReadTerminated(Tmp, terminator, bytesRead, dataReady);
if (found)
{
data = Tmp;
return S_OK;
}

}
}

}
catch (...)
{
//ASSERT(0);
}
return hr;

}

HRESULT SerialDevice::ReadByNumber(std::string& data, long count, long  timeOut)
{
HRESULT hr = CanProcess();

if (FAILED(hr))
{
return hr;
}
try
{
std::string Tmp;
Tmp.erase();


int iLocal = serialBuffer.ReadByNumber(Tmp, count, dataReady);

if (iLocal == count)
{
data = Tmp;
}
else
{//there are either none or less bytes...
long iRead = 0;
int iRemaining = count - iLocal;
while (1)
{
DWORD dwWait = WaitForSingleObject(dataReady, timeOut);
if (dwWait == WAIT_TIMEOUT)
{
data.erase();
hr = E_FAIL;
return hr;
}

iRead = serialBuffer.ReadByNumber(Tmp, iRemaining, dataReady);
iRemaining -= iRead;


if (iRemaining == 0)
{
data = Tmp;
return S_OK;
}
}
}
}
catch (...)
{
}
return hr;

}
//-----------------------------------------------------------------------
//-- Reads all the data that is available in the local buffer..
//does NOT make any blocking calls in case the local buffer is empty
//-----------------------------------------------------------------------
HRESULT SerialDevice::ReadAvailable(std::string& data)
{

HRESULT hr = CanProcess();
if (FAILED(hr)) return hr;
try
{
std::string temp;
bool ret = serialBuffer.ReadAvailable(temp, dataReady);

data = temp;
}
catch (...)
{
//ASSERT(0);
hr = E_FAIL;
}
return hr;

}
*/