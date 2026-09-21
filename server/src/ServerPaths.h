#ifndef MINECRAFT_SEVER_SERVERPATHS_H__
#define MINECRAFT_SEVER_SERVERPATHS_H__

#include <string>

#ifdef _WIN32
// windows.h 默认会把老的 winsock.h 一起拉进来，而 RakNet 用的是 winsock2.h，
// 两者进同一个编译单元就会 "sockaddr 重定义"。所以这里先把 winsock 挡掉。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <windows.h>

//
// Windows 上"字符串编码"其实是三个世界，服务器必须自己统一，否则中文全是乱码：
//
//   1. C++ 源码、网络包、日志里传的是 **UTF-8**；
//   2. 文件系统内核用的是 **UTF-16**；
//   3. 窄字符 CRT（fopen / _findfirst / std::ifstream）用的是**本地代码页**
//      ——中文 Windows 上是 GBK。
//
// 所以约定：**对外一律 UTF-8**；凡是碰文件系统的调用点，先把 UTF-8 转成 UTF-16，
// 再走宽字符 API（_wfopen / _wfindfirst / std::ifstream(const wchar_t*)）。
// 这样中文文件名（`称号.js`）和中文玩家名（存档文件）才不会乱码或找不到文件。
//
namespace ServerPaths
{
	inline std::wstring toWide(const std::string& utf8)
	{
		if (utf8.empty())
			return std::wstring();
		int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
		if (n <= 0)
			return std::wstring();
		std::wstring w((size_t)n, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], n);
		return w;
	}

	inline std::string toUtf8(const std::wstring& w)
	{
		if (w.empty())
			return std::string();
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
		if (n <= 0)
			return std::string();
		std::string s((size_t)n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
		return s;
	}
}

#endif // _WIN32

#endif // MINECRAFT_SEVER_SERVERPATHS_H__
