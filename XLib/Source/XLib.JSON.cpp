#include "XLib.JSON.h"

using namespace XLib;

enum class JSONStreamParser::State : uint8
{
	Default = 0,
	DoubleQuotedKey,
	UnquotedKey, // JSON5 extension
	DoubleQuotedValue,
	UnquotedValue, // number, bool, null

	// JSON5 extension: comments
	SingleLineComment,
	MultiLineComment,
	CommentOpeningSlashConsumed,
	MultiLineCommentClosingStarConsumed,
};

void JSONStreamParser::processPortion(const void* data, uint32 length)
{

}
