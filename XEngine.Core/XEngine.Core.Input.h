#pragma once

#include <XLib.Vectors.h>
#include <XLib.Containers.IntrusiveDoublyLinkedList.h>
#include <XLib.System.Window.h>

namespace XEngine::Core { class Input; }

namespace XEngine::Core
{
	enum class CursorState
	{
		Default = 0,
		Disabled,
		Hidden,
	};

	class InputHandler abstract : public XLib::NonCopyable
	{
		friend Input;

	private:
		XLib::IntrusiveDoublyLinkedListItemHook handlersListItemHook;

	protected:
		virtual void onMouseMove(sint16x2 delta) {}
		virtual void onMouseButton(XLib::MouseButton button, bool state) {}
		virtual void onMouseWheel(float32 delta) {}
		virtual void onKeyboard(XLib::VirtualKey key, bool state) {}
		virtual void onCharacter(wchar character) {}
		virtual void onCloseRequest() {}

	public:
		InputHandler() = default;
		~InputHandler();
	};

	class Input abstract final
	{
		friend class InputProxy;

	private:
		using HandlersList = XLib::IntrusiveDoublyLinkedList<
			InputHandler, &InputHandler::handlersListItemHook>;

		static HandlersList handlersList;

	private:
		static void OnMouseMove(sint16x2 delta);
		static void OnMouseButton(XLib::MouseButton button, bool state);
		static void OnMouseWheel(float32 delta);
		static void OnKeyboard(XLib::VirtualKey key, bool state);
		static void OnCharacter(wchar character);
		static void OnCloseRequest();

	public:
		static void AddHandler(InputHandler* handler);
		static void RemoveHandler(InputHandler* handler);

		static bool IsKeyDown(XLib::VirtualKey key);
		static bool IsMouseButtonDown(XLib::MouseButton button);

		static void SetCursorState(CursorState state);
	};
}