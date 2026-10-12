#include "stdafx.h"
#include "Keyboard.hpp"
#include <Windows.h>
#include "Log.hpp"
#include <iostream>
#include "D3D/D3DHooks.hpp"

namespace Keyboard {
	/// Simply send ESC key as if pressed via the keyboard
	/// Used to avoid menus.
	/// </summary>
	void PressDownArrowKey()
	{
		SendKey(VK_DOWN);
	}

	/// <summary>
	/// Simply send ESC key as if pressed via the keyboard
	/// Used to avoid menus during startup.
	/// </summary>
	void SendEscapeKey()
	{
		SendKey(VK_ESCAPE);
	}

	/// <summary>
	/// Presses Enter. Normally used in a loop to skip most of the login dialog. "Fork in the toaster" method
	/// </summary>
	void AutoEnterGame()
	{
		SendKey(VK_RETURN);
	}

	/// <summary>
	/// Force a Steam screenshot. Requires the default "F12" screenshot key for Steam.
	/// </summary>
	void TakeScreenshot()
	{
		SendKey(VK_F12);
		LOG_INFO("Took screenshot" << std::endl);
	}

	void SendKey(unsigned int key)
	{
		const HWND gameWindow = D3DHooks::GetGameWindow();
		if (!gameWindow) return;

		PostMessage(gameWindow, WM_KEYDOWN, key, 0);
		Sleep(30);
		PostMessage(gameWindow, WM_KEYUP, key, 0);
	}
}
