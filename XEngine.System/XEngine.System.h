#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Containers.DoublyLinkedList.h>

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
	
	namespace Internal { class InputHandlersList; }

	class InputHandler abstract : public XLib::NonCopyable
	{
		friend Internal::InputHandlersList;

	private:
		XLib::IntrusiveDoublyLinkedListItemHook handlersListItemHook;

	protected:
		virtual void onMouseMove(sint32 deltaX, sint32 deltaY) {}
		virtual void onMouseButton(MouseButton button, bool state) {}
		virtual void onScroll(float32 delta) {}
		virtual void onKeyboard(KeyCode key, bool state) {}
		virtual void onCharacter(uint32 characterUTF32) {}
		virtual void onCloseRequest() {}

	public:
		InputHandler() = default;
		~InputHandler();

	private:
		using HandlersList = XLib::IntrusiveDoublyLinkedList<InputHandler, &InputHandler::handlersListItemHook>;
		static HandlersList handlersList;
	};

	class Window : public XLib::NonCopyable
	{
	private:
		void* hWnd = nullptr;

	public:
		Window() = default;
		~Window();

		void create(uint32 winth, uint32 heigth);
		void destroy();

		inline void* getHandle() const { return hWnd; }
	};

	void RegisterInputHandler(InputHandler* inputHandler);
	void UnregisterInputHandler(InputHandler* inputHandler);
	// RegisterInputDeviceChangeHanlder
	// UnregisterInputDeviceChangeHandler

	bool IsKeyDown(KeyCode key);
	bool IsMouseButtonDown(MouseButton mouseButton);
	// GetCursorPosition()

	void SetCursorEnabled(bool enabled);
	void SetCursorVisible(const Window& window, bool visible);

	void DispatchEvents();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal ////////////////////////////////////////////////////////////////////////////////////////

namespace XEngine::System::Internal
{
	class InputHandlersList
	{
	private:
		XLib::IntrusiveDoublyLinkedList<InputHandler, &InputHandler::handlersListItemHook> handlersList;

	public:
		InputHandlersList() = default;
		~InputHandlersList() = default;

		inline void registerHandler(InputHandler* handler)
		{
			handlersList.pushBack(handler);
		}
		void unregisterHandler(InputHandler* handler)
		{
			handlersList.remove(handler);
		}

		void onMouseMove(sint32 deltaX, sint32 deltaY)
		{
			for (InputHandler& handler : handlersList)
				handler.onMouseMove(deltaX, deltaY);
		}
		void onMouseButton(MouseButton button, bool state)
		{
			for (InputHandler& handler : handlersList)
				handler.onMouseButton(button, state);
		}
		void onScroll(float32 delta)
		{
			for (InputHandler& handler : handlersList)
				handler.onScroll(delta);
		}
		void onKeyboard(KeyCode key, bool state)
		{
			for (InputHandler& handler : handlersList)
				handler.onKeyboard(key, state);
		}
		void onCharacter(uint32 characterUTF32)
		{
			for (InputHandler& handler : handlersList)
				handler.onCharacter(characterUTF32);
		}
		void onCloseRequest()
		{
			for (InputHandler& handler : handlersList)
				handler.onCloseRequest();
		}
	};
}
