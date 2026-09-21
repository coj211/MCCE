#include <network/mco/RestRequestJob.hpp>
#include <util/Job.hpp>

// 适配实现：win32 本地 stub（不发起真实 HTTP 请求）
std::shared_ptr<RestRequestJob> RestRequestJob::CreateJob(RestRequestType a2, std::shared_ptr<RestService> a3, Minecraft* a4) {
	return std::shared_ptr<RestRequestJob>();
}
RestRequestJob::RestRequestJob() {}
void RestRequestJob::launchRequest(
	std::shared_ptr<RestRequestJob> a2,
	std::shared_ptr<ThreadCollection> a3,
	std::function<void(int32_t, const std::string&, const RestCallTagData&, std::shared_ptr<RestRequestJob>)> a4,
	std::function<void(bool, bool, int32_t, const std::string&, const RestCallTagData&, std::shared_ptr<RestRequestJob>)> a5) {}
void RestRequestJob::setBody(const std::string&) {}
void RestRequestJob::setTagData(const RestCallTagData&) {}
RestRequestJob::~RestRequestJob() {}
void RestRequestJob::stop() {}
void RestRequestJob::run() {}
void RestRequestJob::finish() {}
// setMethod 是模板成员：定义模板体并显式实例化空参数版本（PlayScreen 只用这个）
template<typename... _args>
void RestRequestJob::setMethod(const std::string& a2, _args... args) {}
template void RestRequestJob::setMethod<>(const std::string&);
// Job 基类析构（头文件只声明未实现）
Job::~Job() {}
