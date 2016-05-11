#include <Windows.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include "SerialDevice.h"
using namespace std;

void InitConsole()
{
	AllocConsole();

	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	int hCrt = _open_osfhandle((long)handle_out, _O_TEXT);
	FILE* hf_out = _fdopen(hCrt, "w");
	setvbuf(hf_out, NULL, _IONBF, 1);
	*stdout = *hf_out;

	HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
	hCrt = _open_osfhandle((long)handle_in, _O_TEXT);
	FILE* hf_in = _fdopen(hCrt, "r");
	setvbuf(hf_in, NULL, _IONBF, 128);
	*stdin = *hf_in;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////||||||||||||||//////////////////////////////
///////////////////////////vvvvvvvvvvvvvv//////////////////////////////
///////////////////////////////////////////////////////////////////////

class DerivedSerialDevice : public SerialDevice
{
	// overriding constructor
public:
	DerivedSerialDevice(wchar_t* name) : SerialDevice(name) {}

	// overriding virtual data processing method
	void processData(char* data, long length)
	{
		cout << "/" << data[0] << "\\" ;; // fancy data output
	}
};

///////////////////////////////////////////////////////////////////////
///////////////////////////^^^^^^^^^^^^^^//////////////////////////////
///////////////////////////||||||||||||||//////////////////////////////
///////////////////////////////////////////////////////////////////////

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
	InitConsole();
	DerivedSerialDevice sd(L"COM3");
	if (sd.Init() == S_OK)
		cout << "Am deschis!";
	else
		cout << "N-am deschis!";
	string s;

	sd.Start();
	getchar();
	sd.Stop();
	sd.UnInit();
	cout << "S-a inchis!\n";
	getchar();
	return 0;
}