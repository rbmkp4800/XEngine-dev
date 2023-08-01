#include <Windows.h>

#include "XEngine.System.h"

using namespace XEngine::System;

static const wchar XEngineWindowClassName[] = L"XEngineOutputWindow";

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
