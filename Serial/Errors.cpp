/**********************
Errors.cpp
**********************/
#include <cstdarg>
#include <cstdio>

void dbgprint(const char* s, ...)
{
#ifdef _DEBUG
	va_list args;
	int n;
	va_start(args, s);
	vfprintf(stdout, s, args);
	va_end(args);
#endif
}