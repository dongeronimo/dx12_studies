#pragma once
#include "pch.h"
namespace common
{
	enum KeyCodes {
		W, A, S, D, Q, E, Esc, None
	};
	/// <summary>
	/// A window.
	/// </summary>
	class Window
	{
	public:
		static KeyCodes GetLastKey();
		/// <summary>
		/// Instantiates the window.
		/// The window won't be shown until you call Show()
		/// </summary>
		/// <param name="hInstance"></param>
		Window(HINSTANCE hInstance, 
			const std::wstring& className,
			const std::wstring& title);
		Window(HINSTANCE hInstance,
			const std::wstring& className,
			const std::wstring& title,
			int w, int h);
		~Window();
		void Show();
		void MainLoop();
		HWND Hwnd()const { return mHWND; }
		std::optional<std::function<void()>> mOnIdle;
		std::optional<std::function<void(int w, int h)>> mOnResize;
		std::optional<std::function<void()>> mOnCreate;
		friend LRESULT CALLBACK __WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	private:
		HWND mHWND;

	};

}

