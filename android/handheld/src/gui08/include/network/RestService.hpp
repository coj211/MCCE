#pragma once
#include <_types.h>
#include <map>
#include <string>
// 适配头：自 Multiplatform-1.6.4_classic 原版移植（仅本地 stub，不联网）
struct RestService
{
	std::map<std::string, std::string> cookieData;
	std::string serviceURL;

	RestService(const std::string&);
	std::map<std::string, std::string> getCookieData();
	std::string getCookieDataAsString();
	std::string* getSeriveURL();
	void setCookieData(const std::string&, const std::string&);
};
