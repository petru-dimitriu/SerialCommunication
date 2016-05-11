// SerialBuffer.h: interface for the SerialBuffer class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <Windows.h>


class SerialBuffer
{
	std::string internalBuffer;
	CRITICAL_SECTION	criticalSectionLock;
	bool	lockAlways;
	long	currentPos;
	long  bytesUnread;
	void  Init();
	void	ClearAndReset(HANDLE& eventToReset);
public:
	inline void LockBuffer() {EnterCriticalSection(&criticalSectionLock); }
	inline void UnLockBuffer() {LeaveCriticalSection(&criticalSectionLock); }


	SerialBuffer();
	virtual ~SerialBuffer();

	//---- public interface --
	void AddData(char ch);
	void AddData(std::string& data);
	void AddData(std::string& data, int iLen);
	void AddData(char *strData, int iLen);
	std::string GetData() { return internalBuffer; }

	void		Flush();
	long		ReadByNumber(std::string &data, long count, HANDLE& eventToReset);
	bool		ReadTerminated(std::string &data, char terminatorChar, long  &bytesRead, HANDLE& eventToReset);
	bool		ReadAvailable(std::string &data, HANDLE & eventToReset);
	inline long GetSize() { return internalBuffer.size(); }
	inline bool IsEmpty() { return internalBuffer.size() == 0; }
};