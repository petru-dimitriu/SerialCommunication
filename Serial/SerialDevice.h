
#pragma once

#include "SerialBuffer.h"
#include <map>

typedef enum tagSERIAL_STATE
{
	SERIALSTATE_UNKNOWN,
	SERIALSTATE_UNINITIALISED,
	SERIALSTATE_INITIALISED,
	SERIALSTATE_STARTED,
	SERIALSTATE_STOPPED,

} SERIAL_STATE;

class SerialDevice
{
private:
	wchar_t* portName;
	long baudRate;
	int parity, stopBits, byteSize;

	SERIAL_STATE	state;
	HANDLE	port;
	HANDLE	threadTerm;
	HANDLE	thread;
	HANDLE	threadStarted;
	HANDLE	dataReady;
	bool	isConnected;
	void	InvalidateHandle(HANDLE& handle);
	void	CloseAndCleanHandle(HANDLE& handle);
	static unsigned int __stdcall	startReadingLoopThread(void*);

	SerialBuffer serialBuffer;
	CRITICAL_SECTION criticalSectionLock;
	SERIAL_STATE GetCurrentState() { return state; }
public:
	SerialDevice();
	SerialDevice(wchar_t* portName, long baudRate=9600, int parity = NOPARITY, int stopBits = ONESTOPBIT, int byteSize = 8);
	void setBaudRate(long baudRate);
	void setParity(int parity);
	void setStopBit(int stopBits);
	void setPortName(wchar_t* name);
	void setDataSize(int byteSize);
	virtual ~SerialDevice();
	HANDLE	GetWaitForEvent() { return dataReady; }

	inline void		LockcallingInstance()			{ EnterCriticalSection(&criticalSectionLock); }
	inline void		UnLockcallingInstance()		{ LeaveCriticalSection(&criticalSectionLock); }
	inline void		InitLock()			{ InitializeCriticalSection(&criticalSectionLock); }
	inline void		DelLock()			{ DeleteCriticalSection(&criticalSectionLock); }
	inline bool		IsInputAvailable()
	{
		LockcallingInstance();
		bool abData = (!serialBuffer.IsEmpty());
		UnLockcallingInstance();
		return abData;
	}
	inline bool		IsConnection() { return isConnected; }
	inline void		SetDataReadEvent()	{ SetEvent(dataReady); }

	HRESULT			Write(const char* data, DWORD size);
	HRESULT			Init();
	HRESULT			Start();
	HRESULT			Stop();
	HRESULT			UnInit();

	// -- !!
	// function which runs in a separate thread,
	// reads data and sends it to the processing function
	static unsigned __stdcall ReadingLoopThread(void*cInstance);

	HRESULT  CanProcess();
	void OnSetDebugOption(long  iOpt, BOOL bOnOff);

	// -- !!
	// virtual method which processes the data that
	// has just been received
	virtual void processData(char* data, long length);

};


