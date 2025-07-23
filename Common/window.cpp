#include "pch.h"
#include "window.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"


common::Window* _window = nullptr;

static common::KeyCodes gLastKey = common::KeyCodes::None;

LRESULT CALLBACK __WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	assert(_window);

	switch (uMsg)
	{	
		case WM_CREATE:
			if (_window->mOnCreate.has_value())
			{
				auto fn = _window->mOnCreate;
				(*fn)();
			}
			break;
		case WM_KEYDOWN:
			switch (wParam) {
			case 'A':
				gLastKey = common::KeyCodes::A;
				break;
			case 'W':
				gLastKey = common::KeyCodes::W;
				break;
			case 'S':
				gLastKey = common::KeyCodes::S;
				break;
			case 'D':
				gLastKey = common::KeyCodes::D;
				break;
			case 'Q':
				gLastKey = common::KeyCodes::Q;
				break;
			case 'E':
				gLastKey = common::KeyCodes::E;
				break;
			case VK_ESCAPE:
				gLastKey = common::KeyCodes::Esc;
				break;
			default:
				gLastKey = common::KeyCodes::None;
			}
			return 0;
		case WM_KEYUP:
			gLastKey = common::KeyCodes::None;
			break;
		case WM_SIZE:
			RECT rect; GetWindowRect(hwnd, &rect);
			if (_window->mOnResize.has_value())
			{
				int w = rect.right - rect.left;
				int h = rect.bottom - rect.top;
				auto fn = _window->mOnResize;
				(*fn)(w, h);
			}
				
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProc(hwnd,
		uMsg,
		wParam,
		lParam);
}
namespace common
{
	KeyCodes Window::GetLastKey()
	{
		return gLastKey;
	}
	Window::Window(HINSTANCE hInstance,
		const std::wstring& className,
		const std::wstring& title)
	{
		_window = this;
		WNDCLASSEX wc;
		wc.cbSize = sizeof(WNDCLASSEX);                // The size, in bytes, of this structure
		wc.style = 0;                                 // The class style(s)
		wc.lpfnWndProc = __WindowProc;                           // A pointer to the window procedure
		wc.cbClsExtra = 0;                                 // The number of extra bytes to allocate following the window-class structure.
		wc.cbWndExtra = 0;                                 // The number of extra bytes to allocate following the window instance.
		wc.hInstance = hInstance;                         // A handle to the instance that contains the window procedure for the class.
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);   // A handle to the class icon. This member must be a handle to an icon resource. If this member is NULL, the system provides a default icon.
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);       // A handle to the class cursor. This member must be a handle to a cursor resource. If this member is NULL, an application must explicitly set the cursor shape whenever the mouse moves into the application's window.
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);          // A handle to the class background brush.
		wc.lpszMenuName = NULL;                              // Pointer to a null-terminated character string that specifies the resource name of the class menu.
		wc.lpszClassName = className.c_str();                     // A string that identifies the window class.
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);   // A handle to a small icon that is associated with the window class.Create the window
		auto registerClassResult = RegisterClassEx(&wc);
		assert(registerClassResult);
		mHWND = CreateWindowEx(
			0,                      // Optional window styles.
			className.c_str(),          // Window class
			title.c_str(),            // Window text
			WS_POPUPWINDOW,    // Window style
			CW_USEDEFAULT,          // Position X
			CW_USEDEFAULT,          // Position Y
			800,                    // Width
			600,                    // Height
			NULL,                   // Parent window
			NULL,                   // Menu
			hInstance,              // Instance handle
			NULL                    // Additional application data
		);
		assert(mHWND != NULL);
	}
	Window::Window(HINSTANCE hInstance, const std::wstring& className, const std::wstring& title, int w, int h)
	{
		_window = this;
		WNDCLASSEX wc;
		wc.cbSize = sizeof(WNDCLASSEX);                // The size, in bytes, of this structure
		wc.style = 0;                                 // The class style(s)
		wc.lpfnWndProc = __WindowProc;                           // A pointer to the window procedure
		wc.cbClsExtra = 0;                                 // The number of extra bytes to allocate following the window-class structure.
		wc.cbWndExtra = 0;                                 // The number of extra bytes to allocate following the window instance.
		wc.hInstance = hInstance;                         // A handle to the instance that contains the window procedure for the class.
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);   // A handle to the class icon. This member must be a handle to an icon resource. If this member is NULL, the system provides a default icon.
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);       // A handle to the class cursor. This member must be a handle to a cursor resource. If this member is NULL, an application must explicitly set the cursor shape whenever the mouse moves into the application's window.
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);          // A handle to the class background brush.
		wc.lpszMenuName = NULL;                              // Pointer to a null-terminated character string that specifies the resource name of the class menu.
		wc.lpszClassName = className.c_str();                     // A string that identifies the window class.
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);   // A handle to a small icon that is associated with the window class.Create the window
		auto registerClassResult = RegisterClassEx(&wc);
		assert(registerClassResult);
		mHWND = CreateWindowEx(
			0,                      // Optional window styles.
			className.c_str(),          // Window class
			title.c_str(),            // Window text
			WS_POPUPWINDOW,    // Window style
			CW_USEDEFAULT,          // Position X
			CW_USEDEFAULT,          // Position Y
			w,                    // Width
			h,                    // Height
			NULL,                   // Parent window
			NULL,                   // Menu
			hInstance,              // Instance handle
			NULL                    // Additional application data
		);
		assert(mHWND != NULL);
	}
	Window::~Window()
	{
		DestroyWindow(mHWND);
	}
	void Window::Show()
	{
		ShowWindow(mHWND, SW_SHOW);
		UpdateWindow(mHWND);
	}
	void Window::MainLoop()
	{
		MSG msg;
		ZeroMemory(&msg, sizeof(MSG));
		while (true)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
					break;

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				if (mOnIdle) (*mOnIdle)(); //call onIdle if i have an event handler

			}
		}
	}
}