#include <Windows.h>

#include "XEngine.System.h"

// HID usage tables http://www.usb.org/developers/hidpage/Hut1_12v2.pdf (page 26)
#define HID_GENERIC_DESKTOP_USAGE_PAGE	0x01
#define HID_MOUSE_USAGE_ID				0x02
#define HID_KEYBOARD_USAGE_ID			0x06

using namespace XLib;
using namespace XEngine::System;

static const wchar XEngineWindowClassName[] = L"XEngineOutputWindow";

static XEngine::System::Internal::InputHandlersList inputHandlersList;

static LRESULT __stdcall WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
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

	RECT rect = { 0, 0, LONG(width), LONG(height) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

	hWnd = CreateWindowW(XEngineWindowClassName, L"XEngine", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

	ShowWindow(HWND(hWnd), SW_SHOW);
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

void XEngine::System::RegisterInputHandler(InputHandler* inputHandler)
{
	inputHandlersList.list.pushBack(inputHandler);
}

void XEngine::System::UnregisterInputHandler(InputHandler* inputHandler)
{
	XAssertNotImplemented();
}
