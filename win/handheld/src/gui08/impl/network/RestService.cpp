#include <network/RestService.hpp>

// 适配实现：win32 本地 stub，不发起真实 HTTP 请求
RestService::RestService(const std::string& a2) {
	this->serviceURL = a2;
}
std::map<std::string, std::string> RestService::getCookieData() {
	return this->cookieData;
}
std::string RestService::getCookieDataAsString() {
	std::string ret;
	for (auto& kv : this->cookieData) {
		if (!ret.empty()) ret += "; ";
		ret += kv.first + "=" + kv.second;
	}
	return ret;
}
std::string* RestService::getSeriveURL() {
	return &this->serviceURL;
}
void RestService::setCookieData(const std::string& a2, const std::string& a3) {
	this->cookieData[a2] = a3;
}
