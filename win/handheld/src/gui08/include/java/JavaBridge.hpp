#pragma once
#include <_types.h>
#include <string>
struct Minecraft;

struct JavaBridge
{
	// 适配头：win32 版无 Java 版服务器连接能力
	static bool begin(Minecraft* mc, const std::string& name, const std::string& host, int port)
	{
		return false;
	}
};
