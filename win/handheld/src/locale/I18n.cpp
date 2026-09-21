#include "I18n.h"
#include <sstream>
#include "../AppPlatform.h"
#include "../util/StringUtils.h"
#include "../world/level/tile/Tile.h"
#include "../world/item/ItemInstance.h"
#include <ctype.h>

I18n::Map I18n::_strings;
// 运行时注入的翻译（模组自定义物品 / 方块名）。
// 单独一张表：loadLanguage() 会 _strings.clear()（启动和切语言时都会），
// 混在一起的话模组名字会被抹掉，界面上就显示成翻译键本身
// （形如 "tile.modblock_200.name<"）。
I18n::Map I18n::_runtimeStrings;

void I18n::loadLanguage( AppPlatform* platform, const std::string& languageCode )
{
	_strings.clear();
	fillTranslations(platform, "lang/en_US.lang", true);

	if (languageCode != "en_US")
		fillTranslations(platform, "lang/" + languageCode + ".lang", true);
}
bool I18n::get( const std::string& id, std::string& out ) {
	// 运行时注入优先（模组显式设的名字，且不受语言重载影响）。
	Map::const_iterator rit = _runtimeStrings.find(id);
	if (rit != _runtimeStrings.end()) {
		out = rit->second;
		return true;
	}
	Map::const_iterator cit = _strings.find(id);
	if (cit != _strings.end()) {
		out = cit->second;
		return true;
	}
	return false;
}

std::string I18n::get( const std::string& id )
{
	// 运行时注入优先（模组显式设的名字，且不受语言重载影响）。
	Map::const_iterator rit = _runtimeStrings.find(id);
	if (rit != _runtimeStrings.end())
		return rit->second;
	Map::const_iterator cit = _strings.find(id);
	if (cit != _strings.end())
		return cit->second;

	return id + '<';//lang.getElement(id);
}

StringVector I18n::availableLanguages(AppPlatform* platform) {
	StringVector codes;
	if (platform)
		codes = platform->listLanguageCodes();
	// 平台不支持枚举（或目录为空）→ 回退内置双语言，保证语言界面至少可切换。
	if (codes.empty()) {
		codes.push_back("en_US");
		codes.push_back("zh_CN");
	}
	return codes;
}

std::string I18n::languageDisplayName(AppPlatform* platform, const std::string& code) {
	if (!platform) {
		return code == "zh_CN" ? "简体中文" : code;
	}
	// 只读解析 lang/<code>.lang 的 language.name 行，不污染当前加载的语言。
	BinaryBlob blob = platform->readAssetFile("lang/" + code + ".lang");
	if (blob.data && blob.size > 0) {
		std::string data((const char*)blob.data, blob.size);
		delete[] blob.data;
		std::stringstream fin(data, std::ios_base::in);
		std::string line;
		while (std::getline(fin, line)) {
			if (line.compare(0, 14, "language.name=") == 0) {
				std::string name = Util::stringTrim(line.substr(14));
				if (!name.empty())
					return name;
				break;
			}
		}
	}
	return code;
}

void I18n::setTranslation( const std::string& id, const std::string& value )
{
	// 写进运行时表：loadLanguage() 清空的是语言文件表，这里的名字
	// （模组方块/物品名）在启动和切换语言之后都得留着。
	_runtimeStrings[id] = value;
}

void I18n::fillTranslations( AppPlatform* platform, const std::string& filename, bool overwrite )
{
	BinaryBlob blob = platform->readAssetFile(filename);
	if (!blob.data || blob.size <= 0)
		return;

	std::string data((const char*)blob.data, blob.size);
	std::stringstream fin(data, std::ios_base::in);

	std::string line;
	while( std::getline(fin, line) ) {
		int spos = line.find('=');
		if (spos == std::string::npos)
			continue;

		std::string key   = Util::stringTrim(line.substr(0, spos));
		Map::const_iterator cit = _strings.find(key);
		if (!overwrite && cit != _strings.end())
			continue;

		std::string value = Util::stringTrim(line.substr(spos + 1));
		// operator[] overwrites an existing key; map::insert would silently
		// keep the old value, breaking language overrides (zh_CN over en_US).
		_strings[key] = value;
	}

	delete[] blob.data;
}

std::string I18n::getDescriptionString( const ItemInstance& item )
{
	// Convert to lower. Normally std::transform would be used, but tolower might be
	// implemented with a macro in certain C-implementations -> messing stuff up
	const std::string desc = item.getDescriptionId();

	std::string s = desc;
	std::string trans;

	// Handle special cases
	if (item.id == Tile::cloth->id)
		return get(item.getAuxValue()? "desc.wool" : "desc.woolstring");
	else if (item.id == Tile::fenceGate->id)
		return I18n::get("desc.fence");
	else if (item.id == Tile::stoneSlabHalf->id)
		return I18n::get("desc.slab");

	for (unsigned int i = 0; i < s.length(); ++i)
		s[i] = ::tolower(s[i]);

	// Replace item./tile. with desc., hopefully it's enough
	if (s[0] == 't') s = Util::stringReplace(s, "tile.", "desc.");
	if (s[0] == 'i') s = Util::stringReplace(s, "item.", "desc.");
	if (I18n::get(s, trans))
		return trans;

	// Remove all materials from the identifier, since swordWood should
	// be read as just sword
	const char* materials[] = {
		"wood",
		"iron",
		"stone",
		"diamond",
		"gold",
		"brick",
		"emerald",
		"lapis",
		"cloth"
	};

	Util::removeAll(s, materials, sizeof(materials) / sizeof(const char*));
	if (I18n::get(s, trans))
		return trans;

	std::string mapping[] = {
		"tile.workbench",	"craftingtable",
	};
	const char numMappings = sizeof(mapping) / sizeof(std::string);
	for (int i = 0; i < numMappings; i += 2) {
		if (desc == mapping[i]) {
			if (I18n::get("desc." + mapping[i+1], trans))
				return trans;
		}
	}

	return desc + " : couldn't find desc";
}
