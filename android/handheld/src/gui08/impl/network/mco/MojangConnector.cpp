#include <network/mco/MojangConnector.hpp>
#include <network/RestService.hpp>
#include <network/mco/MCOParser.hpp>

// 适配实现：win32 本地 stub（Realms/MCO 未启用，全部返回空/默认值）
MojangConnector::MojangConnector(struct Minecraft* a2) {
	this->minecraft = a2;
	this->status = STATUS_0;
	this->serverCreationEnabled = 0;
	this->serviceEnabled = 0;
}
void MojangConnector::clearLoginInformation() {}
std::shared_ptr<RestService> MojangConnector::getAccountSercice() { return std::shared_ptr<RestService>(); }
MojangConnectionStatus MojangConnector::getConnectionStatus() { return this->status; }
std::string MojangConnector::getEncryptedJoinDataString(long long, const std::string&, const std::string&) { return ""; }
std::string* MojangConnector::getJoinMCOPayload() { static std::string s; return &s; }
std::shared_ptr<LoginInformation> MojangConnector::getLoginInformation() { return std::shared_ptr<LoginInformation>(); }
std::shared_ptr<MCOParser> MojangConnector::getMCOParser() { return std::shared_ptr<MCOParser>(); }
std::shared_ptr<std::unordered_map<long long, MCOServerListItem>> MojangConnector::getMCOServerList() { return std::shared_ptr<std::unordered_map<long long, MCOServerListItem>>(); }
std::shared_ptr<RestService> MojangConnector::getMCOSercice() { return std::shared_ptr<RestService>(); }
std::string* MojangConnector::getServerKey() { static std::string s; return &s; }
std::shared_ptr<ThreadCollection> MojangConnector::getThreadCollection() { return std::shared_ptr<ThreadCollection>(); }
bool_t MojangConnector::isMCOCreateServersEnabled() { return this->serverCreationEnabled; }
bool_t MojangConnector::isServiceEnabled() { return this->serviceEnabled; }
void MojangConnector::setLoginInformation(const LoginInformation&) {}
void MojangConnector::setMCOCreateServersEnabled(bool_t a2) { this->serverCreationEnabled = a2; }
void MojangConnector::setMCOServerList(std::shared_ptr<std::unordered_map<long long, MCOServerListItem>> a2) { this->serverList = a2; }
void MojangConnector::setMCOServiceEnabled(bool_t a2) { this->serviceEnabled = a2; }
void MojangConnector::setPayload(const std::string&) {}
void MojangConnector::setServerKey(const std::string&) {}
void MojangConnector::setStatus(MojangConnectionStatus a2) { this->status = a2; }
void MojangConnector::updateUIThread() {}
std::string MojangConnector::urlEncode(std::string a2) { return a2; }
MojangConnector::~MojangConnector() {}
