#include <XLib.Program.h>

#include "GameSample1.h"

void XLib::Program::Run()
{
	GameSample1::Game game;
	XEngine::Core::Engine::Run(&game);
}