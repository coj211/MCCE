#include <network/mco/MCOParser.hpp>
#include <network/mco/LoginInformation.hpp>
#include <network/mco/MCOServerListItem.hpp>

// 适配实现：win32 本地 stub（Realms 未启用）
void MCOParser::parseErrorMessage(const std::string&, std::string&, int32_t&) {}
void MCOParser::parseJoinWorld(const std::string&, std::string&, uint16_t&, std::string&) {}
LoginInformation MCOParser::parseMCOAccountValidSessionReturnValue(const std::string&) { return LoginInformation(); }
std::unordered_map<int64_t, MCOServerListItem> MCOParser::parseServerList(const std::string&) {
	return std::unordered_map<int64_t, MCOServerListItem>();
}
void MCOParser::parseStatus(const std::string&, bool&, bool& createServersEnabled, bool& serviceEnabled) {
	createServersEnabled = false;
	serviceEnabled = false;
}
