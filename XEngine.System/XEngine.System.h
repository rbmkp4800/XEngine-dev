#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

namespace XEngine::System
{
	enum class MouseButton : uint8
	{
		Undefined = 0,
		Left = 1,
		Right = 2,
		Middle = 3,
	};

	enum class KeyCode : uint8
	{
		Undefined	= 0x00,
		Backspace	= 0x08,
		Tab			= 0x09,
		Enter		= 0x0D,
		Escape		= 0x1B,
		Space		= 0x20,
		Left		= 0x25,
		Up			= 0x26,
		Right		= 0x27,
		Down		= 0x28,
		Delete		= 0x2E,
	};

	class InputHandler abstract : public XLib::NonCopyable
	{
	private:
		//XLib::IntrusiveDoublyLinkedListItemHook handlersListItemHook;

	protected:
		virtual void onMouseMove(sint32 xDelta, sint32 yDelta) {}
		virtual void onMouseButton(MouseButton button, bool state) {}
		virtual void onScroll(float32 xDelta, float32 yDelta) {}
		virtual void onKeyboard(KeyCode key, bool state) {}
		virtual void onCharacter(uint32 characterUTF32) {}
		virtual void onCloseRequest() {}

	public:
		InputHandler() = default;
		~InputHandler();
	};

	// Probably this should be 'OutputSurfaceHandle'?
	class OutputSurface : public XLib::NonCopyable
	{

	};

	void RegisterInputHandler(InputHandler* inputHandler);
	void UnregisterInputHandler(InputHandler* inputHandler);
	// RegisterInputDeviceChangeHanlder
	// UnregisterInputDeviceChangeHandler
	// RegisterGraphicsHardwareChangeHandler
	// UnregisterGraphicsHardwareChangeHandler

	void DispatchEvents();

	bool IsKeyDown(KeyCode key);
	bool IsMouseButtonDown(MouseButton mouseButton);
	// GetCursorPosition()

	void SetCursorEnabled(bool enabled);
	void SetCursorVisible(const OutputSurface& windowSurface, bool visible);


	void CreateWindowOutputSurface(uint32 width, uint32 height, OutputSurface& surface);
	// CreateDisplayOutputSurface
	// CreateHMDOutputSurface
	void DestroyOutputSurface(OutputSurface& surface);

	bool IsOutputSurfaceInFocus(const OutputSurface& surface);

	// EnumerateGPUs
	// EnumerateDisplays
	// EnumerateHMDs
}
