#include <XLib.System.Threading.Lock.h>

#include "XEngine.Core.Input.h"

using namespace XLib;
using namespace XEngine::Core;

Input::HandlersList Input::handlersList;
static Lock handlersListLock;

static constexpr uint32 mouseButtonIndexFirst = uint32(MouseButton::Left);
static constexpr uint32 mouseButtonIndexLast = uint32(MouseButton::Right);
static constexpr uint32 keyIndexFirst = 0x08;
static constexpr uint32 keyIndexLast = 0x91;

static bool mouseButtonsState[mouseButtonIndexLast - mouseButtonIndexFirst + 1] = {};
static bool keysState[keyIndexLast - keyIndexFirst + 1] = {};

extern void Output_SetCursorState(CursorState state);

void Input::AddHandler(InputHandler* handler)
{
	ScopedLock lock(handlersListLock);
	if (!handler->handlersListItemHook.isHooked())
		handlersList.insert(handler);
}

void Input::RemoveHandler(InputHandler* handler)
{
	ScopedLock lock(handlersListLock);
	if (handler->handlersListItemHook.isHooked())
		handlersList.remove(handler);
}

bool Input::IsKeyDown(XLib::VirtualKey key)
{
	uint32 keyIndex = uint32(key) - keyIndexFirst;
	if (keyIndex < countof(keysState))
		return keysState[keyIndex];
	return false;
}

bool Input::IsMouseButtonDown(XLib::MouseButton button)
{
	uint32 buttonIndex = uint32(button) - mouseButtonIndexFirst;
	if (buttonIndex < countof(mouseButtonsState))
		return mouseButtonsState[buttonIndex];
	return false;
}

InputHandler::~InputHandler()
{
	Input::RemoveHandler(this);
}

void Input::OnMouseMove(sint16x2 delta)
{
	ScopedLock lock(handlersListLock);
	for (InputHandler& handler : handlersList)
		handler.onMouseMove(delta);
}
void Input::OnMouseButton(XLib::MouseButton button, bool state)
{
	uint32 buttonIndex = uint32(button) - mouseButtonIndexFirst;
	if (buttonIndex < countof(mouseButtonsState))
		mouseButtonsState[buttonIndex] = state;

	ScopedLock lock(handlersListLock);
	for (InputHandler& handler : handlersList)
		handler.onMouseButton(button, state);
}
void Input::OnMouseWheel(float32 delta)
{
	ScopedLock lock(handlersListLock);
	for (InputHandler& handler : handlersList)
		handler.onMouseWheel(delta);
}
void Input::OnKeyboard(XLib::VirtualKey key, bool state)
{
	uint32 keyIndex = uint32(key) - keyIndexFirst;
	if (keyIndex < countof(keysState))
		keysState[keyIndex] = state;

	ScopedLock lock(handlersListLock);
	for (InputHandler& handler : handlersList)
		handler.onKeyboard(key, state);
}
void Input::OnCharacter(wchar character)
{
	ScopedLock lock(handlersListLock);
	for (InputHandler& handler : handlersList)
		handler.onCharacter(character);
}
void Input::OnCloseRequest()
{
	ScopedLock lock(handlersListLock);
	for (InputHandler& handler : handlersList)
		handler.onCloseRequest();
}

void Input::SetCursorState(CursorState state) { Output_SetCursorState(state); }