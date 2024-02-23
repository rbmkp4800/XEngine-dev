#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: IN THE END I DECIDED THAT WE SHOULD HAVE SINGLE HOST. BUT WE WILL KEEP VISUAL/LOGIC SEPARATION INSIDE ELEMENT.

namespace XUI
{
	enum class RootHandle : uint16 { Zero = 0, };
	//enum class ElementHandle : uint32 { Zero = 0, }; // If we allow elements manipulation only in controls context, then for external user element handle means nothing. He can operate only via controls.
	// Also user can't create new controls externally (apart from root), so the only way it can obtain one is from another control.
	// User can't do anything with control as an element (even destroy), but he can interact with controls internal state.

	enum class GenericControlHandle : uint32 { Zero = 0, };
	// ControlHandle<ControlType>;

	// NOTE: For some reason it looks like we can create elements only from controls.
	// It seems to have no sence to do it another way.
	// Examples when I need to create elements not from control??

	// When I poke control from some other domain (just calling random method and changing state of logic) this approach will ban instant changes to elements,
	// as technically this is not event/message and there is no control context to do elements tree changes. What should I do in this situation?

	// NOTE: If we move creation destruction to controls, then we know what control "owns" each element.
	// So control can't kill element owned by another control. Do we need this mechanic?
	// This also will mean that control can't insert new node into subree of another control.

	// What am I trying to achieve when constraining node creation/destruction to control context? What is real problem I am trying to solve?

	// Kind of problem: What if I am retarded and I am doing some changes to elements after `updateLayout` and before `render`?
	// Same issue is about multithreading...

	// So the problem is that I am trying to constrain elements tree modification to very specific point in time.
	// Same way as iced does `view` call and tree can't be changed externally.

	// One option to do it properly is to consider external messages as inputs.
	// For now I pass inputs array as an argument to `dispatchEvents` and it contains only mouse/keyboard.
	// I can also put my custom messages there. Accumulate them during frame and then dispatch.

	// BTW: We can consider doing mesage accumulation inside `Host`. We just post message anytime we want and do not think about queue management.

	// I think we still can work with controls via C++ methods, we just need to sync our changes properly via these messages.
	// So we can modify control state and then send message like `state_updated` so it can resync all the elements with internal state.

	// !!! BIG FUCKING PROBLEM !!!
	// If I will use control logic to put blinking carret in correct location inside text-input, then it means that I need to use results of layouting inside control logic.
	// Apart from carret there is also example when I spawn some controls with auto-completion and stuff near the carret... Their location depends on how text is layouted...

	// Probably we should not constrain operations with elements to dispatch only. I do not see real issues that we are trying to solve this way. Only retarded shit.

	class Host : public XLib::NonCopyable
	{
	public:
		Host() = default;
		~Host() = default;

		//template <typename ControlType>
		RootHandle createRoot();
		void destroyRoot(RootHandle root);

		void getRootControl();
		void resizeRoot(RootHandle root);

		// These should be available only in control context.
		//ElementHandle createElement();
		//ElementHandle createControlElement();
		//void destroyElement();

		// Do we need this? NOPE!
		//ElementHandle getRootElement(RootHandle root);

		// Keyboard, mouse, custom user stuff.
		// Messages are only sent to controls by definition. Not to elements.
		void postMessage(GenericControlHandle control, /*message code*/);

		// Are there any cases when we do not call all of there consequtively each frame?
		// probably `dispatchMessages` and `updateLayout` can be single `update` call.
		void dispatchMessages(); // Return maximum delay to run next dispatch.
		void updateLayout();

		void render();
	};
}

#if 0

namespace XUI
{
	// TODO: `VisualElementHandle` or `VisualNodeHandle`

	enum class VisualElementHandle : uint32 { Zero = 0, };
	enum class VisualRootHandle : uint16 { Zero = 0, };

	template <typename ControlType>
	class ControlHandle
	{
	private:
		uint32 handleValue;

	public:

	};

	class VisualHost : public XLib::NonCopyable
	{
	private:


	public:
		VisualHost() = default;
		~VisualHost() = default;

		VisualRootHandle createRoot();
		void destroyRoot(VisualRootHandle root);

		VisualElementHandle createElement(VisualElementHandle parent);
		void destroyElement(VisualElementHandle element);

		template <typename ControlType>
		ControlHandle<ControlType> attachControlToElement(VisualElementHandle element); // or we should have only `createControlElement<T>` ?

		VisualElementHandle getRootElement(VisualRootHandle root) const;
		void resizeRoot(VisualRootHandle root, uint16x2 size);

		void update();
		void render();

		void queryTriggersAtPosition(VisualRootHandle root, sint16x2 position) const;
	};

	class ControlsHost : public XLib::NonCopyable
	{
	private:




	public:
		ControlsHost() = default;
		~ControlsHost() = default;

		template <typename ControlType>
		ControlType* resolveHandle(ControlHandle<ControlType> handle);

		void dispatch();
	};

	class BaseControl abstract : public XLib::NonCopyable
	{
	private:

	private:
		BaseControl() = default;
		~BaseControl() = default;

	protected:
		virtual void initialize() = 0;
		virtual void processMessage() = 0;
	};

	class ButtonControl : public BaseControl
	{
	private:
		VisualElementHandle visualBlock;
		VisualElementHandle visualText;

	protected:
		virtual void initialize() override;
		virtual void processMessage() override;

	public:
		ButtonControl() = default;
		~ButtonControl();

		void create();
		void destroy();
	};

	class TextInputControl : public BaseControl
	{
	private:

	protected:
		virtual void processMessage() override;


	};
}

#endif
