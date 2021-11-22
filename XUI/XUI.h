#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

namespace XUI
{
	class Host;

	enum class ViewHandle : uint32;
	enum class LayoutHandle : uint32;
	enum class ElementHandle : uint32;

	struct HostUpdateResult
	{
		float32 nextUpdateDelay;
	};

	struct Input
	{

	};

	class ElementBase abstract : public XLib::NonCopyable
	{
		friend Host;

	private:

	private:
		ElementBase() = default;
		virtual ~ElementBase() = default;

	protected:
		virtual void update() {}

	public:

	};

	class DrawCommandsList : public XLib::NonCopyable
	{
		friend Host;

	private:

	public:
		DrawCommandsList() = default;
		~DrawCommandsList() = default;
	};

	class Host : public XLib::NonCopyable
	{
	private:


	public:
		Host() = default;
		~Host() = default;

		ViewHandle createView();
		void destroyView(ViewHandle viewHandle);

		ElementHandle createElement(LayoutHandle parentLayoutHandle);
		void destroyElement(ElementHandle elementHandle);

		HostUpdateResult update();

		void invalidateViewRegionForRedraw(ViewHandle viewHandle);
		void drawView(ViewHandle viewHandle, DrawCommandsList& drawCommands);

		LayoutHandle getViewRootLayout(ViewHandle viewHandle) const;
	};
}
