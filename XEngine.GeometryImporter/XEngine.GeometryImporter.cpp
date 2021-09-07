#include <XLib.h>
#include <XLib.System.File.h>

using namespace XLib;

int main()
{
	File file;
	file.open("", FileAccessMode::Read);

	file.close();

	return 0;
}