#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

namespace XLib
{
	enum class VirtualKey : uint8
	{
		Escape = 0x1B,
		Backspace = 0x08,
		MouseLeftButton = 0x01,
		MouseMiddleButton = 0x04,
		MouseRightButton = 0x02,
		Left = 0x25,
		Up = 0x26,
		Right = 0x27,
		Down = 0x28,
		Space = 0x20,
		Delete = 0x2E,
	};
	enum class MouseButton : uint8
	{
		Left = 1,
		Middle = 2,
		Right = 3,
	};

	struct CreationArgs
	{
		uint16 width, height;
	};
	struct ResizingArgs
	{
		uint16 width, height;
	};
	struct MouseState
	{
		sint16 x, y;
		bool leftButton, middleButton, rightButton;
	};

	namespace Internal
	{
		struct WindowInternal;
	}

	class WindowBase : public NonCopyable
	{
		friend Internal::WindowInternal;

	private:
		void *handle;

	protected:
		virtual void onCreate(const CreationArgs& args) {}
		virtual void onRedraw() {}
		virtual void onDestroy() {}
		virtual void onResize(const ResizingArgs& args) {}
		virtual void onKeyboard(VirtualKey key, bool state) {}
		virtual void onMouseMove(const MouseState& mouseState) {}
		virtual void onMouseButton(const MouseState& mouseState, MouseButton button, bool state) {}
		virtual void onMouseWheel(const MouseState& mouseState, float32 delta) {}
		virtual void onCharacter(wchar character) {}

	public:
		WindowBase();
		~WindowBase();

		void create(uint16 width, uint16 height, const wchar* title, bool visible = true);
		void show(bool state);
		void setFocus();
		void destroy();
		bool isOpened();

		static void DispatchPending();
		static void DispatchAll();

		inline void* getHandle() const { return handle; }
	};
}