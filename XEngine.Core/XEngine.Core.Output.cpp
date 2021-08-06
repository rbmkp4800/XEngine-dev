#include <Windows.h>

#include <XLib.Debug.h>
#include <XLib.System.Threading.h>
#include <XLib.System.Threading.Event.h>

#include "XEngine.Core.Output.h"

#include "XEngine.Core.Input.h"

// HID usage tables http://www.usb.org/developers/hidpage/Hut1_12v2.pdf (page 26)
#define HID_GENERIC_DESKTOP_USAGE_PAGE	0x01
#define HID_MOUSE_USAGE_ID				0x02
#define HID_KEYBOARD_USAGE_ID			0x06

#define WM_XENGINE_OUTPUT_DESTROY		(WM_USER + 1)
#define WM_XENGINE_CURSOR_STATE_CHANGE	(WM_USER + 2)

namespace XEngine::Core
{
	class InputProxy abstract final
	{
	public:
		static inline void OnMouseMove(sint16x2 delta)
			{ Input::OnMouseMove(delta); }
		static inline void OnMouseButton(XLib::MouseButton button, bool state)
			{ Input::OnMouseButton(button, state); }
		static inline void OnMouseWheel(float32 delta)
			{ Input::OnMouseWheel(delta); }
		static inline void OnKeyboard(XLib::VirtualKey key, bool state)
			{ Input::OnKeyboard(key, state); }
		static inline void OnCharacter(wchar character)
			{ Input::OnCharacter(character); }
		static inline void OnCloseRequest()
			{ Input::OnCloseRequest(); }
	};
}

using namespace XLib;
using namespace XEngine::Core;

static Thread dispatchThread;
static Event controlEvent;
static HWND hWnd = nullptr;

static CursorState cursorState = CursorState::Default;
static bool cursorIsHidden = false;

static bool cursorAbsoluteMovementInitialized = false;
static uint16 cursorLastAbsoluteX = 0;
static uint16 cursorLastAbsoluteY = 0;

static bool windowClassRegistered = false;
static bool rawInputRegistered = false;

static constexpr uint16x2 resolution = { 1440, 900 };
static constexpr wchar windowClassName[] = L"XEngine.OutputWindow";
static constexpr wchar windowTitle[] = L"XEngine";

// https://docs.microsoft.com/en-us/windows/desktop/menurc/wm-syscommand

static void ClipCursorToClientRect()
{
	RECT clipRect;
	GetClientRect(hWnd, &clipRect);
	ClientToScreen(hWnd, (POINT*) &clipRect.left);
	ClientToScreen(hWnd, (POINT*) &clipRect.right);
	ClipCursor(&clipRect);
}

static void ResetCursor()
{
	if (cursorIsHidden)
	{
		ShowCursor(TRUE);
		cursorIsHidden = false;
	}
	ClipCursor(nullptr);
}

static void ApplyCursorState()
{
	if (cursorState == CursorState::Default)
		ResetCursor();
	else if (cursorState == CursorState::Disabled)
	{
		if (!cursorIsHidden)
		{
			ShowCursor(FALSE);
			cursorIsHidden = true;
		}
		ClipCursorToClientRect();
	}
}

static void HandleRawInput(WPARAM wParam, LPARAM lParam)
{
	if (wParam != RIM_INPUT)
		return;

	RAWINPUT inputBuffer = {};
	UINT inputBufferSize = sizeof(RAWINPUT);

	UINT result = GetRawInputData(HRAWINPUT(lParam), RID_INPUT,
		&inputBuffer, &inputBufferSize, sizeof(RAWINPUTHEADER));

	if (result == -1)
		return;

	if (inputBuffer.header.dwType == RIM_TYPEMOUSE)
	{
		const auto& data = inputBuffer.data.mouse;

		if (data.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
			InputProxy::OnMouseButton(MouseButton::Left, true);
		if (data.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
			InputProxy::OnMouseButton(MouseButton::Left, false);

		if (data.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
			InputProxy::OnMouseButton(MouseButton::Middle, true);
		if (data.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
			InputProxy::OnMouseButton(MouseButton::Middle, false);

		if (data.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
			InputProxy::OnMouseButton(MouseButton::Right, true);
		if (data.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
			InputProxy::OnMouseButton(MouseButton::Right, false);

		if (data.usButtonFlags & RI_MOUSE_WHEEL)
			InputProxy::OnMouseWheel(float32(sint16(data.usButtonData)) / float32(WHEEL_DELTA));

		// TODO: Investigate further. Maybe add some logging during switch between modes
		if (data.usFlags & MOUSE_MOVE_ABSOLUTE)
		{
			// Absolute movement. Used in RDP/TeamViewer session

			uint16 currentAbsoluteX = 0;
			uint16 currentAbsoluteY = 0;

			if (data.usFlags & MOUSE_VIRTUAL_DESKTOP)
			{
				// http://www.petergiuntoli.com/parsing-wm_input-over-remote-desktop
				// For virtual desktop coords: [0 .. MaxDim] map to [0 .. 0xFFFF]

				// TODO: Handle virtual desktop size change properly
				const uint32 virtualDesktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
				const uint32 virtualDesktopHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

				currentAbsoluteX = uint16(data.lLastX * virtualDesktopWidth / 0xFFFF);
				currentAbsoluteY = uint16(data.lLastY * virtualDesktopHeight / 0xFFFF);
			}
			else
			{
				currentAbsoluteX = uint16(data.lLastX);
				currentAbsoluteY = uint16(data.lLastY);
			}

			if (cursorAbsoluteMovementInitialized)
			{
				const sint32 deltaX = sint32(currentAbsoluteX) - sint32(cursorLastAbsoluteX);
				const sint32 deltaY = sint32(currentAbsoluteY) - sint32(cursorLastAbsoluteY);

				if (deltaX || deltaY)
					InputProxy::OnMouseMove({ sint16(deltaX), sint16(deltaY) });
			}
			else
				cursorAbsoluteMovementInitialized = true;

			cursorLastAbsoluteX = currentAbsoluteX;
			cursorLastAbsoluteY = currentAbsoluteY;
		}
		else
		{
			// Relative movement

			if (data.lLastX || data.lLastY)
				InputProxy::OnMouseMove({ sint16(data.lLastX), sint16(data.lLastY) });

			// Assuming that absolute mode can be switched back to relative
			cursorAbsoluteMovementInitialized = false;
		}
	}
	else if (inputBuffer.header.dwType == RIM_TYPEKEYBOARD)
	{
		auto& data = inputBuffer.data.keyboard;

		InputProxy::OnKeyboard(VirtualKey(data.VKey), data.Flags == 0);
	}
}

static LRESULT __stdcall WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_XENGINE_OUTPUT_DESTROY:
			DestroyWindow(hWnd);
			break;

		case WM_XENGINE_CURSOR_STATE_CHANGE:
			cursorState = CursorState(wParam);
			ApplyCursorState();
			break;

		case WM_ACTIVATE:
			if (LOWORD(wParam) != WA_INACTIVE)
				ApplyCursorState();
			else
			{
				ResetCursor();

				cursorAbsoluteMovementInitialized = false;
			}
			break;

		case WM_SYSCOMMAND:
			if (cursorState == CursorState::Disabled)
			{
				switch (wParam & 0xFFF0)
				{
					case SC_KEYMENU:
					case SC_MOVE:
					case SC_SIZE:
						return 0;
				}
			}
			break;

		case WM_SIZE:
		case WM_MOVE:
			ApplyCursorState();
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_CLOSE:
			InputProxy::OnCloseRequest();
			break;

		case WM_INPUT:
			HandleRawInput(wParam, lParam);
			// DefWindowProc must be called to cleaunup after WM_INPUT
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

static uint32 __stdcall DispatchThreadMain(void*)
{
	RECT rect = { 0, 0, LONG(resolution.x), LONG(resolution.y) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

	hWnd = CreateWindow(windowClassName, windowTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

	controlEvent.set();

	ShowWindow(hWnd, SW_SHOW);

	MSG message = { 0 };
	while (GetMessage(&message, nullptr, 0, 0))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	hWnd = nullptr;

	return 0;
}

void Output_SetCursorState(CursorState state)
{
	PostMessage(hWnd, WM_XENGINE_CURSOR_STATE_CHANGE, WPARAM(state), 0);
}

void Output::Initialize()
{
	XASSERT(!dispatchThread.isInitialized(), "already initialized");

	// Initailize Raw Input
	if (!rawInputRegistered)
	{
		RAWINPUTDEVICE rawInputDevices[2] = {};

		rawInputDevices[0].usUsagePage = HID_GENERIC_DESKTOP_USAGE_PAGE;
		rawInputDevices[0].usUsage = HID_MOUSE_USAGE_ID;
		rawInputDevices[0].dwFlags = 0;
		rawInputDevices[0].hwndTarget = nullptr;

		rawInputDevices[1].usUsagePage = HID_GENERIC_DESKTOP_USAGE_PAGE;
		rawInputDevices[1].usUsage = HID_KEYBOARD_USAGE_ID;
		rawInputDevices[1].dwFlags = 0;
		rawInputDevices[1].hwndTarget = nullptr;

		BOOL result = RegisterRawInputDevices(rawInputDevices, 2, sizeof(RAWINPUTDEVICE));

		rawInputRegistered = true;
	}

	// Register window class
	if (!windowClassRegistered)
	{
		WNDCLASSEX wndClassEx = {};
		wndClassEx.cbSize = sizeof(WNDCLASSEX);
		wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
		wndClassEx.lpfnWndProc = WndProc;
		wndClassEx.cbClsExtra = 0;
		wndClassEx.cbWndExtra = 0;
		wndClassEx.hInstance = GetModuleHandle(nullptr);
		wndClassEx.hIcon = nullptr;
		wndClassEx.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wndClassEx.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
		wndClassEx.lpszMenuName = nullptr;
		wndClassEx.lpszClassName = windowClassName;
		wndClassEx.hIconSm = nullptr;

		RegisterClassEx(&wndClassEx);

		windowClassRegistered = true;
	}

	controlEvent.initialize(false);
	dispatchThread.create(&DispatchThreadMain);

	controlEvent.wait();
}

void Output::Destroy()
{
	PostMessage(hWnd, WM_XENGINE_OUTPUT_DESTROY, 0, 0);

	dispatchThread.wait();

	dispatchThread.destroy();
	controlEvent.destroy();
}

uint32 Output::GetViewCount() { return 1; }
uint16x2 Output::GetViewResolution(uint32 viewIndex) { return resolution; }
void* Output::GetViewWindowHandle(uint32 viewIndex) { return hWnd; }