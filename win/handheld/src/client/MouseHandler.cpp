#include "MouseHandler.h"
#include "player/input/ITurnInput.h"

#ifdef RPI
#include <SDL/SDL.h>
#endif
#if defined(MACOS) || defined(LINUX)
#include <SDL.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
extern bool g_win32MouseCaptured;
extern bool g_win32ShouldCapture;
extern HWND g_win32Hwnd;

// Implemented here so both main_win32.h and this file share one symbol.
void win32WarpToCenter(HWND hwnd) {
	if (!hwnd) return;
	RECT r;
	GetClientRect(hwnd, &r);
	POINT pt = { (r.right - r.left) / 2, (r.bottom - r.top) / 2 };
	ClientToScreen(hwnd, &pt);
	SetCursorPos(pt.x, pt.y);
}
#endif

MouseHandler::MouseHandler( ITurnInput* turnInput )
:	_turnInput(turnInput)
{}

MouseHandler::MouseHandler()
:	_turnInput(0)
{}

MouseHandler::~MouseHandler() {
}

void MouseHandler::setTurnInput( ITurnInput* turnInput ) {
	_turnInput = turnInput;
}

void MouseHandler::grab() {
	xd = 0;
	yd = 0;

#if defined(RPI)
	//LOGI("Grabbing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_ON);
	SDL_ShowCursor(0);
#elif defined(MACOS) || defined(LINUX)
	SDL_SetRelativeMouseMode(SDL_TRUE);
	SDL_ShowCursor(0);
#elif defined(_WIN32)
	g_win32MouseCaptured = true;
	g_win32ShouldCapture = true;
	// ShowCursor uses a reference count; loop until the cursor is truly hidden
	// so an unbalanced ShowCursor(TRUE) elsewhere can't keep it visible.
	while (ShowCursor(FALSE) >= 0) {}
	if (g_win32Hwnd) {
		win32WarpToCenter(g_win32Hwnd);
		RECT r;
		GetClientRect(g_win32Hwnd, &r);
		MapWindowPoints(g_win32Hwnd, NULL, (LPPOINT)&r, 2);
		ClipCursor(&r);
	}
#endif
}

void MouseHandler::release() {
#if defined(RPI)
	//LOGI("Releasing input!\n");
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(1);
#elif defined(MACOS) || defined(LINUX)
	SDL_SetRelativeMouseMode(SDL_FALSE);
	SDL_ShowCursor(1);
#elif defined(_WIN32)
	g_win32MouseCaptured = false;
	g_win32ShouldCapture = false;
	// Loop until the cursor is truly visible (counter back at 0).
	while (ShowCursor(TRUE) < 0) {}
	ClipCursor(NULL);
#endif
}

void MouseHandler::poll() {
#ifdef _WIN32
	// Standard-FPS frame-delta look. Sample the cursor's offset from the
	// window center once per frame, convert it to a view turn, then warp
	// the cursor back to the center. This is independent of the WM_INPUT
	// message stream, so look input can never stall when messages lag or
	// pile up (world load, chunk streaming stutters, etc.). The cursor is
	// pinned at the center, so the offset IS the per-frame movement.
	if (g_win32MouseCaptured && g_win32Hwnd) {
		RECT r;
		GetClientRect(g_win32Hwnd, &r);
		const int cx = (r.right - r.left) / 2;
		const int cy = (r.bottom - r.top) / 2;
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(g_win32Hwnd, &pt);
		float dx = (float)(pt.x - cx);
		float dy = (float)(pt.y - cy);
		win32WarpToCenter(g_win32Hwnd);
		// Screen-pixel -> turn magnitude. Combined with GameRenderer's
		// sens*4 factor; tune if look feels too fast/slow.
		const float k = 0.12f;
		xd = dx * k;
		yd = dy * k;
		return;
	}
	// Self-heal the capture flag: a focus race (world load, stray
	// WM_KILLFOCUS, alt-tab) can leave g_win32MouseCaptured false while
	// the game still wants capture. Without it look is dead and only an
	// UI toggle (open/close inventory) would re-grab. If the window is
	// foreground again, silently re-capture.
	if (g_win32ShouldCapture && !g_win32MouseCaptured && g_win32Hwnd &&
		GetForegroundWindow() == g_win32Hwnd) {
		g_win32MouseCaptured = true;
		while (ShowCursor(FALSE) >= 0) {}
		if (g_win32Hwnd) {
			win32WarpToCenter(g_win32Hwnd);
			RECT r;
			GetClientRect(g_win32Hwnd, &r);
			MapWindowPoints(g_win32Hwnd, NULL, (LPPOINT)&r, 2);
			ClipCursor(&r);
		}
	}
#endif
	if (_turnInput != 0) {
		TurnDelta td = _turnInput->getTurnDelta();
		xd = td.x;
		yd = td.y;
	}
}
