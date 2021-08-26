#include <XLib.h>
#include <XLib.JSON.h>
#include <XLib.System.File.h>

using namespace XLib;

class GLTFxJSONParserHandler final : public JSONStreamParserHandlerBase
{
private:
	enum class State : uint8
	{
		Start = 0,

	};


private:
	virtual bool onKeyString(const char* str, uint32 length, bool truncated) override;

	virtual bool onValueObjectBegin() override;
	virtual bool onValueObjectEnd() override;
	virtual bool onValueArrayBegin() override;
	virtual bool onValueArrayEnd() override;

	virtual bool onValueString(const char* str, uint32 length, bool truncated) override;
	virtual bool onValueNumber(float64 value) override;
	virtual bool onValueBool(bool value) override;
	virtual bool onValueNull() override;

	//virtual bool onStringContinuation(const char* str, uint32 length, bool complete) = 0;

public:
	GLTFxJSONParserHandler() = default;
	~GLTFxJSONParserHandler() = default;
};

int main()
{
	GLTFxJSONParserHandler handler;
	JSONStreamParser jsonParser(handler);

	File file;
	file.open("", FileAccessMode::Read);

	byte fileReadBuffer[0x4000];

	for (;;)
	{
		uint32 readSize = 0;
		file.read(fileReadBuffer, sizeof(fileReadBuffer), readSize);
		jsonParser.processPortion(fileReadBuffer, readSize);
	}

	file.close();

	return 0;
}