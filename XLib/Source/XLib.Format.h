#pragma once

#include "XLib.h"

namespace XLib
{
	template <typename StreamReaderT, typename ... FmtArgsT>
	inline void FormatRead(StreamReaderT& reader, FmtArgsT ... fmtArgs);

	template <typename StreamWriterT, typename ... FmtArgsT>
	inline void FormatWrite(StreamWriterT& writer, FmtArgsT ... fmtArgs);

	namespace Fmt
	{
		class RdToNL
		{

		};

		class RdWS
		{
		public:
			RdWS(bool atLeastOne = false);
			~RdWS() = default;
		};

		class RdS32
		{
		private:
			sint32& result;

		public:
			RdS32(sint32& result);
			~RdS32() = default;

			template <typename Reader>
			static bool Read(Reader& reader, RdS32& result);
		};

		template <typename ResultStringWriter>
		class RdStr
		{
		private:
			ResultStringWriter& resultWriter;

		public:
			RdStr(ResultStringWriter& resultWriter);
			~RdStr() = default;
		};
	}
}
