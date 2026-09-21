#pragma once
#include <_types.h>
#include <string>

struct JavaPingResult
{
	enum State { ONLINE, OFFLINE, LOADING };
	State state;
	int online;
	int max;
	int protocol;
	std::string version;
};

struct JavaPing
{
	// 适配头：win32 版无 Java 服务器支持，直接视为离线
	static JavaPingResult get(const std::string& host, int port)
	{
		JavaPingResult r;
		r.state = JavaPingResult::OFFLINE;
		r.online = 0;
		r.max = 0;
		r.protocol = 0;
		r.version = "";
		return r;
	}
};
