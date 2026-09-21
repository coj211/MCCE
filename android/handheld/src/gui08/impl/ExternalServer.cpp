#include <ExternalServer.hpp>

// 适配实现：自 Multiplatform-1.6.4_classic 原版移植 + isJava 字段
ExternalServer::ExternalServer(const ExternalServer& a2) {
	this->field_0 = a2.field_0;
	this->field_4 = a2.field_4;
	this->field_8 = a2.field_8;
	this->field_C = a2.field_C;
	this->isJava = a2.isJava;
}
ExternalServer::ExternalServer() {
	this->field_0 = 0;
	this->isJava = false;
}
ExternalServer::ExternalServer(int32_t a2, const std::string& a3, const std::string& a4, int32_t a5) {
	this->field_0 = a2;
	this->field_4 = a3;
	this->field_8 = a4;
	this->field_C = a5;
	this->isJava = false;
}
ExternalServer::ExternalServer(int32_t a2, const std::string& a3, const std::string& a4, int32_t a5, bool_t java) {
	this->field_0 = a2;
	this->field_4 = a3;
	this->field_8 = a4;
	this->field_C = a5;
	this->isJava = java;
}
