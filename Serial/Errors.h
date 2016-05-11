
#include <string>
#include <cstdio>

class SerialError
{
private:
	std::string error;

	SerialError(std::string errText) : error(errText) {}
	void print(FILE* f) { fprintf(f, error.c_str()); }
};

void dbgprint(const char* s, ...);