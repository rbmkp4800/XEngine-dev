#include <Windows.h>
#include <stdio.h>

#include "XLib.Debug.h"

using namespace XLib;

//void Debug::Log(const char* message)
//{
//	printf("%s\n", message);
//	OutputDebugStringA(message);
//	OutputDebugStringA("\n");
//}
//
//void Debug::Crash(const char* message)
//{
//	printf("[ERROR] %s\n", message);
//	OutputDebugStringA("[ERROR] ");
//	OutputDebugStringA(message);
//	OutputDebugStringA("\n");
//	throw;
//}
//
//void Debug::Warning(const char* message)
//{
//	printf("[WARNING] %s\n", message);
//	OutputDebugStringA("[WARNING] ");
//	OutputDebugStringA(message);
//	OutputDebugStringA("\n");
//}
//
//void Debug::LogLastSystemError(const char* location)
//{
//	char message[128] = {};
//	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
//		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message, sizeof(message), nullptr);
//	OutputDebugStringA("[SYSTEM ERROR] ");
//	OutputDebugStringA(location);
//	OutputDebugStringA(message);
//	printf("[SYSTEM ERROR] %s%s", location, message);
//}