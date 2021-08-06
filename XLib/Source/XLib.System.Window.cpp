#include <Windows.h>
#include <WindowsX.h>

#include "XLib.System.Window.h"

using namespace XLib;
using namespace XLib::Internal;

CreationArgs creationArgs(uint16 x, uint16 y) {	return { x, y }; }
ResizingArgs resizingArgs(uint16 x, uint16 y) { return { x, y }; }
MouseState mouseState(WPARAM wParam, LPARAM lParam)
{
	return { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
		wParam & MK_LBUTTON ? true : false, wParam & MK_MBUTTON ? true : false, wParam & MK_RBUTTON ? true : false };
}

struct Internal::WindowInternal abstract final
{
	static LRESULT __stdcall WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		WindowBase *window = nullptr;
		if (message == WM_NCCREATE)
		{
			CREATESTRUCT *createStruct = (CREATESTRUCT*)lParam;
			window = (WindowBase*)createStruct->lpCreateParams;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, LONG_PTR(window));
		}
		else
		{
			window = (WindowBase*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		}

		switch (message)
		{
			case WM_CREATE:
			{
				window->handle = hWnd;
				RECT clientRect = { 0 };
				GetClientRect(hWnd, &clientRect);
				window->onCreate(creationArgs(uint16(clientRect.right - clientRect.left),
					uint16(clientRect.bottom - clientRect.top)));

				break;
			}

			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				BeginPaint(HWND(window->handle), &ps);
				window->onRedraw();
				EndPaint(HWND(window->handle), &ps);

				break;
			}

			case WM_DESTROY:
				window->onDestroy();
				break;

			case WM_SIZE:
				window->onResize(resizingArgs(LOWORD(lParam), HIWORD(lParam)));
				break;

			case WM_KEYDOWN:
				window->onKeyboard(VirtualKey(wParam), true);
				break;

			case WM_KEYUP:
				window->onKeyboard(VirtualKey(wParam), false);
				break;

			case WM_MOUSEMOVE:
				window->onMouseMove(mouseState(wParam, lParam));
				break;

			case WM_LBUTTONDOWN:
				window->onMouseButton(mouseState(wParam, lParam), MouseButton::Left, true);
				break;

			case WM_LBUTTONUP:
				window->onMouseButton(mouseState(wParam, lParam), MouseButton::Left, false);
				break;

			case WM_RBUTTONDOWN:
				window->onMouseButton(mouseState(wParam, lParam), MouseButton::Right, true);
				break;

			case WM_RBUTTONUP:
				window->onMouseButton(mouseState(wParam, lParam), MouseButton::Right, false);
				break;

			case WM_MOUSEWHEEL:
				window->onMouseWheel(mouseState(wParam, lParam),
					float32(GET_WHEEL_DELTA_WPARAM(wParam)) / float32(WHEEL_DELTA));
				break;

			case WM_CHAR:
				window->onCharacter(wchar(wParam));
				break;

			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
				break;
		}

		return 0;
	}
};

WindowBase::WindowBase() : handle(nullptr) {}
WindowBase::~WindowBase()
{
	if (handle)
	{
		destroy();
		handle = nullptr;
	}
}

void WindowBase::create(uint16 width, uint16 height, const wchar* title, bool visible)
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);

	static constexpr wchar windowClassName[] = L"XLib.Window";

	static bool windowClassRegistered = false;
	if (!windowClassRegistered)
	{
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WindowInternal::WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = windowClassName;
		wcex.hIconSm = nullptr;
		RegisterClassEx(&wcex);
	}

	RECT rect = { 0, 0, LONG(width), LONG(height) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

	CreateWindow(windowClassName, title, WS_OVERLAPPEDWINDOW | (visible ?  WS_VISIBLE : 0) | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, hInstance, this);
}
void WindowBase::show(bool state) { ShowWindow(HWND(handle), state ? SW_SHOW : SW_HIDE); }
void WindowBase::setFocus() { SetFocus(HWND(handle)); }
void WindowBase::destroy() { DestroyWindow(HWND(handle)); }
bool WindowBase::isOpened() { return IsWindow(HWND(handle)) ? true : false; }

void WindowBase::DispatchPending()
{
	MSG message = { 0 };
	while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}
void WindowBase::DispatchAll()
{
	MSG message = { 0 };
	while (GetMessage(&message, nullptr, 0, 0))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}