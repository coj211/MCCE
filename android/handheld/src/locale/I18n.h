#ifndef NET_MINECRAFT_LOCALE__I18n_H__
#define NET_MINECRAFT_LOCALE__I18n_H__

//package net.minecraft.locale;

#include <map>
#include <string>
#include "../AppPlatform.h"   // StringVector

class AppPlatform;
class ItemInstance;

class I18n
{
	typedef std::map<std::string, std::string> Map;
public:
	static void loadLanguage(AppPlatform* platform, const std::string& languageCode);

	static bool get(const std::string& id, std::string& out);
    static std::string get(const std::string& id);

	// 可用语言列表（平台枚举 data/lang/*.lang；平台不支持时回退内置 en_US/zh_CN）。
	static StringVector availableLanguages(AppPlatform* platform);
	// 某语言的显示名（读它的 language.name= 行，如 "English" / "简体中文"）。
	// 不切换当前语言：临时解析对应文件第一处 language.name。
	static std::string languageDisplayName(AppPlatform* platform, const std::string& code);

	// Runtime translation injection (used by the JS mod API to name
	// mod-defined items/blocks). 存在独立的一张表里：语言重载（loadLanguage）
	// 只清空语言文件表，这些运行时名字要一直有效。
	static void setTranslation(const std::string& id, const std::string& value);
    //static std::string get(const std::string& id, Object... args) {
    //    return lang.getElement(id, args);
    //}
	static std::string getDescriptionString( const ItemInstance& item );

private:
	static void fillTranslations(AppPlatform* platform, const std::string& filename, bool overwrite);
	static Map _strings;
	// 模组 / 插件运行时注入的翻译（setTranslation 写入；loadLanguage 不清）。
	static Map _runtimeStrings;
};

#endif /*NET_MINECRAFT_LOCALE__I18n_H__*/
