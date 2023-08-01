#include <Windows.h>

#include "XEngine.System.h"

// HID usage tables http://www.usb.org/developers/hidpage/Hut1_12v2.pdf (page 26)
#define HID_GENERIC_DESKTOP_USAGE_PAGE	0x01
#define HID_MOUSE_USAGE_ID				0x02
#define HID_KEYBOARD_USAGE_ID			0x06

using namespace XLib;
using namespace XEngine::System;

static const wchar XEngineWindowClassName[] = L"XEngineOutputWindow";
static XEngine::System::Internal::InputHandlersList GlobalInputHandlersList;
static bool KeyboardState[0x100] = {};

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

	static sint32 mouseLastAbsoluteX = 0;
	static sint32 mouseLastAbsoluteY = 0;
	static bool mouseLastAbsoluteStateSet = false;

	if (inputBuffer.header.dwType == RIM_TYPEMOUSE)
	{
		const auto& data = inputBuffer.data.mouse;

		if (data.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
			GlobalInputHandlersList.onMouseButton(MouseButton::Left, true);
		if (data.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
			GlobalInputHandlersList.onMouseButton(MouseButton::Left, false);

		if (data.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
			GlobalInputHandlersList.onMouseButton(MouseButton::Middle, true);
		if (data.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
			GlobalInputHandlersList.onMouseButton(MouseButton::Middle, false);

		if (data.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
			GlobalInputHandlersList.onMouseButton(MouseButton::Right, true);
		if (data.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
			GlobalInputHandlersList.onMouseButton(MouseButton::Right, false);

		if (data.usButtonFlags & RI_MOUSE_WHEEL)
			GlobalInputHandlersList.onScroll(float32(sint16(data.usButtonData)) / float32(WHEEL_DELTA));

		// TODO: Investigate further. Maybe add some logging during switch between modes.
		if (data.usFlags & MOUSE_MOVE_ABSOLUTE)
		{
			// Absolute movement. Used in RDP/TeamViewer session.

			uint16 currentAbsoluteX = 0;
			uint16 currentAbsoluteY = 0;

			if (data.usFlags & MOUSE_VIRTUAL_DESKTOP)
			{
				// http://www.petergiuntoli.com/parsing-wm_input-over-remote-desktop
				// For virtual desktop coords: [0 .. MaxDim] map to [0 .. 0xFFFF]

				// TODO: Handle virtual desktop size change properly and do not call 'GetSystemMetrics' each time.
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

			if (mouseLastAbsoluteStateSet)
			{
				const sint32 deltaX = sint32(currentAbsoluteX) - sint32(mouseLastAbsoluteX);
				const sint32 deltaY = sint32(currentAbsoluteY) - sint32(mouseLastAbsoluteY);

				if (deltaX || deltaY)
					GlobalInputHandlersList.onMouseMove(deltaX, deltaY);
			}

			mouseLastAbsoluteX = currentAbsoluteX;
			mouseLastAbsoluteY = currentAbsoluteY;
			mouseLastAbsoluteStateSet = true;
		}
		else
		{
			// Relative movement.

			if (data.lLastX || data.lLastY)
				GlobalInputHandlersList.onMouseMove(data.lLastX, data.lLastY);

			// Assuming that absolute mode can be switched back to relative.
			mouseLastAbsoluteStateSet = false;
		}
	}
	else if (inputBuffer.header.dwType == RIM_TYPEKEYBOARD)
	{
		auto& data = inputBuffer.data.keyboard;

		const bool keyState = data.Flags == RI_KEY_MAKE;

		if (data.VKey < countof(KeyboardState))
			KeyboardState[data.VKey] = keyState;

		GlobalInputHandlersList.onKeyboard(KeyCode(data.VKey), keyState);
	}
}

static LRESULT __stdcall WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INPUT:
			HandleRawInput(wParam, lParam);
			return DefWindowProc(hWnd, message, wParam, lParam); // NOTE: 'DefWindowProc' must be called to cleaunup after WM_INPUT.

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

static void RegisterRawInput()
{
	static bool isRawInputRegistered = false;
	if (isRawInputRegistered)
		return;

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
	XAssert(result);

	isRawInputRegistered = true;
}

static void RegisterWindowClass()
{
	static bool isWindowClassRegistered = false;
	if (isWindowClassRegistered)
		return;

	WNDCLASSEX wndClassEx = {};
	wndClassEx.cbSize = sizeof(WNDCLASSEX);
	wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
	wndClassEx.lpfnWndProc = WndProc;
	wndClassEx.hInstance = GetModuleHandle(nullptr);
	wndClassEx.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wndClassEx.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);;
	wndClassEx.lpszClassName = XEngineWindowClassName;
	RegisterClassExW(&wndClassEx);

	isWindowClassRegistered = true;
}

InputHandler::~InputHandler()
{
	XAssertNotImplemented();
}

Window::~Window()
{
	XAssertNotImplemented();
}

void Window::create(uint32 width, uint32 height)
{
	RegisterWindowClass();
	RegisterRawInput();

	RECT rect = { 0, 0, LONG(width), LONG(height) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

	hWnd = CreateWindowW(XEngineWindowClassName, L"XEngine", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

	ShowWindow(HWND(hWnd), SW_SHOW);
}

void XEngine::System::RegisterInputHandler(InputHandler* inputHandler)
{
	GlobalInputHandlersList.registerHandler(inputHandler);
}

void XEngine::System::UnregisterInputHandler(InputHandler* inputHandler)
{
	GlobalInputHandlersList.unregisterHandler(inputHandler);
}

bool XEngine::System::IsKeyDown(KeyCode key)
{
	return uint32(key) < countof(KeyboardState) ? KeyboardState[uint32(key)] : false;
}

void XEngine::System::DispatchEvents()
{
	MSG message = { 0 };
	while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}
