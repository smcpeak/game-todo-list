// game-todo-list.cc
// Program to maintain a TODO list while playing a game in another window.

// See license.txt for copyright and terms of use.

#include "game-todo-list.h"            // this module

#include "winapi-util.h"               // WIDE_STRINGIZE, SELECT_RESTORE_OBJECT, GET_AND_RELEASE_HDC

#include <windows.h>                   // Windows API

#include <cassert>                     // assert
#include <cstdlib>                     // std::{atoi, getenv}
#include <iostream>                    // std::{wcerr, flush}
#include <sstream>                     // std::wostringstream


// Level of diagnostics to print.
//
//   1: API call failures.
//
//   2: Information about messages, etc., of low volume.
//
//   3: Higher-volume messages, e.g., relating to mouse movement.
//
// The default value is not used, as `wWinMain` overwrites it.
//
int g_tracingLevel = 1;


// Write a diagnostic message.
#define TRACE(level, msg)           \
  if (g_tracingLevel >= (level)) {  \
    std::wcerr << msg << std::endl; \
  }

#define TRACE1(msg) TRACE(1, msg)
#define TRACE2(msg) TRACE(2, msg)
#define TRACE3(msg) TRACE(3, msg)

// Add the value of `expr` to a chain of outputs using `<<`.
#define TRVAL(expr) L" " WIDE_STRINGIZE(expr) L"=" << (expr)


// Identifiers for the registered hotkeys.
static int const HOTKEY_ID_F5 = 1;
static int const HOTKEY_ID_UP = 2;


GTLMainWindow::GTLMainWindow()
{}


// Based on
// https://learn.microsoft.com/en-us/windows/win32/gdi/capturing-an-image.
void GTLMainWindow::captureScreen()
{
  GET_AND_RELEASE_HDC(hdcScreen, NULL);
  GET_AND_RELEASE_HDC(hdcWindow, m_hwnd);

  // This cannot use `GET_AND_RELEASE_HDC` because this DC must be
  // destroyed using `DeleteObject`, not `ReleaseDC` (!).
  HDC hdcMemDC;
  CALL_HANDLE_WINAPI(hdcMemDC, CreateCompatibleDC, hdcWindow);
  GDIObjectDeleter hdcMemDC_deleter(hdcMemDC);

  // Region of our window to fill with the screenshot.
  RECT rcClient;
  CALL_BOOL_WINAPI(GetClientRect, m_hwnd, &rcClient);

  // Create a compatible bitmap from the Window DC.
  HBITMAP hbmScreenshot;
  CALL_HANDLE_WINAPI(hbmScreenshot, CreateCompatibleBitmap,
    hdcWindow,
    rcClient.right - rcClient.left,
    rcClient.bottom - rcClient.top);
  GDIObjectDeleter hbmScreenshot_deleter(hbmScreenshot);

  // Select the compatible bitmap into the compatible memory DC.
  SELECT_RESTORE_OBJECT(hdcMemDC, hbmScreenshot);

  // Docs claim: "This is the best stretch mode."  This function does
  // not have a sensible way to signal errors, so I do not check.
  SetStretchBltMode(hdcMemDC, HALFTONE);

  // Screenshot with result going to the Memory DC.
  CALL_BOOL_WINAPI(StretchBlt,
    hdcMemDC,                          // hdcDest
    0, 0,                              // xDest, yDest
    rcClient.right,                    // wDest
    rcClient.bottom,                   // hDest
    hdcScreen,                         // hdcSrc
    0, 0,                              // xSrc, ySrc
    GetSystemMetrics(SM_CXSCREEN),     // wSrc
    GetSystemMetrics(SM_CYSCREEN),     // hSrc
    SRCCOPY);                          // rop

  // Draw that on the window too.
  CALL_BOOL_WINAPI(BitBlt,
    hdcWindow,                         // hdcDest
    0, 0,                              // xDest, yDest
    rcClient.right,                    // wDest
    rcClient.bottom,                   // hDest
    hdcMemDC,                          // hdcSrc
    0, 0,                              // xSrc, ySrc
    SRCCOPY);                          // rop
}


void GTLMainWindow::onPaint()
{
  PAINTSTRUCT ps;
  HDC hdc;
  CALL_HANDLE_WINAPI(hdc, BeginPaint, m_hwnd, &ps);

  // Open a scope so selected objects can be restored at scope exit.
  {
    FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));

    HFONT hFont = (HFONT)GetStockObject(SYSTEM_FONT);
    SELECT_RESTORE_OBJECT(hdc, hFont);

    CALL_BOOL_WINAPI(TextOut, hdc, 10, 10, L"Sample text", 11);
  }

  EndPaint(m_hwnd, &ps);
}


void GTLMainWindow::onHotKey(WPARAM id, WPARAM fsModifiers, WPARAM vk)
{
  TRACE2(L"hotkey:"
         " id=" << id <<
         " fsModifiers=" << fsModifiers <<
         " vk=" << vk);

  if (id == HOTKEY_ID_F5) {
    captureScreen();
  }
}


bool GTLMainWindow::onKeyDown(WPARAM wParam, LPARAM lParam)
{
  TRACE2(L"onKeyDown:" << std::hex <<
         TRVAL(wParam) << TRVAL(lParam) << std::dec);

  switch (wParam) {
    case 'Q':
      // Q to quit.
      TRACE2(L"Saw Q keypress.");
      PostMessage(m_hwnd, WM_CLOSE, 0, 0);
      return true;
  }

  // Not handled.
  return false;
}


LRESULT CALLBACK GTLMainWindow::handleMessage(
  UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg) {
    case WM_CREATE:
      // Set the window icon.
      {
        HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
        SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
      }

      // Register the hotkeys.
      CALL_BOOL_WINAPI(RegisterHotKey,
        m_hwnd,
        HOTKEY_ID_F5,        // id
        0,                   // fsModifiers
        VK_F5);              // vk
      CALL_BOOL_WINAPI(RegisterHotKey,
        m_hwnd,
        HOTKEY_ID_UP,        // id
        0,                   // fsModifiers
        VK_UP);              // vk

      return 0;

    case WM_DESTROY:
      TRACE2(L"received WM_DESTROY");

      CALL_BOOL_WINAPI(UnregisterHotKey,
        m_hwnd,
        HOTKEY_ID_F5);
      CALL_BOOL_WINAPI(UnregisterHotKey,
        m_hwnd,
        HOTKEY_ID_UP);

      PostQuitMessage(0);
      return 0;

    case WM_PAINT:
      onPaint();
      return 0;

    case WM_HOTKEY:
      onHotKey(wParam, LOWORD(lParam), HIWORD(lParam));
      return 0;

    case WM_KEYDOWN:
      if (onKeyDown(wParam, lParam)) {
        // Handled.
        return 0;
      }
      break;
  }

  return BaseWindow::handleMessage(uMsg, wParam, lParam);
}


// If `envvar` is set, return its value as an integer.  Otherwise return
// `defaultValue`.
static int envIntOr(char const *envvar, int defaultValue)
{
  if (char const *value = std::getenv(envvar)) {
    return std::atoi(value);
  }
  else {
    return defaultValue;
  }
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
  // Elsewhere, I rely on the assumption that I can get the module
  // handle using `GetModuleHandle`.
  assert(hInstance == GetModuleHandle(nullptr));

  // Configure tracing level, with default of 1.
  g_tracingLevel = envIntOr("TRACE", 1);

  // Create the window.
  GTLMainWindow mainWindow;
  CreateWindowExWArgs cw;
  cw.m_lpWindowName = L"Game TODO List";
  cw.m_x       = 200;
  cw.m_y       = 200;
  cw.m_nWidth  = 400;
  cw.m_nHeight = 400;
  cw.m_dwStyle = WS_OVERLAPPEDWINDOW;
  mainWindow.createWindow(cw);

  TRACE2(L"Calling ShowWindow");
  ShowWindow(mainWindow.m_hwnd, nCmdShow);

  // Run the message loop.

  MSG msg = { };
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  TRACE2(L"Returning from main");
  return 0;
}


// EOF
