#pragma once

//#define DbgMsgHead __FUNCTION__##"(): "
//#define DbgMsgFmt(message) (DbgMsgHead message)
//#define SysErrorDbgMsgFmt DbgMsgHead

#ifdef _DEBUG

#define XASSERT(cond, message)	\
if (!(cond)) {					\
	throw;						\
}

#else

#define XASSERT(cond, ...)

#endif

#define XFATAL(message)	throw;

namespace XLib
{
	/*struct Debug abstract final
	{
		static void Log(const char* message);
		static void Crash(const char* message);
		static void Warning(const char* message);
		static inline void CrashCondition(bool condition, const char* message) { if (condition) Crash(message); }
		static inline void WarningCondition(bool condition, const char* message) { if (condition) Warning(message); }

#ifdef _DEBUG
		static inline void CrashConditionOnDebug(bool condition, const char* message) { CrashCondition(condition, message); }
#else
		static inline void CrashConditionOnDebug(bool condition, const char* message) {}
#endif

		static void LogLastSystemError(const char* location);
	};*/

/*

#pragma once

#include "XLib.StringWriter.h"

struct Debug abstract final
{
private:
	static constexpr uint32 maxMessageLength = 256;

public:
	template <typename ... Types>
	static inline void Log(Types ... args)
	{
		char message[maxMessageLength];
		StringWriter writer(message);
		writer.put(args ...);
		Log(message);
	}

	template<>
	static inline void Log(const char *message);

	template <typename ... Types> static inline void Crash(Types ... args);
	template <typename ... Types> static inline void Warning(Types ... args);
	template <typename ... Types> static inline void Assert(bool condition, Types ... args);

#ifdef _DEBUG
	static inline void AssertIfDebug(bool condition, const char* message) { if (!condition) Crash(message); }
#else
	static inline void AssertIfDebug(bool condition, const char* message) {}
#endif
};
*/
}