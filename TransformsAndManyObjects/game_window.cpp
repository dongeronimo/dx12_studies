#include "pch.h"
#include "game_window.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

transforms::Window* _window = nullptr;

static transforms::KeyCodes gLastKey = transforms::KeyCodes::None;

LRESULT CALLBACK __WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	assert(_window);
	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
		return true;
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
			gLastKey = transforms::KeyCodes::A;
			break;
		case 'W':
			gLastKey = transforms::KeyCodes::W;
			break;
		case 'S':
			gLastKey = transforms::KeyCodes::S;
			break;
		case 'D':
			gLastKey = transforms::KeyCodes::D;
			break;
		case 'Q':
			gLastKey = transforms::KeyCodes::Q;
			break;
		case 'E':
			gLastKey = transforms::KeyCodes::E;
			break;
		case VK_ESCAPE:
			gLastKey = transforms::KeyCodes::Esc;
			break;
		default:
			gLastKey = transforms::KeyCodes::None;
		}
		return 0;
	case WM_KEYUP:
		gLastKey = transforms::KeyCodes::None;
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
namespace transforms
{
	KeyCodes Window::GetLastKey()
	{
		return gLastKey;
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

		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		int x = 1;
		int y = 1;

		mHWND = CreateWindowEx(
			0,                      // Optional window styles.
			className.c_str(),          // Window class
			title.c_str(),            // Window text
			WS_POPUPWINDOW,    // Window style
			x,          // Position X
			y,          // Position Y
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
		SetForegroundWindow(mHWND); // Bring to front
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