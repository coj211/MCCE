#pragma once
// 适配头：本移植为 0.8.1 Pocket Edition 客户端，不支持直连 Java 版服务器
struct JavaSession
{
	static const int PROTOCOL = 47; // 1.8.x，仅用于显示版本不符提示
};
