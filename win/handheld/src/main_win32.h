#ifndef MAIN_WIN32_H__
#define MAIN_WIN32_H__

/*
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
*/

#include "client/renderer/gles.h"
#include <EGL/egl.h>
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <windowsx.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <WinSock2.h>
#include <process.h>
#include <imm.h>

// GUI (Windows) subsystem: no console window on launch. mainCRTStartup
// keeps the CRT entry that calls main() (main_win32.h defines main).
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:mainCRTStartup")

#include <cstdio>
#include <cstdarg>
#include "platform/input/Mouse.h"
#include "platform/input/Multitouch.h"
#include "util/Mth.h"
#include "util/FrameProf.h"
#include "AppPlatform_win32.h"

static App* g_app = 0;
static volatile bool g_running = true;

// Frame-time breakdown for the F3 overlay (no file I/O, no per-frame logging):
// how long the last app->update() took (tick + world render + GUI) and how
// long eglSwapBuffers took. Written by the main loop / App::swapBuffers, read
// by PerfRenderer when the user opens F3.
double g_diagUpdateMs = 0.0;
double g_diagSwapMs   = 0.0;

// 帧戳（定义在 LevelRenderer.cpp）：每帧递增，让 chunk 重建预算能跨同一帧
// 内的多次调用累计（离屏场景/水面反射/主视图会各调一次 updateDirtyChunks）。
extern int g_frameStamp;

bool g_win32MouseCaptured = false;  // shared with MouseHandler.cpp
bool g_win32ShouldCapture = false;  // desired in-game mouse capture (set by MouseHandler grab/release)
HWND g_win32Hwnd = NULL;             // shared with MouseHandler.cpp

// IME (input method editor) state.
//
// Design: we do NOT attach/detach the IME context (that broke Chinese
// composition — the forced old handle never received WM_IME_CHAR). Instead
// the window keeps its system-default context and we switch the IME's
// CONVERSION MODE per screen type:
//   - text-input screen open (chat/server box): force CHINESE mode
//     (IME_CMODE_NATIVE) so the player can type pinyin; composed text
//     arrives via WM_CHAR/WM_IME_CHAR and is fed to the game.
//   - gameplay (no text screen): force ENGLISH mode (clear
//     IME_CMODE_NATIVE). A TSF input method (Microsoft Pinyin etc.) in
//     Chinese mode intercepts letter keys BEFORE the window process sees
//     them (they arrive as VK_PROCESSKEY), so WASD/inventory keys must
//     never be exposed to a Chinese-mode IME while playing.
// We only track whether a text screen is open (g_imeEnabled); the
// conversion mode itself is forced in both directions, never "restored",
// so gameplay keys always work regardless of the player's OS-wide IME mode.
static bool g_imeEnabled = false;       // a text-input screen is open

// Diagnostic logging for IME debugging. Off by default (write "mcpe_mod.log"
// next to the exe only when MCPE_IME_DEBUG is set), so a release build never
// leaks the user's home path or does per-message file I/O.
static void imeDbg(const char* fmt, ...) {
	static bool enabled = (getenv("MCPE_IME_DEBUG") != NULL);
	if (!enabled) return;
	FILE* f = fopen("mcpe_mod.log", "a");
	if (!f) return;
	va_list ap; va_start(ap, fmt);
	fprintf(f, "[ime] ");
	vfprintf(f, fmt, ap);
	va_end(ap);
	fclose(f);
}

// Enable (force Chinese input mode) or disable (force English mode) the IME
// for this window. Called from Minecraft::setScreen whenever a screen with
// text input opens/closes. hwnd is the game window.
//
// While a text screen is open we force English (clear IME_CMODE_NATIVE) so
// that English keyboards type directly (letters arrive via WM_KEYDOWN ->
// win32KeyToText). If the player switches their input method to a Chinese
// IME in Chinese mode, win32ImeIsChinese() turns true and composition works
// via WM_IME_CHAR. We must NOT force NATIVE on text screens: on an English
// keyboard that made every letter arrive as VK_PROCESSKEY (0xE5) into an
// empty composition with no way to confirm -> characters were swallowed and
// nothing could be typed.
// When the screen closes (gameplay) we also force English so a TSF input
// method (Microsoft Pinyin etc.) never intercepts letter keys (WASD etc.).
void win32SetImeEnabled(HWND hwnd, bool enable) {
	HIMC imc = ImmGetContext(hwnd);
	imeDbg("win32SetImeEnabled(%d) cur=%d imc=%p\n", enable, g_imeEnabled, (void*)imc);
	if (imc) {
		DWORD conv = 0, sent = 0;
		if (ImmGetConversionStatus(imc, &conv, &sent)) {
			// Always clear Chinese mode whenever the game takes control of
			// the keyboard — both on opening a text screen and on returning
			// to gameplay. On a text screen this gives English direct typing
			// (letters arrive via WM_KEYDOWN -> win32KeyToText); the player
			// can switch their input method to Chinese for pinyin, at which
			// point win32ImeIsChinese() turns true and WM_IME_CHAR delivers
			// the composed text. During gameplay it stops a TSF IME from
			// swallowing WASD etc. (The old code only cleared it when
			// closing, so a previously-forced Chinese mode persisted into
			// the next text screen and swallowed English keys.)
			ImmSetConversionStatus(imc, conv & ~(DWORD)IME_CMODE_NATIVE, sent);
		}
		// 真正把输入法本体的开关也切一下。以前只切了“转换模式”（等于帮你按一下
		// 中/英键），输入法自己的悬浮状态条/候选窗照样在，游戏中按一下 Shift 又
		// 能切回中文。现在：不开文本输入屏就关掉输入法，开文本屏（聊天/服务器
		// 地址等）时再打开 —— 进聊天框就能打中文，游戏中则绝不冒出来。
		// 用 ImmSetOpenStatus 而不是 ImmAssociateContext(NULL)：后者历史上弄坏过
		// 聊天框的中文合成（见文件顶部那段注释）。
		ImmSetOpenStatus(imc, enable ? TRUE : FALSE);
		ImmReleaseContext(hwnd, imc);
	}
	g_imeEnabled = enable;
}

// Feed one UTF-16 code unit (as delivered by WM_CHAR / WM_IME_CHAR) into the
// game's text input, converting to UTF-8. Handles surrogate pairs (non-BMP
// chars such as CJK ext-B / emoji arrive as TWO WM_CHARs — merging them here
// avoids emitting a broken half of a pair) and drops control characters
// (0x00-0x1F, 0x7F): Enter/Backspace are delivered as key events, not as
// text, and feeding 0x0D/0x08 here would corrupt the message.
static void win32FeedWChar(WPARAM wParam) {
	static WCHAR pendingHigh = 0;
	WCHAR c = (WCHAR)wParam;
	if (c >= 0xD800 && c <= 0xDBFF) {  // high surrogate: wait for the pair
		pendingHigh = c;
		return;
	}
	if (c >= 0xDC00 && c <= 0xDFFF) {  // low surrogate: complete the pair
		if (pendingHigh) {
			WCHAR pair[2] = { pendingHigh, c };
			pendingHigh = 0;
			char tmp[8];
			int m = WideCharToMultiByte(CP_UTF8, 0, pair, 2, tmp, 8, NULL, NULL);
			if (m > 0)
				Keyboard::feedText(std::string(tmp, m));
		}
		return;
	}
	pendingHigh = 0;
	if (c < 0x20 || c == 0x7F)
		return;
	char tmp[8];
	int m = WideCharToMultiByte(CP_UTF8, 0, &c, 1, tmp, 8, NULL, NULL);
	if (m > 0)
		Keyboard::feedText(std::string(tmp, m));
}

// Query the window's IME context for the current conversion mode: true when
// a CJK input method is active AND in Chinese (native) mode, i.e. letter
// keys are consumed for pinyin composition instead of passing through.
// Polled per WM_KEYDOWN so mid-chat 中/英 toggles are picked up instantly
// (no need to react to WM_INPUTLANGCHANGE).
static bool win32ImeIsChinese(HWND hwnd) {
	HIMC imc = ImmGetContext(hwnd);
	if (!imc) return false;
	DWORD conv = 0, sent = 0;
	ImmGetConversionStatus(imc, &conv, &sent);
	ImmReleaseContext(hwnd, imc);
	return (conv & IME_CMODE_NATIVE) != 0;
}

// Generate the character(s) for a WM_KEYDOWN via ToUnicode through the active
// keyboard layout (honors Shift/CapsLock and the Shift+digit symbols). Used
// only when the IME is NOT composing — English mode with a text screen open —
// because WM_CHAR is unreliable on this system. Dead keys (n<0) and control
// chars (Enter/Backspace/arrows) are dropped.
static void win32KeyToText(WPARAM wParam, LPARAM lParam) {
	BYTE kbd[256];
	if (!GetKeyboardState(kbd))
		return;
	WCHAR buf[8] = {0};
	int n = ToUnicode((UINT)wParam, (UINT)((lParam >> 16) & 0xFF), kbd, buf, 7, 0);
	if (n <= 0)
		return;  // dead key / no mapping
	std::string utf8;
	for (int i = 0; i < n && i < 7; ++i) {
		// Only printable characters go into text input. Control keys
		// (Backspace=0x08, Enter=0x0D, Delete=0x7F, arrows...) produce
		// control chars here that would show as a stray box and crash the
		// chat renderer on send.
		if (buf[i] < 0x20 || buf[i] == 0x7F)
			continue;
		char tmp[8];
		int m = WideCharToMultiByte(CP_UTF8, 0, &buf[i], 1, tmp, 8, NULL, NULL);
		if (m > 0) utf8.append(tmp, m);
	}
	if (!utf8.empty())
		Keyboard::feedText(utf8);
}

// Warp the cursor back to the center of the window client area. Look input
// uses raw relative deltas (WM_INPUT), so repositioning the cursor does NOT
// generate view movement (absolute-mode raw events are skipped). Keeps the
// cursor pinned at the center so it can never drift to an edge and trigger
// stray clicks. Implemented in client/MouseHandler.cpp (Win32).
void win32WarpToCenter(HWND hwnd);

static int getBits(int bits, int startBitInclusive, int endBitExclusive, int shiftTruncate) {
	int sum = 0;
	for (int i = startBitInclusive; i<endBitExclusive; ++i)
		sum += (bits & (2<<i));
	return shiftTruncate? (sum >> startBitInclusive) : sum;
}

// Map Win32 virtual keys to the game's internal key codes.
static unsigned char transformKey_win32(WPARAM wParam) {
	if (wParam == VK_LSHIFT || wParam == VK_SHIFT) return Keyboard::KEY_LSHIFT;
	if (wParam == VK_TAB) return 250;  // internal Tab code (same as macOS/Linux)
	return (unsigned char)wParam;
}

// True when the window is minimized (client area is 0x0). We keep the last
// non-zero size instead of letting setSize(0,0) drive widths/heights to zero,
// which previously crashed the GUI with an integer divide-by-zero in
// Screen::toGUICoordinate (0xC0000094 in OptionsScreen slider ticks).
static bool g_windowMinimized = false;

void resizeWindow(HWND hWnd, int nWidth, int nHeight) {
   RECT rcClient, rcWindow;
   POINT ptDiff;
     GetClientRect(hWnd, &rcClient);
     GetWindowRect(hWnd, &rcWindow);
   ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
   ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
   MoveWindow(hWnd,rcWindow.left, rcWindow.top, nWidth + ptDiff.x, nHeight + ptDiff.y, TRUE);
}

void toggleResolutions(HWND hwnd, int direction) {
	static int n = 0;
	static int sizes[][3] = {
		{854, 480, 1},
		{800, 480, 1},
		{480, 320, 1},
		{1024, 768, 1},
		{1280, 800, 1},
		{1024, 580, 1}
	};
	static int count = sizeof(sizes) / sizeof(sizes[0]);
	n = (count + n + direction) % count;
	
	int* size = sizes[n];
	int k = size[2];
	
	resizeWindow(hwnd, k * size[0], k * size[1]);
}

LRESULT WINAPI windowProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	LRESULT retval = 1;
	
	switch (uMsg)
	{
	case WM_SETTEXT: {
		// The PowerVR EGL simulation (libegl) renames the window (PVRVFRAME,
		// "M", ...) via its own WndProc hook. Log what we receive and force
		// our title back; the main loop also re-pins it every frame.
		{
			// lParam points at the new text (ANSI or wide depending on the
			// caller); log it as a wide string for diagnostics.
			WCHAR* t = (WCHAR*)lParam;
			if (t && t[0]) {
				char buf[128];
				int m = WideCharToMultiByte(CP_UTF8, 0, t, -1, buf, 128, NULL, NULL);
				if (m > 0) imeDbg("WM_SETTEXT -> '%s'\n", buf);
			}
		}
		LRESULT r = DefWindowProc(hWnd, uMsg, wParam, lParam);
		static bool forcing = false;
		if (!forcing) {  // SetWindowTextW re-sends WM_SETTEXT; guard recursion
			forcing = true;
			SetWindowTextW(hWnd, L"MinecraftPE Community Edition");
			forcing = false;
		}
		return r;
	}
	case WM_KEYDOWN: {
		if (wParam == 33) toggleResolutions(hWnd, -1);
		if (wParam == 34) toggleResolutions(hWnd, +1);
		imeDbg("WM_KEYDOWN wParam=0x%X scan=0x%X enabled=%d chinese=%d\n",
		       (unsigned)wParam, (unsigned)((lParam >> 16) & 0xFF), g_imeEnabled,
		       g_imeEnabled ? win32ImeIsChinese(hWnd) : 0);
		if (wParam == 0xE5) {
			// VK_PROCESSKEY: the IME consumed this key for composition.
			//  - Text screen open: hand it back to the IME (DefWindowProc ->
			//    ImmProcessKey) so composition continues.
			//  - Gameplay: the IME is forced to English mode and will never
			//    compose, yet a TSF input method still reports letter keys
			//    as VK_PROCESSKEY. The real key lives in the scan code —
			//    recover it and feed the game, or WASD etc. would be lost.
			if (g_imeEnabled)
				return DefWindowProc(hWnd, uMsg, wParam, lParam);
			unsigned char vk = (unsigned char)MapVirtualKey((UINT)((lParam >> 16) & 0xFF), MAPVK_VSC_TO_VK_EX);
			imeDbg("  VK_PROCESSKEY gameplay: vk=0x%X\n", (unsigned)vk);
			unsigned char k = transformKey_win32(vk);
			if (k) Keyboard::feed(k, 1);
			return 0;
		}
		unsigned char k = transformKey_win32(wParam);
		if (g_imeEnabled) {
			// A text-input screen is open. Where each key goes depends on the
			// IME's current conversion mode (polled live, so mid-chat 中/英
			// toggles are honored):
			//  - Chinese (IME_CMODE_NATIVE): every key must reach the IME so
			//    pinyin composition can start and continue. Feeding keys to
			//    the game here would (a) type raw pinyin letters into the
			//    field and (b) returning 0 would swallow the key so the IME
			//    never sees it — composition would never begin. Only the
			//    control keys (Enter=submit, Backspace=erase, Escape=close)
			//    go to the game's key handlers; everything else goes to
			//    DefWindowProc -> ImmProcessKey -> the IME.
			//  - English (player switched out of Chinese mid-chat): the IME
			//    passes keys through; generate the character directly with
			//    ToUnicode (WM_CHAR is unreliable on this system) and feed
			//    the key for keyPressed handlers as usual.
			if (win32ImeIsChinese(hWnd)) {
				if (wParam == VK_RETURN || wParam == VK_BACK || wParam == VK_ESCAPE) {
					if (k) Keyboard::feed(k, 1);
				}
				return DefWindowProc(hWnd, uMsg, wParam, lParam);
			}
			if (k) Keyboard::feed(k, 1);
			win32KeyToText(wParam, lParam);
			return 0;
		}
		// No text-input screen open (gameplay): feed key state only, so
		// keyPressed bindings (WASD, E, inventory...) keep working. Do NOT
		// generate text characters here — nothing consumes them, and they
		// would pile up in the Keyboard buffer and leak into the next text
		// screen that opens.
		if (k) Keyboard::feed(k, 1);
		return 0;
	}
	case WM_KEYUP: {
		if (wParam == 0xE5) {
			// Symmetric with WM_KEYDOWN: a TSF input method reports letter
			// keys as VK_PROCESSKEY even in English mode. In gameplay, feed
			// the key recovered from the scan code so key state is released
			// (otherwise the game would think WASD stays held); in a text
			// screen hand it to the IME.
			if (g_imeEnabled)
				return DefWindowProc(hWnd, uMsg, wParam, lParam);
			unsigned char vk = (unsigned char)MapVirtualKey((UINT)((lParam >> 16) & 0xFF), MAPVK_VSC_TO_VK_EX);
			unsigned char k = transformKey_win32(vk);
			if (k) Keyboard::feed(k, 0);
			return 0;
		}
		unsigned char k = transformKey_win32(wParam);
		// In Chinese mode only the control keys we fed on KEYDOWN are fed on
		// KEYUP; keys handed to the IME for composition never reach the game,
		// and their KEYUP goes back to the IME so its state machine completes.
		if (g_imeEnabled && win32ImeIsChinese(hWnd)) {
			if (wParam == VK_RETURN || wParam == VK_BACK || wParam == VK_ESCAPE) {
				if (k) Keyboard::feed(k, 0);
			}
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
		if (k) Keyboard::feed(k, 0);
		return 0;
	}
	case WM_CHAR: {
		// A text-input screen is open: composed Chinese text (IME in Chinese
		// mode) arrives here as UTF-16 and must go into the text input. In
		// English mode the character was ALREADY fed in WM_KEYDOWN via
		// ToUnicode (win32KeyToText), so skip WM_CHAR there to avoid
		// double-feeding. Control chars (0x0D from Enter, 0x08 from
		// Backspace) are dropped by win32FeedWChar — those are handled as
		// key events. With no text-input screen open we do not generate
		// characters at all (see WM_KEYDOWN), so ignore WM_CHAR then too.
		imeDbg("WM_CHAR wParam=0x%X enabled=%d chinese=%d\n", (unsigned)wParam, g_imeEnabled, g_imeEnabled ? win32ImeIsChinese(hWnd) : 0);
		if (g_imeEnabled && win32ImeIsChinese(hWnd)) {
			imeDbg("  -> feeding WM_CHAR\n");
			win32FeedWChar(wParam);
		}
		return 0;
	}
	case WM_IME_STARTCOMPOSITION:
	case WM_IME_COMPOSITION:
	case WM_IME_ENDCOMPOSITION:
		// Composition lifecycle: let the system manage the candidate window
		// and composition; the final text arrives via WM_IME_CHAR.
		imeDbg("IME composition msg 0x%X\n", (unsigned)uMsg);
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	case WM_IME_CHAR:
		// Composed Chinese text (UTF-16), delivered when the player confirms
		// a candidate. Feed it into the game like WM_CHAR when a text-input
		// screen is open, then consume the message ourselves (return 0):
		// DefWindowProc would translate it into another WM_CHAR, which the
		// WM_CHAR branch would feed again — double-typed characters.
		imeDbg("WM_IME_CHAR wParam=0x%X enabled=%d\n", (unsigned)wParam, g_imeEnabled);
		if (g_imeEnabled) {
			imeDbg("  -> feeding WM_IME_CHAR\n");
			win32FeedWChar(wParam);
		}
		return 0;
	case WM_IME_SETCONTEXT:
		// IME context attached/detached (window activation etc.). Nothing
		// extra to track — we control attachment via win32SetImeEnabled.
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	case WM_INPUTLANGCHANGE:
		// Keyboard layout / IME switched (e.g. 中/英 toggle, Ctrl+Space):
		// pass through; the per-key Chinese-mode check picks up the new mode.
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	case WM_LBUTTONDOWN: {
		Mouse::feed( MouseAction::ACTION_LEFT, 1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		Multitouch::feed(1, 1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0);
		break;
	}
	case WM_LBUTTONUP: {
		Mouse::feed( MouseAction::ACTION_LEFT, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		Multitouch::feed(1, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0);
		break;
	}
	case WM_RBUTTONDOWN: {
		Mouse::feed( MouseAction::ACTION_RIGHT, 1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	}
	case WM_RBUTTONUP: {
		Mouse::feed( MouseAction::ACTION_RIGHT, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	}
	case WM_MOUSEMOVE: {
		if (!g_win32MouseCaptured) {
			Mouse::feed(MouseAction::ACTION_MOVE, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			Multitouch::feed(0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0);
		}
		break;
	}
	case WM_MOUSEWHEEL: {
		short delta = GET_WHEEL_DELTA_WPARAM(wParam);
		Mouse::feed(MouseAction::ACTION_WHEEL, (delta != 0) ? 1 : 0,
		            GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, (short)(delta / 120));
		break;
	}
	case WM_INPUT: {
		if (g_win32MouseCaptured) {
			UINT size = sizeof(RAWINPUT);
			RAWINPUT raw;
			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1) {
				if (raw.header.dwType == RIM_TYPEMOUSE &&
					!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
					short dx = (short)raw.data.mouse.lLastX;
					short dy = (short)raw.data.mouse.lLastY;
					if (dx != 0 || dy != 0) {
						RECT r;
						GetClientRect(hWnd, &r);
						Mouse::feed(MouseAction::ACTION_MOVE, 0,
						            (short)((r.right - r.left) / 2),
						            (short)((r.bottom - r.top) / 2),
						            dx, dy);
					}
				}
			}
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	case WM_SETFOCUS: {
		// If the game wants in-game mouse capture (we are inside a world),
		// re-grab the mouse when focus returns so look control and cursor
		// hiding survive a focus loss/regain cycle.
		if (g_win32ShouldCapture) {
			g_win32MouseCaptured = true;
			while (ShowCursor(FALSE) >= 0) {}
			win32WarpToCenter(hWnd);
			if (g_win32Hwnd) {
				RECT r;
				GetClientRect(g_win32Hwnd, &r);
				MapWindowPoints(g_win32Hwnd, NULL, (LPPOINT)&r, 2);
				ClipCursor(&r);
			}
		}
		// 拿回焦点时同样压一次输入法：不在文本输入屏就把它关掉（与 WM_ACTIVATE
		// 一样的意思 —— 系统可能刚把输入法恢复成“打开”）。
		if (!g_imeEnabled) {
			HIMC imc = ImmGetContext(hWnd);
			if (imc) {
				ImmSetOpenStatus(imc, FALSE);
				ImmReleaseContext(hWnd, imc);
			}
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	case WM_KILLFOCUS: {
		if (g_win32MouseCaptured) {
			g_win32MouseCaptured = false;
			while (ShowCursor(TRUE) < 0) {}
			ClipCursor(NULL);
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	case WM_ACTIVATE: {
		// 窗口重新被激活（Alt+Tab 回来、用鼠标点回来）时，系统会按输入法自己的
		// 记忆把它恢复成“打开”，于是游戏里又冒出输入法状态条。这里按“当前是否
		// 开着文本输入屏”再压一次（g_imeEnabled 由 Minecraft::setScreen 维护）。
		if (LOWORD(wParam) != WA_INACTIVE && !g_imeEnabled) {
			HIMC imc = ImmGetContext(hWnd);
			if (imc) {
				ImmSetOpenStatus(imc, FALSE);
				ImmReleaseContext(hWnd, imc);
			}
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	default:
		if (uMsg == WM_NCDESTROY) g_running = false;
		else {
			if (uMsg == WM_SIZE) {
				int nw = GET_X_LPARAM(lParam);
				int nh = GET_Y_LPARAM(lParam);
				g_windowMinimized = (wParam == SIZE_MINIMIZED) || (nw <= 0) || (nh <= 0);
				if (g_app && !g_windowMinimized) g_app->setSize(nw, nh);
			}
		}
		retval = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}
	return retval;
}

// libegl (PowerVR sim) subclasses our window (SetWindowSubclass) and rewrites
// every WM_SETTEXT it sees to its own frame title ("M", "PVRVFRAME", ...),
// so neither SetWindowTextW nor swapping GWLP_WNDPROC sticks. Calling
// DefWindowProcW(WM_SETTEXT) directly performs the same update of the
// window's internal title text that SetWindowText ultimately does, without
// going through the subclass chain.
//
// We only re-send WM_SETTEXT when the title actually differs from what we
// last set: libegl rewrites the title on init and occasionally later, so
// every-frame re-pinning is unnecessary (DefWindowProcW(WM_SETTEXT) walks
// the window class message pump each call).
static void win32SetWindowTitle(HWND hwnd, const wchar_t* title) {
	static const wchar_t* s_lastTitle = NULL;
	if (s_lastTitle == title) return;              // same pointer (our constant)
	if (s_lastTitle && wcscmp(s_lastTitle, title) == 0) return;  // same text
	DefWindowProcW(hwnd, WM_SETTEXT, 0, (LPARAM)title);
	s_lastTitle = title;
}

void platform(HWND *result, int width, int height) {
	// IMPORTANT: create a UNICODE window class (RegisterClassW + WNDCLASSW).
	// For an ANSI window the IME delivers WM_CHAR/WM_IME_CHAR encoded in the
	// active code page (GBK on zh-CN systems, one byte per message), not
	// UTF-16 — win32FeedWChar would mangle the bytes. A Unicode window
	// always delivers UTF-16 code units, which win32FeedWChar consumes.
	WNDCLASSW wc;
	RECT wRect;
	HWND hwnd;
	HINSTANCE hInstance;

	wRect.left = 0L;
	wRect.right = (long)width;
	wRect.top = 0L;
	wRect.bottom = (long)height;

	hInstance = GetModuleHandle(NULL);

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)windowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));   // grass block icon from exe resources (ApplicationIcon -> IDI_ICON1)
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"OGLES";

	RegisterClassW(&wc);

	AdjustWindowRectEx(&wRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);

	hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, L"OGLES", L"MinecraftPE Community Edition", WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, wRect.right-wRect.left, wRect.bottom-wRect.top, NULL, NULL, hInstance, NULL);
	*result = hwnd;
}

/** Thread that reads input data via UDP network datagrams
    and fills Mouse and Keyboard structures accordingly.
	@note: The bound local net address is unfortunately
	       hard coded right now (to prevent wrong Interface) */
void inputNetworkThread(void* userdata)
{
	// set up an UDP socket for listening
	WSADATA wsaData;
	if (WSAStartup(0x101, &wsaData)) {
		printf("Couldn't initialize winsock\n");
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET) {
		printf("Couldn't create socket\n");
		return;
	}
	
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9991);
	addr.sin_addr.s_addr = inet_addr("192.168.0.119");

	if (bind(s, (sockaddr*)&addr, sizeof(addr))) {
		printf("Couldn't bind socket to port 9991\n");
		return;
	}
	
	sockaddr fromAddr;
	int fromAddrLen = sizeof(fromAddr);

	char buf[1500];
	int* iptrBuf = (int*)buf;

	printf("input-server listening...\n");

	while (1) {
		int read = recvfrom(s, buf, 1500, 0, &fromAddr, &fromAddrLen);
		if (read < 0)
		{
			printf("recvfrom failed with code: %d\n", WSAGetLastError());
			return;
		}
		// Keyboard
		if (read == 2) {
			Keyboard::feed((unsigned char) buf[0], (int)buf[1]);
		}
		// Mouse
		else if (read == 16) {
			Mouse::feed(iptrBuf[0], iptrBuf[1], iptrBuf[2], iptrBuf[2]);
		}
	}
}

int main(void) {
	// Vectored exception handler: fires before any SEH/signal machinery, so
	// we can log the real fault address (signal stack traces are useless).
	AddVectoredExceptionHandler(1, [](EXCEPTION_POINTERS* ep) -> LONG {
		FILE* lf = fopen("C:\\Users\\coj2\\AppData\\Local\\Temp\\mcpe_mod.log", "a");
		if (lf) {
			fprintf(lf, "[veh] EXCEPTION code=0x%08X addr=%p\n",
				ep->ExceptionRecord->ExceptionCode,
				ep->ExceptionRecord->ExceptionAddress);
			// Symbol of the faulting instruction (EIP).
			{
				SymInitialize(GetCurrentProcess(), NULL, TRUE);
				char buf2[sizeof(SYMBOL_INFO) + 256] = {0};
				SYMBOL_INFO* si2 = (SYMBOL_INFO*)buf2;
				si2->SizeOfStruct = sizeof(SYMBOL_INFO);
				si2->MaxNameLen = 255;
				DWORD64 disp2 = 0;
				DWORD64 eip = (ep->ContextRecord) ? (DWORD64)ep->ContextRecord->Eip : 0;
				if (eip && SymFromAddr(GetCurrentProcess(), eip, &disp2, si2))
					fprintf(lf, "[veh] FAULT EIP=%p in %s+0x%llX\n", (void*)eip, si2->Name, (unsigned long long)disp2);
				else
					fprintf(lf, "[veh] FAULT EIP=%p <no symbol>\n", (void*)eip);
				SymCleanup(GetCurrentProcess());
			}
			// Symbolized stack trace (dbghelp + PDB next to the exe).
			{
				SymInitialize(GetCurrentProcess(), NULL, TRUE);
				void* frames[32];
				ULONG hash = 0;
				USHORT n = RtlCaptureStackBackTrace(0, 32, frames, &hash);
				for (USHORT i = 0; i < n; ++i) {
					DWORD64 pc = (DWORD64)frames[i];
					char buf[sizeof(SYMBOL_INFO) + 256] = {0};
					SYMBOL_INFO* si = (SYMBOL_INFO*)buf;
					si->SizeOfStruct = sizeof(SYMBOL_INFO);
					si->MaxNameLen = 255;
					DWORD64 disp = 0;
					if (SymFromAddr(GetCurrentProcess(), pc, &disp, si)) {
						IMAGEHLP_LINE64 line = { sizeof(IMAGEHLP_LINE64) };
						DWORD ldisp = 0;
						if (SymGetLineFromAddr64(GetCurrentProcess(), pc, &ldisp, &line))
							fprintf(lf, "  #%d 0x%p %s+0x%llX (%s:%u)\n", i, (void*)pc, si->Name, (unsigned long long)disp, line.FileName, line.LineNumber);
						else
							fprintf(lf, "  #%d 0x%p %s+0x%llX\n", i, (void*)pc, si->Name, (unsigned long long)disp);
					} else {
						fprintf(lf, "  #%d 0x%p <no symbol>\n", i, (void*)pc);
					}
				}
				SymCleanup(GetCurrentProcess());
			}
			// Minidump for offline analysis (dbghelp).
			HMODULE dbg = LoadLibraryA("dbghelp.dll");
			if (dbg) {
				typedef BOOL (WINAPI *MDW)(HANDLE, DWORD, HANDLE, DWORD, MINIDUMP_EXCEPTION_INFORMATION*, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
				MDW mdw = (MDW)GetProcAddress(dbg, "MiniDumpWriteDump");
				if (mdw) {
					static int dumpNo = 0;
					char path[MAX_PATH];
					sprintf_s(path, "H:\\workerspace\\workapace\\crash_%d.dmp", ++dumpNo);
					HANDLE hf = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hf != INVALID_HANDLE_VALUE) {
						MINIDUMP_EXCEPTION_INFORMATION mdei = { GetCurrentThreadId(), ep, FALSE };
						mdw(GetCurrentProcess(), GetCurrentProcessId(), hf,
							MINIDUMP_TYPE(MiniDumpWithDataSegs | MiniDumpWithHandleData), &mdei, NULL, NULL);
						CloseHandle(hf);
						fprintf(lf, "[veh] minidump written: %s\n", path);
					}
				}
			}
			fclose(lf);
		}
		return EXCEPTION_CONTINUE_SEARCH;
	});
	// Set the working directory to the exe's directory so that all relative
	// resource paths (data/, worlds/, Documents/) resolve correctly no matter
	// how the game is launched (double-click, shortcut, other CWD, ...).
	{
		char exePath[MAX_PATH];
		DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
		if (len > 0 && len < MAX_PATH) {
			char* slash = strrchr(exePath, '\\');
			if (slash) {
				*slash = '\0';
				SetCurrentDirectoryA(exePath);
			}
		}
	}
	AppContext appContext = {};   // zero-init: doRender etc. were stack garbage
	appContext.doRender = true;   // always present frames (was undefined behavior)
	MSG sMessage;

#ifndef STANDALONE_SERVER

	EGLint aEGLAttributes[] = {
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8,
		EGL_DEPTH_SIZE,		16,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_NONE
	};
	EGLint aEGLContextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 1,
		EGL_NONE
	};

	EGLConfig m_eglConfig[1];
	EGLint nConfigs;

	HWND hwnd;
	g_running = true;

	// 关掉 PVRVFrame 的控制窗口。
	// libEGL/libgles_cm 这套 PowerVR 模拟层在初始化时会弹出一个标题为
	// "PVRVFRAME" 的 FOX-toolkit 窗口（FXTopWindow，里面是硬件 profile、
	// 单步一帧之类没人玩的调试开关）。玩家一开游戏它就跟出来，很碍事。
	// libEGL.dll 导出了 PVRVFrameEnableControlWindow(bool) 用来开关它，
	// 必须赶在 eglGetDisplay/eglInitialize 把它创建出来之前调用。
	{
		HMODULE hEglDll = GetModuleHandleA("libEGL.dll");
		if (!hEglDll) hEglDll = LoadLibraryA("libEGL.dll");
		if (hEglDll) {
			typedef void (*PFN_PVRVFRAME_ENABLE_CONTROL_WINDOW)(int enable);
			PFN_PVRVFRAME_ENABLE_CONTROL_WINDOW pfnEnableControlWindow =
				(PFN_PVRVFRAME_ENABLE_CONTROL_WINDOW) GetProcAddress(hEglDll, "PVRVFrameEnableControlWindow");
			if (pfnEnableControlWindow)
				pfnEnableControlWindow(0);
		}
	}

	// Platform init.
	appContext.platform = new AppPlatform_win32();
	platform(&hwnd, appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);
	g_win32Hwnd = hwnd;
	// No text-input screen is open at startup; g_imeEnabled flips on when a
	// chat/server box opens (Minecraft::setScreen -> win32SetImeEnabled),
	// which forces the IME into Chinese mode for pinyin composition. While
	// playing, the IME stays in the player's own mode and letter keys that
	// the IME swallows for composition (VK_PROCESSKEY) are passed through to
	// the IME via DefWindowProc, while plain keys go to the game — so WASD
	// works unless the player's IME is actively composing in Chinese mode.
	g_imeEnabled = false;
	// Register for raw mouse input (enables in-game relative mouse look)
	{
		RAWINPUTDEVICE rid;
		rid.usUsagePage = 0x01;  // HID_USAGE_PAGE_GENERIC
		rid.usUsage     = 0x02;  // HID_USAGE_GENERIC_MOUSE
		rid.dwFlags     = 0;
		rid.hwndTarget  = hwnd;
		RegisterRawInputDevices(&rid, 1, sizeof(rid));
	}

	// EGL init.
	appContext.display = eglGetDisplay(GetDC(hwnd));
	//m_eglDisplay = eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY);

	eglInitialize(appContext.display, NULL, NULL);

	eglChooseConfig(appContext.display, aEGLAttributes, m_eglConfig, 1, &nConfigs);
	printf("EGLConfig = %p\n", m_eglConfig[0]);

	appContext.surface = eglCreateWindowSurface(appContext.display, m_eglConfig[0], (NativeWindowType)hwnd, 0);
	printf("EGLSurface = %p\n", appContext.surface);

	appContext.context = eglCreateContext(appContext.display, m_eglConfig[0], EGL_NO_CONTEXT, NULL);//aEGLContextAttributes);
	printf("EGLContext = %p\n", appContext.context);
	if (!appContext.context) {
		printf("EGL error: %d\n", eglGetError());
	}

	eglMakeCurrent(appContext.display, appContext.surface, appContext.surface, appContext.context);

	// Vsync handling: we deliberately do NOT enable vsync here. With the
	// PowerVR EGL simulation + this old fixed-function renderer, frames often
	// land in the 16.7–33ms band; vsync then aligns them to the next vblank
	// and turns ~20ms frames into ~33ms ones (visible as ~38fps instead of
	// 60). The software QPC cap in the main loop only sleeps on *fast* frames
	// and never throttles slow ones, so it gives a steady ≤60 without the
	// vsync cliff. interval=0 makes sure the EGL layer doesn't add its own.
	{
		typedef EGLBoolean (EGLAPIENTRY *PFN_EGLSWAPINTERVAL)(EGLDisplay, EGLint);
		PFN_EGLSWAPINTERVAL pfnSwapInterval = (PFN_EGLSWAPINTERVAL)eglGetProcAddress("eglSwapInterval");
		if (pfnSwapInterval)
			pfnSwapInterval(appContext.display, 0);
	}

	glInit();

	// The PowerVR EGL simulation (libegl) renames the window to "PVRVFRAME"
	// during initialization, and window icons only stick if set explicitly.
	// Restore our title and grass-block icon once GL is up.
	{
		HINSTANCE hInst = GetModuleHandle(NULL);
		win32SetWindowTitle(hwnd, L"MinecraftPE Community Edition");
		HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));
		if (hIcon) {
			SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		}
	}

#endif
	App* app = new MAIN_CLASS();

	g_app = app;
	((MAIN_CLASS*)g_app)->externalStoragePath = ".";
	((MAIN_CLASS*)g_app)->externalCacheStoragePath = ".";
	g_app->init(appContext);
	g_app->setSize(appContext.platform->getScreenWidth(), appContext.platform->getScreenHeight());

	//_beginthread(inputNetworkThread, 0, 0);

	// Frame pacing: target 60fps. NOTE: a plain Sleep(N) is NOT precise enough
	// on Windows (the timer granularity can overshoot a 6ms request to ~15.6ms,
	// which turned "render 10ms + sleep" into ~26ms/frame = ~38fps). We
	// therefore sleep only the bulk of the remaining time and spin the last
	// sub-millisecond. Slow frames are never throttled further.
	//
	// timeBeginPeriod is resolved dynamically so we don't add a winmm.lib
	// link dependency to the project.
	typedef UINT (WINAPI *PFN_TIMEBEGIN)(UINT);
	typedef UINT (WINAPI *PFN_TIMEEND)(UINT);
	HMODULE hWinmm = LoadLibraryA("winmm.dll");
	PFN_TIMEBEGIN pTimeBegin = hWinmm ? (PFN_TIMEBEGIN)GetProcAddress(hWinmm, "timeBeginPeriod") : NULL;
	PFN_TIMEEND   pTimeEnd   = hWinmm ? (PFN_TIMEEND)GetProcAddress(hWinmm, "timeEndPeriod") : NULL;
	if (pTimeBegin) pTimeBegin(1);  // 1ms timer resolution keeps Sleep(1) from overshooting
	LARGE_INTEGER qpf;
	QueryPerformanceFrequency(&qpf);
	const double FrameIntervalMs = 1000.0 / 60.0;
	const double MinimizedIntervalMs = 1000.0 / 15.0;

	// 限帧等待：等到本帧预算用完为止。
	// Sleep 在 Windows 上会过冲（即使 timeBeginPeriod(1)，Sleep(1) 实测常睡
	// 1.5~2ms），所以只让 Sleep 承担大块时间，最后 ~1ms 用 Sleep(0)/自旋收尾。
	// 之前用 "Sleep(remain-1.5) + Sleep(1)" 实测得到 18.06ms@目标16.67ms
	// （≈55fps）；这里把余量放大到 3ms，最后一段改成 Sleep(0) 轮询，把帧时间
	// 钉在目标值上。
	#define FRAME_PACE(frameStart, targetMs) do { \
		for (;;) { \
			LARGE_INTEGER _now; \
			QueryPerformanceCounter(&_now); \
			double _elapsed = (double)(_now.QuadPart - (frameStart).QuadPart) * 1000.0 / (double)qpf.QuadPart; \
			double _remain = (targetMs) - _elapsed; \
			if (_remain <= 0.10) break; \
			if (_remain > 4.0) Sleep((DWORD)(_remain - 3.0)); \
			else if (_remain > 2.0) Sleep(1); \
			else if (_remain > 0.10) Sleep(0); \
			else YieldProcessor(); \
		} \
	} while (0)

	// Main event loop
	while(g_running && !app->wantToQuit())
	{
		LARGE_INTEGER qpcFrameStart;
		QueryPerformanceCounter(&qpcFrameStart);
		++g_frameStamp;   // 新的一帧（重建预算据此重置）

		// libEGL (PowerVR sim) keeps renaming our window (PVRVFRAME, "M", ...)
		// from its own WndProc hook; re-pin the title every frame so it
		// always reads "MinecraftPE Community Edition".
		win32SetWindowTitle(hwnd, L"MinecraftPE Community Edition");
		// Do Windows stuff:
		while (PeekMessage (&sMessage, NULL, 0, 0, PM_REMOVE) > 0) {
			if(sMessage.message == WM_QUIT) {
				g_running = false;
				break;
			}
			else {
				TranslateMessage(&sMessage);
				DispatchMessage(&sMessage);
			}
		}
		LARGE_INTEGER qpcT0, qpcT1;
		QueryPerformanceCounter(&qpcT0);
		app->update();
		QueryPerformanceCounter(&qpcT1);
		g_diagUpdateMs = (double)(qpcT1.QuadPart - qpcT0.QuadPart) * 1000.0 / (double)qpf.QuadPart;

		// Hold the frame to its budget (60fps; ~15fps while minimized so the
		// world/network keep ticking without rendering invisible frames).
		if (g_running && app && !app->wantToQuit())
			FRAME_PACE(qpcFrameStart, g_windowMinimized ? MinimizedIntervalMs : FrameIntervalMs);

		// Diagnostics (only writes anything while the F3 overlay is open).
		{
			LARGE_INTEGER qpcEnd;
			QueryPerformanceCounter(&qpcEnd);
			double frameMs = (double)(qpcEnd.QuadPart - qpcFrameStart.QuadPart) * 1000.0 / (double)qpf.QuadPart;
			FrameProf::endFrame(g_diagUpdateMs, frameMs);
		}
	}

	#undef FRAME_PACE

	if (pTimeEnd && pTimeBegin) pTimeEnd(1);
	if (hWinmm) FreeLibrary(hWinmm);

	Sleep(50);
	delete app;
	Sleep(50);
	appContext.platform->finish();
	Sleep(50);
	delete appContext.platform;
	Sleep(50);
	//printf("_crtDumpMemoryLeaks: %d\n", _CrtDumpMemoryLeaks());
	
#ifndef STANDALONE_SERVER
	// Exit.
	eglMakeCurrent(appContext.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(appContext.display, appContext.context);
	eglDestroySurface(appContext.display, appContext.surface);
	eglTerminate(appContext.display);
#endif

	return 0;
}

#endif /*MAIN_WIN32_H__*/
