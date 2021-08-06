#include <Windows.h>

#include "XLib.Program.h"

using namespace XLib;

int main()
{
	Program::Run();
	return 0;
}

int __stdcall WinMain(HINSTANCE, HINSTANCE, char*, int)
{
	Program::Run();
	return 0;
}