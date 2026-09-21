// 0.8.1 GUI 移植：Util081 精简实现（0.8.1 util/Util.cpp 移植，utf8proc 依赖手写替代）
#include <_types.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <ctype.h>
#include <cstring>

#include "util/Util.hpp"

std::string Util081::EMPTY_STRING = "";

bool_t Util081::startsWith(const std::string& a1, const std::string& a2) {
	if (a2.size() > a1.size()) return 0;
	return a1.compare(0, a2.size(), a2) == 0;
}

bool_t Util081::endsWith(const std::string& a1, const std::string& a2) {
	if (a2.size() > a1.size()) return 0;
	return a1.compare(a1.size() - a2.size(), a2.size(), a2) == 0;
}

std::string* Util081::stringReplace(std::string& a1, const std::string& a2, const std::string& a3, int32_t a4) {
	std::string::size_type pos = 0;
	int32_t count = 0;
	while ((pos = a1.find(a2, pos)) != std::string::npos) {
		a1.replace(pos, a2.size(), a3);
		pos += a3.size();
		++count;
		if (a4 >= 0 && count >= a4) break;
	}
	return &a1;
}

int32_t Util081::hashCode(const std::string& s) {
	int32_t hash = 0;
	for (size_t counter = 0; counter < s.length(); ++counter) {
		int32_t c = s[counter];
		hash = 31 * hash + c;
	}
	return hash;
}

// UTF-8 前导字节长度
static int utf8CharLen(unsigned char c) {
	if (c < 0x80) return 1;
	if ((c & 0xE0) == 0xC0) return 2;
	if ((c & 0xF0) == 0xE0) return 3;
	if ((c & 0xF8) == 0xF0) return 4;
	return 1;
}

int32_t Util081::utf8len(const std::string& a1) {
	int32_t v1 = 0;
	size_t i = 0;
	while (i < a1.size()) {
		int cl = utf8CharLen((unsigned char)a1[i]);
		i += cl;
		++v1;
	}
	return v1;
}

std::string Util081::utf8substring(const std::string& a2, int32_t start, int32_t end) {
	std::string a1 = "";
	int32_t v8 = 0;
	size_t i = 0;
	while (i < a2.size()) {
		int cl = utf8CharLen((unsigned char)a2[i]);
		if (v8 >= start && (v8 < end || end < 0)) {
			a1.append(a2, i, cl);
		}
		i += cl;
		++v8;
		if (end >= 0 && v8 >= end) break;
	}
	return a1;
}

std::string Util081::simpleFormat(const std::string& a2, std::vector<std::string> a3) {
	std::string ret = "";
	int32_t curFmt = 0;
	bool_t fmt = 0;
	bool_t v8 = 0;
	for (int32_t i = 0; i < (int32_t)a2.length(); ++i) {
		if (fmt) {
			if (a2[i] != '%' && curFmt < (int32_t)a3.size()) {
				ret += a3[curFmt];
				++curFmt;
			}
			ret += a2[i];
			fmt = 0;
		} else if (v8) {
			ret += a2[i];
			v8 = 0;
		} else if (a2[i] == '\x7f') {
			ret += "\x7f";
			v8 = 1;
		} else if (a2[i] == '%') {
			fmt = 1;
		} else {
			ret += a2[i];
		}
	}
	if (fmt) {
		if (curFmt < (int32_t)a3.size()) {
			ret += a3[curFmt];
		}
	}
	return ret;
}

std::string Util081::toLower(const std::string& s) {
	std::string cp = s;
	std::transform(cp.begin(), cp.end(), cp.begin(), ::tolower);
	return cp;
}

void Util081::stringSplit(const std::string& a1, int32_t a2, const float* a3, std::function<void(const std::string&, float)> a4) {
	int32_t v4 = 0;
	float v5 = 0;
	int32_t v10 = -1;
	int32_t v11 = 0;
	while (v4 < (int32_t)a1.size()) {
		int32_t v12 = (uint8_t)a1[v4];
		v5 += a3[v12];
		if (v12 == ' ' || v12 == '\t') {
			v10 = v4;
		}
		if ((int)v5 > a2) {
			if (v12 != '\n') {
				if (v10 < 0) {
					--v4;
				}
				if (v10 >= 0) {
					v4 = v10;
				}
				std::string v15 = a1.substr(v11, v4 - v11 + 1);
				a4(v15, v5);
				v11 = v4 + 1;
				v10 = -1;
				v5 = 0;
				++v4;
				continue;
			}
		} else if (v12 != '\n') {
			++v4;
			continue;
		}
		std::string v15 = a1.substr(v11, v4 - v11);
		a4(v15, 0);
		v11 = v4 + 1;
		v10 = -1;
		v5 = 0;
		++v4;
	}
	std::string v15 = a1.substr(v11, v4 - v11);
	a4(v15, v5);
}

std::string Util081::stringTrim(const std::string& a2, const std::string& a3, bool_t a4, bool_t a5) {
	int32_t v8 = (int32_t)a2.length();
	int32_t v9 = (int32_t)a3.length();
	if (v8 && v9 && (a4 || a5)) {
		int32_t v10 = v8 - 1;
		int32_t i;
		if (a4) {
			for (i = 0; i < v8; ++i) {
				const char* c = std::find(a3.data(), a3.data() + a3.length(), a2[i]);
				if (c == a3.data() + a3.length()) break;
			}
		} else {
			i = 0;
		}
		if (a5) {
			while (v10 >= i) {
				const char* c = std::find(a3.data(), a3.data() + a3.length(), a2[v10]);
				if (c == a3.data() + a3.length()) break;
				--v10;
			}
		}
		return a2.substr(i, v10 - i + 1);
	} else {
		return "";
	}
}

std::string Util081::stringTrim(const std::string& a2) {
	return Util081::stringTrim(a2, " \t\n\r", 1, 1);
}
