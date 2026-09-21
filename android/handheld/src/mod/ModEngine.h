#ifndef NET_MINECRAFT_MOD_MODENGINE_H__
#define NET_MINECRAFT_MOD_MODENGINE_H__

#include "../../thirdparty/duktape/duktape.h"

#include "../world/level/tile/ModBlockPart.h"
#include "../world/phys/AABB.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <cstring>

class Minecraft;
class Player;
class ItemInstance;
struct ScriptedBoxDef;
class ScriptedModel;
class Mob;
class Level;
class LevelChunk;
class Dimension;
class ScriptedMob;
class Model;
class Biome;

// 脚本给一个模型部件指定的姿态：旋转（弧度）+ 可选的位置。
// 位置一旦给了就是**绝对位置**（直接写部件的 x/y/z），用来做“躺下”这类需要
// 整体挪位的姿态 —— 只转角度做不到（每个部件是绕自己的点转的）。
// 模型空间：y 向下、-z 是前方、1 格 = 1/16 个方块。
struct ModBlockPart;   // 定义在 world/level/tile/ModBlockPart.h（这里只用指针/引用）

struct ModPartPose {
	float rx, ry, rz;      // 旋转（弧度）
	bool hasPos;
	float px, py, pz;      // 骨骼整体平移（格内单位）
	bool hasScale;         // 动画给了 scale —— 会**替换**骨骼的静态 scale
	float sx, sy, sz;
};

// 模型骨架里的一根骨骼。pivot 是格内 0..1 的旋转中心（与 ModBlockPart 同口径）。
// 骨骼动画（基岩 .animation.json 对 bone 打的关键帧）绕这个 pivot 旋转 / 整体平移 /
// 缩放，并沿 parent 链从根一路累积到每根骨骼，带动该骨骼下的所有部件。
//
// 关键语义：动画里的 rotation/scale 是**替换**骨骼自己的静态值，不是叠加。
// （基岩就是这么干的 —— hold 动画给 luger 的 [90,0,0] 和它 .geo.json 里的静态
//  [90,0,0] 是同一个值，叠加就变成 180°，模型整个翻过来。）
struct ModBoneDef {
	std::string name;
	std::string parent;	// 空 = 根骨骼
	float px, py, pz;	// 旋转中心（格内 0..1）
	float rx, ry, rz;	// 静态旋转（弧度）；动画没给 rotation 时用它
	ModBoneDef(): name(), parent(), px(0.5f), py(0.5f), pz(0.5f), rx(0), ry(0), rz(0) {}
};

// 一个动画方块的位置引用（几何每帧重画，不进区块网格）。
struct ModAnimBlockRef {
	int x, y, z, tileId;
};

// Argument marshalling for fireEvent (C++ -> JS).
static inline void modPushArg(duk_context* ctx, int v)              { duk_push_int(ctx, v); }
static inline void modPushArg(duk_context* ctx, unsigned int v)     { duk_push_uint(ctx, v); }
static inline void modPushArg(duk_context* ctx, float v)            { duk_push_number(ctx, v); }
static inline void modPushArg(duk_context* ctx, double v)           { duk_push_number(ctx, v); }
static inline void modPushArg(duk_context* ctx, bool v)             { duk_push_boolean(ctx, v); }
static inline void modPushArg(duk_context* ctx, const std::string& v) { duk_push_lstring(ctx, v.c_str(), v.size()); }
static inline void modPushArg(duk_context* ctx, const char* v)      { duk_push_string(ctx, v); }

// Metadata of one mod, parsed from the header comment of its .js file:
//   // name: My Mod
//   // author: Someone
//   // version: 1.0
//   // description: What it does
struct ModInfo {
	std::string file;        // file name inside the mods dir, e.g. "my.js"
	std::string name;        // display name (falls back to file name)
	std::string author;
	std::string version;
	std::string description;
	std::string cover;       // path of the first png under cover/ in the zip, e.g. "cover/banner.png" (empty = none)
	bool enabled;            // enabled in modlist.json
};

// JS mod engine wrapping Duktape. M2 scope: scan the mods directory, read
// mod metadata, install new mods from a file picker, toggle enable state
// (persisted to mods/modlist.json) and load enabled mods at startup.
//
// ── Runtime mod-log switch (performance) ─────────────────────────────────
// ModEngine::log() used to fopen + fprintf + fflush the log file on EVERY
// call, i.e. synchronous disk I/O from inside the frame. These flags gate it:
//   - g_modLogStartupPhase: true while mods load and the world is being
//     generated/loaded (startup diagnostics matter there), then turned off.
//   - g_modLogF3Debug: mirrors Minecraft::options.renderDebug, so opening the
//     F3 debug overlay re-enables file logging on demand.
// When both are false a log() call returns immediately (no I/O at all).
// The crash/terminate/signal handlers write the log themselves and are NOT
// gated, so crashes still leave a trace.
extern bool g_modLogStartupPhase;
extern bool g_modLogF3Debug;

class ModEngine {
public:
	typedef ModAnimBlockRef AnimBlockRef;

	ModEngine(Minecraft* mc);
	~ModEngine();

	// Create the Duktape heap and register built-in bindings.
	bool init();

	// Cross-mod communication (JS mods.comm.get/set): engine-side string
	// map shared by all isolated mod heaps.
	void commSet(const std::string& k, const std::string& v) { _comm[k] = v; }
	bool commGet(const std::string& k, std::string& out) const {
		std::map<std::string, std::string>::const_iterator it = _comm.find(k);
		if (it == _comm.end()) return false;
		out = it->second;
		return true;
	}

	// Scripted dimension JS callbacks (defined in the owning mod's heap as
	// __dim_<id>_height / __dim_<id>_bottom / __dim_<id>_chunkgen). The
	// chunk source calls these across every mod sandbox.
	float callDimensionHeight(int dimId, int x, int z);            // default 64
	float callDimensionBottom(int dimId, int x, int z);            // default 0
	bool callDimensionChunkGen(int dimId, int x, int z, LevelChunk* chunk);

	// Inject a package png into a spare gui/items.png slot for a custom item
	// icon; returns the 0-255 slot index (1 on failure).
	//
	// 注意：这个走的是 items.png，而 ItemRenderer 对 id < 256 的物品只用
	// terrain.png 采样图标槽，所以 Item.defineItem 的 png 图标请用下面那个
	// injectModBlockTexture（模组物品号一律 < 256）。
	int injectModItemIcon(const std::string& path, int itemId);
	// Inject a package png into a spare terrain-atlas slot and return the
	// slot index (256-511) the tile/item should use. Returns 1 on failure.
	// public：Item.defineItem 的 png 图标也走这里（见上面那条说明）。
	int injectModBlockTexture(const std::string& path, int blockId);
	// Overwrite a rectangular region of a vanilla atlas/texture with a
	// package png (used for resource-pack style overrides).
	bool replaceImage(const std::string& texPath, int dstX, int dstY, const std::string& srcPng);
	// Like replaceImage but the source png is nearest-neighbour scaled to
	// (scaleW x scaleH) before blitting (replaceBlockIcon uses this to fill
	// gui_blocks' 48x48 icon slots from any-size art).
	bool replaceImageScaled(const std::string& texPath, int dstX, int dstY, const std::string& srcPng, int scaleW, int scaleH);
	// Textures overridden via replaceImage; unloaded (reverted) at the
	// start of every mod reload so disabled mods restore vanilla assets.
	std::vector<std::string> _overriddenTextures;
	std::set<std::string> _commandNames;   // mod 注册的命令名（不含 '/'）
	// mod 注册命令的简述（Commands.register 的可选第三参数），给聊天输入框的
	// 指令补全列表显示用。老的只传两个参数的模组没有条目，查不到就是空串。
	std::map<std::string, std::string> _commandDescs;
	std::string commandDesc(const std::string& name) const {
		std::map<std::string, std::string>::const_iterator it = _commandDescs.find(name);
		return it == _commandDescs.end() ? std::string() : it->second;
	}

	// --- Player skin (主菜单纸娃娃皮肤选择) ---
	// Load an arbitrary png/jpg from disk, nearest-neighbour fit to the
	// 64x32 player skin, replace mob/char.png everywhere (in-game player +
	// main-menu doll), and remember the path in skins/current.txt so the
	// next launch re-applies it. Returns false on any failure.
	bool applyPlayerSkinFromFile(const std::string& imagePath);
	// Re-apply the skin saved in skins/current.txt (called once at startup).
	bool applySavedPlayerSkin();
	// No custom skin: upload vanilla mob/char.png as the upper half of a 64x64
	// canvas (player model is modern64 → needs a 64x64 texture to sample).
	bool applyDefaultSkinTo64();
	// Full path of the currently saved skin (skins/current.txt content), "" = none.
	static std::string currentSkinPath();
	// True if a saved skin exists and differs from the built-in char.png.
	static bool hasSavedSkin();
	// Remove the saved skin (back to vanilla char.png).
	void clearPlayerSkin();
	// Current skin is the 64x64 double-layer format (renders outer hat/jacket layer).
	static bool isPlayerSkin64() { return s_playerSkinIs64; }
	static void setPlayerSkin64(bool v) { s_playerSkinIs64 = v; }

	// --- UI override registry (resource-pack style vanilla-UI editing) ---
	// One entry per screen element ("screen.key", e.g. "mainmenu.button.2").
	// Mods declare overrides with UI.override/UI.setImage/UI.addButton;
	// entries live only while the mods are loaded (cleared on reload) so
	// disabling a mod restores the vanilla UI automatically.
	// 09b · nine-slice skin: 9 pngs (TL,T,TR,L,C,R,BL,B,BR). Corners drawn at
	// their own pixel size (they must all share the same pixel size), edges
	// stretch along their axis, centre stretches both ways.
	static const int kSkinParts = 9;  // order: TL,T,TR,L,C,R,BL,B,BR
	struct UiElementOverride {
		bool hasPos, hasSize, hasText, hasVisible, hasAlpha, hasImage;
		bool hasSkin;
		int x, y, w, h;
		std::string text;
		bool visible;
		float alpha;            // 0..1; <0 = unset
		std::string imagePath;  // zip-internal png for the element bg ("" = none)
		std::string skin[kSkinParts];  // nine-slice pngs ("" = part unused)
		UiElementOverride()
		: hasPos(false), hasSize(false), hasText(false), hasVisible(false),
		  hasAlpha(false), hasImage(false), hasSkin(false), x(0), y(0), w(0), h(0),
		  visible(true), alpha(-1.f), imagePath() {
			for (int i = 0; i < kSkinParts; ++i) skin[i].clear();
		}
	};
	struct UiAddedButton {
		std::string key;    // mod-unique id, also the stash callback key
		std::string text;
		std::string imagePath;  // optional png bg
		int x, y, w, h;
		UiAddedButton() : x(0), y(0), w(100), h(24) {}
	};
	struct UiScreenDef {
		// element key suffix ("button.2", "title", "header") -> props
		std::map<std::string, UiElementOverride> elements;
		std::vector<UiAddedButton> addButtons;  // extra buttons on this screen
	};
	std::map<std::string, UiScreenDef> _uiScreens;   // screen id -> defs
	// Get the merged override for screen+element key; false if none.
	bool uiQuery(const std::string& screen, const std::string& key, UiElementOverride& out) const;
	// Dispatch a click on a UI-added button (by its key) to the mod that
	// registered it (stash fn "__mod_ui_btn_<key>").
	void uiNotifyButtonClick(const std::string& key);

	// ── HUD 触摸按钮（移动版：屏幕上的模组按钮）──────────────────────────
	// 模组用 UI.addHudButton({key,x,y,w,h}) 注册一个 GUI 坐标的矩形（和
	// onGuiRender 里 ui.getWidth()/ui.fillRect 用的是同一套坐标；绘制仍由模组
	// 自己负责）。引擎在触摸落下时做命中判定：命中就派发 stash 回调
	// "__mod_hud_<key>_down" / "_up" / "_click"，并把这根手指「吃掉」——
	// mouseTaken(0) 会返回 true（不挖方块），TouchInputHolder::getCapturedPointer()
	// 也会返回它（不转视角）。手机上没有第二/第三个鼠标键，这是模组做
	// 「射击 / 换弹 / 瞄准」这类按钮的唯一通道。
	struct UiHudButton {
		std::string key;
		int x, y, w, h;
		UiHudButton() : x(0), y(0), w(48), h(48) {}
	};
	std::vector<UiHudButton> _hudButtons;
	const std::vector<UiHudButton>& hudButtons() const { return _hudButtons; }

	// ── 原版 HUD 控件的样式覆盖（UI.setControlStyle）──────────────
	// 让模组能改原版控件的位置/大小/隐藏/透明度/形状/颜色，例如把跳跃键缩小
	// 挪到屏幕边缘并画成半透明圆。可用 key（见 TouchscreenInput 里的注册）：
	//   "jump" "sneak" "fly_up" "fly_down"
	//   "dpad_up" "dpad_down" "dpad_left" "dpad_right" "joystick"
	struct ControlStyle {
		bool used;         // 模组动过这个控件
		bool hasRect;      // 模组指定了位置/大小（GUI 坐标）
		float x, y, w, h;
		bool hidden;       // 隐藏：不注册、不绘制、也不挡触摸
		bool hasOpacity;   // 图案/填充透明度 0..1
		float opacity;
		int shape;         // 0 = 原版贴图, 1 = 纯色矩形, 2 = 纯色圆环
		unsigned int fill;     // shape != 0 时的填充色（ARGB）
		unsigned int border;   // shape != 0 时的边框色（ARGB）
		float borderWidth;     // 边框粗细（GUI 像素）
		ControlStyle() : used(false), hasRect(false), x(0), y(0), w(0), h(0),
		                 hidden(false), hasOpacity(false), opacity(1.0f),
		                 shape(0), fill(0x60ffffff), border(0xffffffff), borderWidth(1.5f) {}
	};
	const ControlStyle* controlStyle(const std::string& key) const;
	void setControlStyle(const std::string& key, const ControlStyle& st);
	void clearControlStyle(const std::string& key);

	// 原版控件当前矩形（GUI 坐标）：TouchscreenInput 布局完推给引擎，
	// 供模组 UI.getControlRect("jump") 拿来自已算新布局。
	struct ControlRect { float x, y, w, h; bool visible; };
	void setControlRect(const std::string& key, float x, float y, float w, float h, bool visible);
	const ControlRect* controlRect(const std::string& key) const;
	// 命中判定（参数是 GUI 坐标）。命中返回 true 并写出 key。
	bool hudButtonAt(float gx, float gy, std::string* outKey) const;
	void hudButtonDown(const std::string& key);
	void hudButtonUp(const std::string& key);
	// ── HUD 按钮 / 按键映射：按指针记账 ──────────────────────
	// 手机能同时按多个 HUD 按钮，而“当前活跃按钮”只能存一个 —— 后按的会把
	// 先按的覆盖掉，先按的那个就永远等不到抬起（按钮一直亮、注入的鼠标键
	// 也一直不回收）。所以按下/抬起都按指针（pointer id）记账。
	void hudPress(int pointer, const std::string& key);
	void hudRelease(int pointer);
	bool hudPointerActive(int pointer) const;
	int  hudFirstPointer() const;            // 任一被占用的指针（-1 = 无）
	// HUD 布局版本：模组增删按钮时 +1，引擎据此重建触摸布局的排除区。
	int  hudLayoutVersion() const { return _hudLayoutVersion; }
	void markHudLayoutChanged() { _hudLayoutVersion++; }

	// ── HUD「按键映射」按钮 ─────────────────────────────────────────────
	// 手机上一根手指只能算一根手指（Multitouch 把 pointer0 镜像成鼠标左键），
	// 没法同时表达「左键按住 + 右键按住 + R」这类组合。所以让模组在屏幕上注册
	// 几块矩形，按住它们时引擎往输入系统里注入对应的鼠标键/键盘键：
	//     UI.addHudKey({key,x,y,w,h, map:'mouse0' | 'mouse1' | 'key:82'})
	// 注入后模组那边收到的还是标准的 onMouse / onKey（桌面那套逻辑一行都不用改）。
	// 命中期间这根手指同样被「吃掉」：不挖方块、不转视角。
	struct UiHudKey {
		std::string key;
		int x, y, w, h;
		int mapKind;    // 0 = 鼠标左键, 1 = 鼠标右键, 2 = 键盘键
		int mapCode;    // mapKind == 2 时的键码
		UiHudKey() : x(0), y(0), w(48), h(48), mapKind(0), mapCode(0) {}
	};
	std::vector<UiHudKey> _hudKeys;
	const std::vector<UiHudKey>& hudKeys() const { return _hudKeys; }
	bool hudKeyAt(float gx, float gy, std::string* outKey) const;
	void hudKeyDown(const std::string& key);
	void hudKeyUp(const std::string& key);
	// 当前被 HUD 映射按钮按住的鼠标键（触摸屏平台给模组的鼠标键只看它）
	bool hudMouseKeyHeld(int button) const { return ((_hudMouseHeld >> button) & 1) != 0; }
	// 模组显式接管/放开某个鼠标键（拿着枪时接管，原版就不挖方块了）
	void setMouseTaken(int button, bool take);
	// 禁用/重载模组时用：清掉所有 HUD 按钮/映射，并回收注入的鼠标键/键盘键
	void clearHudButtons();

	// Crash-diagnostics: last JS event + owning mod being dispatched when
	// the process aborts (read by the crash/signal handlers).
	std::string lastEventName;
	std::string lastEventMod;

	// Invoke an Item.defineItem onUse JS callback; true if a callback ran.
	bool callItemUseOn(int id, int x, int y, int z, int face);

	// --- Custom block lifecycle callbacks (stage 2) ---
	// Block.defineBlock options onTick/onUse/onPlace/onRemove/
	// onNeighborChanged/onStepOn are stored in the heap stash under
	// "__mod_tile_<evt>_<tileId>" by jsBlockDefineBlock. These helpers
	// invoke one of them (use returns JS's boolean = "consumed right click").
	bool hasTileCallback(int tileId, const char* evt) const;
	template <typename... Args>
	bool callTileEvent(int tileId, const char* evt, Args... args) {
		duk_context* ctx = (duk_context*)_ctx;
		if (!ctx) return false;
		// ModTile events carry the PHYSICAL tile id; JS callbacks are keyed
		// by the LOGICAL id the mod wrote in defineBlock (auto-allocation may
		// have moved the physical slot). Translate before building the key.
		int keyId = tileId;
		{
			std::map<int, int>::const_iterator it = _modBlockPhysId.find(tileId);
			if (it != _modBlockPhysId.end())
				keyId = it->second;
		}
		std::string key = std::string("__mod_tile_") + evt + "_" + std::to_string(keyId);
		duk_push_heap_stash(ctx);
		duk_get_prop_string(ctx, -1, key.c_str());
		if (!duk_is_function(ctx, -1)) {
			duk_pop_2(ctx);  // fn + stash
			return false;
		}
		int argc = 0;
		int dummy[] = { 0, (modPushArg(ctx, args), ++argc, 0)... };
		(void)dummy;
		if (duk_pcall(ctx, argc) != 0) {
			logThrottled(std::string("tile-") + evt, std::string("JS error in tile ") + evt + ": " + duk_safe_to_string(ctx, -1));
			duk_pop_2(ctx);  // err + stash
			return false;
		}
		bool consumed = duk_get_boolean(ctx, -1) != 0;
		duk_pop_2(ctx);  // result + stash
		return consumed;
	}

	// Stage 3: ModTileEntity per-tick forward to JS handler
	// __mod_tile_entity_tick(x, y, z) (single global callback shared by all
	// mod tile entities; the mod decides by coordinates which block it is).
	void callTileEntityTick(int x, int y, int z);

	// --- Stage 5: scripted screens (UI.openScreen) ---
	// Dispatch events from the open ScriptedScreen back to the JS callbacks
	// the mod registered with UI.openScreen (stored in the heap stash).
	void notifyScreenButton(int buttonId);
	void notifyScreenText(int inputId, const std::string& text);
	void notifyScreenClose();
	void notifyScreenRender();
	void notifyScreenCell(int cellId);
	// Stage 6: world environment controls.
	// Custom sky tint (setSkyColor); (-1,-1,-1) = use vanilla default.
	void setSkyColor(float r, float g, float b) { _skyR = r; _skyG = g; _skyB = b; }
	bool hasSkyColor(float& r, float& g, float& b) const {
		if (_skyR < 0) return false;
		r = _skyR; g = _skyG; b = _skyB; return true;
	}

	// Stage 5: screen opened from a JS callback, applied on the next tick
	// (so the caller's setScreen(NULL) doesn't clobber it).
	void setPendingScreen(class ScriptedScreen* s) { _pendingScreen = s; }

	// Scan the mods directory and return metadata for every .js found.
	// Re-reads modlist.json for enable state.
	std::vector<ModInfo> scanMods();

	// Comma-separated list of currently enabled mod names ("name(file)"),
	// used by the world mod-guard to detect missing/extra mods.
	std::string getEnabledModsJoined();

	// Show a world mod-guard warning as an in-game GUI message + log.

	// World mod-guard: compare the mod list recorded in level.dat against the
	// currently enabled mods; returns a warning naming missing/added mods or "".
	std::string checkWorldMods(const std::string& saved);
	// 当前启用的 mod 名列表（逗号分隔）——“世界守卫”与服务器“自动接受”共用
	std::string buildModNameList();

	// Copy a .js file into the mods directory and return the new file name.
	// Returns empty string on failure.
	std::string installModFile(const std::string& srcPath);

	// Enable or disable a mod by file name. Persists to modlist.json and
	// rebuilds the JS environment so the change takes effect immediately
	// (no restart needed). Returns false on failure.
	bool setEnabled(const std::string& file, bool enabled);

	// --- Multiplayer mod auto-sync ---
	// Enabled mod file names in modlist.json order.
	std::vector<std::string> getEnabledModFiles();
	// Size of mods/<file> in bytes, or -1 when missing/invalid.
	long modFileSize(const std::string& file);
	// Read / write a whole mod package under the mods directory.
	bool readModFileBytes(const std::string& file, std::vector<unsigned char>& out);
	bool writeModFileBytes(const std::string& file, const std::vector<unsigned char>& data);
	// Replace modlist.json "enabled" with `files` (order kept) and hot-reload
	// every mod so the sync takes effect immediately.
	bool applyEnabledList(const std::vector<std::string>& files);

	// Load every enabled mod (startup). Skips files that fail to parse/run.
	void loadEnabledMods();

	// Rebuild the whole Duktape heap and reload every enabled mod. Used so
	// toggling a mod's enable state applies right away.
	void reloadEnabledMods();

	// Execute one script file. Returns false if unreadable or it threw.
	bool runScript(const std::string& path);

	// --- 服务器命令（mod 注册）---
	// mod 用 JS 侧 `Commands.register(name, handler)` 注册命令；handler(sender, args)
	// 的返回值若是字符串，就作为回复发给执行命令的玩家。服务器收到 "/xxx" 聊天时
	// 先查这张表（见 ServerSideNetworkHandler::handleServerCommand）。
	void registerCommandName(const std::string& name);
	bool isRegisteredCommand(const std::string& name) const;
	// line 形如 "/heal Steve"（senderName = 执行者名字，服务器用玩家名）。
	// 返回 true 表示命令已被 mod 处理（reply 是要回给玩家的文本，可为空）。
	bool runRegisteredCommand(const std::string& line, const std::string& senderName, std::string& reply);

	// --- Mod packages (.zip: main.js + png resources) ---
	// Run main.js from a zip mod package and register every .png inside as
	// a GL texture (keyed by its path in the archive, e.g. "mob/golem.png").
	// ctx: target heap (NULL = main heap). Sandbox loading passes the mod's
	// own heap so its globals stay isolated from other mods.
	bool runScriptZip(const std::string& path, duk_context* ctx = NULL);
	// Look up a texture registered by a zip mod package (0 if unknown).
	unsigned int getModTexture(const std::string& path) const;
	// Look up the pixel size of a mod package png (false if unknown).
	bool getModTextureSize(const std::string& path, int& w, int& h) const;
	// Extract the first png under cover/ inside a zip mod and register it as
	// a GL texture (cached per mod file). Returns the GL texture id, or 0 if
	// the mod has no cover image. Used by the mod manager screen.
	unsigned int getModCoverTexture(const std::string& file);
	// Cover image pixel size (filled by getModCoverTexture when it decodes).
	// Returns false if the mod has no cover or it failed to decode.
	bool getModCoverSize(const std::string& file, int& w, int& h);

	// --- Custom blocks (M7) ---
	// JS Block.defineBlock: register a new tile + its tile item and add it
	// to the creative inventory. textureId is an index into terrain.png.
	// Id auto-allocation: if `id` is already taken (by a vanilla tile, another
	// mod, or a mod item sharing the id space), the next free slot is picked
	// so mods never collide. Returns the REAL (physical) tile id, or -1 on
	// failure. light: 0-15 emissive brightness (0 = not emissive).
	int defineBlock(int id, const std::string& name, int textureId, const std::string& texturePath, const std::string& material, int renderLayer = 0, int renderShape = 0, const std::string& topTex = "", const std::string& sideTex = "", const std::string& bottomTex = "", float destroyTime = 0.6f, bool requiresPickaxe = false, bool blockEntity = false, int light = 0);

	// --- Custom particles (M7) ---
	// JS Particle.define: register a particle with a texture from the mod
	// package, size, lifetime, color tint, gravity and shrink behaviour.
	struct ScriptedParticleDef {
		std::string name;
		std::string texturePath;  // path inside the zip, e.g. "particles/spark.png"
		unsigned int textureId;   // resolved GL texture id
		float size;
		int lifetime;
		float r, g, b;            // -1 = keep texture colors
		float gravity;
		bool shrink;
		ScriptedParticleDef()
		: name(), texturePath(), textureId(0), size(0.5f), lifetime(30),
		  r(-1.0f), g(-1.0f), b(-1.0f), gravity(0.0f), shrink(true) {}
	};
	void defineParticle(const ScriptedParticleDef& def);
	// Spawn a JS-defined particle; returns false if the name is unknown.
	bool spawnScriptedParticle(const std::string& name, float x, float y, float z, float xd, float yd, float zd);

	// --- Scripted dimensions (M7) ---
	// JS Dimension.define: register a dimension whose terrain is generated
	// by the mod's JS getHeight(x, z) callback.
	struct ScriptedDimensionDef {
		int id;
		std::string name;
		bool hasClouds;
		int grassId, dirtId, stoneId;
		ScriptedDimensionDef() : id(0), name(), hasClouds(true), grassId(2), dirtId(3), stoneId(1) {}
	};
	void defineDimension(int id, const std::string& name, bool hasClouds = true, int grassId = 2, int dirtId = 3, int stoneId = 1);
	// 自动挑一个没被占用的维度编号（主世界占 0 与 10，模组从 11 起）。
	int allocDimensionId() const;
	// 清掉群系分布查询缓存。模组的分布状态变了（比如 /hellbiome off）就调它 ——
	// 不然已算过并缓存的坐标还会返回旧结果。
	void clearBiomeCache();
	// 取消某个维度的群系分布回调（Biome.undistribution）。
	void undefineBiomeDistribution(int dimId);
	// 玩家跨群系时触发 onBiomeEnter / onBiomeLeave（客户端本地玩家；每 tick 调一次）。
	void tickBiomeTracking();
	// True if a JS generator (Dimension.define) exists for this id.
	bool hasDimensionGenerator(int id) const { return _scriptedDimensions.find(id) != _scriptedDimensions.end(); }
	// Surface block ids for a scripted dimension (grass/dirt/stone).
	void dimensionBlocks(int id, int& grass, int& dirt, int& stone) const;
	// Loading overlay: non-empty text = full-screen dim+text on top of
	// everything (dimension travel etc.).
	const std::string& loadingOverlay() const { return _loadingText; }
	void setLoadingOverlay(const std::string& t) { _loadingText = t; }

	// Generic per-world key-value state (mods/<levelName>.state), so mods
	// can persist small values like hunger across sessions.
	void setModState(const std::string& key, const std::string& value);
	std::string getModState(const std::string& key);

	// Dimension persistence: remember which dimension the world was in when
	// it was saved, so a restart can resume there. State is stored per
	// level name in the mods dir (dimstate_<level>.txt).
	void saveDimensionState();
	int loadDimensionState();
	void setCurrentDimension(int d) { _currentDim = d; }
	// Returns a ScriptedDimension for a JS-defined id, or NULL.
	Dimension* createScriptedDimension(int id);
	// (id, name) pairs for the world-creation UI.
	const std::map<int, ScriptedDimensionDef>& scriptedDimensions() const { return _scriptedDimensions; }

	// --- 模组群系（JS 侧 Biome.define）---
	// 一条刷怪表项（对应 Biome::MobSpawnerData：生物 typeId + 权重 + 数量范围）。
	struct BiomeSpawnEntry {
		int mob, weight, minCount, maxCount;
		BiomeSpawnEntry() : mob(0), weight(1), minCount(1), maxCount(1) {}
	};
	// 模组群系定义。方块 / 高度 / 刷怪 / 岩浆湖 / 天空色都能指定；
	// getHeight 与 decorate 是可选回调（存成全局函数，见 jsBiomeDefine）。
	struct ScriptedBiomeDef {
		int id;
		std::string name;
		int topMaterial, material;      // 表层 / 填充方块（-1 = 用原版默认 grass/dirt）
		int surfaceDepth;               // 填充厚度（<0 = 用原版公式）
		float heightScale;              // 地形起伏倍率（1 = 原版）
		float heightBias;               // 地形高度偏移（0 = 原版）
		float temperature, downfall;
		int skyColor;                   // <0 = 用温度算（原版行为）
		int lavaLakeChance;             // 0 = 不生成岩浆湖；N = 每区块 1/N 概率
		// 颜色（-1 = 用原版：原版群系的草地色是硬编码 0x339933、树叶按 data 三色、
		// 雾色用维度常量）
		int grassColor, foliageColor, fogColor;
		// 雪/冰：-1 = 原版；1 = 雪原（结冰 + 积雪）；0 = 强制不雪
		int snowOverride;
		int waterFogColor;              // -1 = 原版水下色
		// 植被（-1 = 原版；模组群系不给 = 不长原版植被）
		int treeCount, grassCount, flowerCount, mushroomChance, reedsCount, cactusCount;
		std::string treeKind;           // "" = 原版树；none/oak/birch/pine/spruce
		// 刷怪（-1 = 原版）
		float creatureProbability;
		int spawnYMin, spawnYMax, monsterLightMax;
		bool hasHeight, hasDecorate;
		std::vector<BiomeSpawnEntry> monsters, creatures, water;
		ScriptedBiomeDef()
		:	id(0), name(), topMaterial(-1), material(-1), surfaceDepth(-1),
			heightScale(1.0f), heightBias(0.0f), temperature(0.5f), downfall(0.5f),
			skyColor(-1), lavaLakeChance(0),
			grassColor(-1), foliageColor(-1), fogColor(-1),
			snowOverride(-1), waterFogColor(-1),
			treeCount(-1), grassCount(-1), flowerCount(-1), mushroomChance(-1),
			reedsCount(-1), cactusCount(-1), treeKind(),
			creatureProbability(-1.0f), spawnYMin(-1), spawnYMax(-1), monsterLightMax(-1),
			hasHeight(false), hasDecorate(false) {}
	};
	// 注册一个模组群系。requestedId <= 0 = 自动编号；返回实际编号，-1 = 失败。
	int defineBiome(int requestedId, const ScriptedBiomeDef& def);
	// 自动编号：已用最大编号 + 1，向后跳过被占用的（原版占 1..11）。
	int allocBiomeId() const;
	bool hasScriptedBiome(int id) const { return _scriptedBiomes.find(id) != _scriptedBiomes.end(); }
	const ScriptedBiomeDef* scriptedBiomeDef(int id) const {
		std::map<int, ScriptedBiomeDef>::const_iterator it = _scriptedBiomes.find(id);
		return it == _scriptedBiomes.end() ? NULL : &it->second;
	}
	const std::map<int, ScriptedBiomeDef>& scriptedBiomes() const { return _scriptedBiomes; }
	// 群系编号 -> Biome 实例（NULL = 不是模组群系）。
	Biome* scriptedBiome(int id);
	// 该维度挂了分布回调吗？（没挂 = 完全走原版温/雨查表，一格 JS 都不进）
	bool hasBiomeDistribution(int dimId) const { return _biomeDistDims.find(dimId) != _biomeDistDims.end(); }
	void markBiomeDistribution(int dimId) { _biomeDistDims.insert(dimId); }
	// 该坐标该用哪个模组群系（0 = 用原版）。按 4 格对齐缓存。
	int biomeIdAt(int dimId, int x, int z);
	// 群系自定义列高（getHeight 回调）；false = 该群系没定义。
	bool callBiomeHeight(int biomeId, int x, int z, int& heightOut);
	// 群系装饰回调（区块 postProcess 时调一次）；true = 模组自己装饰，跳过原版植被。
	bool callBiomeDecorate(int biomeId, int x, int z);
	// Mod-registered tile/item ids, cleared on reload so a toggled mod
	// doesn't leave stale Tile::tiles/Item::items entries behind.
	std::vector<int> _modTiles;
	std::vector<int> _modItems;

	// Block id auto-allocation (Stage A): mods keep writing their logical
	// id; if that physical slot is taken (vanilla / another mod / a mod item
	// sharing the id space) the next free slot is assigned. _modBlockPhysId
	// maps physical -> logical so ModTile events (which carry the physical
	// id) still find the JS callbacks (keyed by the logical id). Allocation
	// is deterministic for a given enabled-mod set (scan order fixed), so
	// physical ids are stable across reloads of the same combination.
	std::map<int, int> _modBlockPhysId;    // physical id -> logical id

	// logical id -> physical id snapshot loaded from the current world's
	// level.dat. defineBlock consults it first so a save's physical ids are
	// reused even after the enabled-mod combination changed.
	std::map<int, int> _blockSnapshot;

	// ── 自定义方块：动画登记表 + 微方块几何缓存 ──
	// 游戏刻计数（给 level.getTicks()）。
	long long _tickCount;
	// 动画方块坐标（上限 512；几何每帧重画，不写进区块网格）。
	std::vector<AnimBlockRef> _animBlocks;
	// 重建线程投递的"新看到的动画方块"，主线程消费。
	std::vector<AnimBlockRef> _animSeen;
	std::mutex _animMutex;
	// 微方块：打包坐标 -> 解析好的部件列表 / 其对应的原始字符串。
	// 只在字符串变了才重新解析；重建线程只读，不在调 JS。
	std::map<long long, std::vector<ModBlockPart> > _cellParts;
	std::map<long long, std::string> _cellPartsRaw;
	std::set<long long> _cellPending;
	std::mutex _cellPartsMutex;
	static long long packPos(int x, int y, int z);
	// 回调名：__mod_block<kind>_<logicalId>（存成"全局函数"，跨 heap 可查）。
	std::string blockCbKey(const char* kind, int tileId);
	// 解析一格几何字符串（'|' 分隔的部件描述）。
	static void parseCellGeomString(const std::string& s, std::vector<ModBlockPart>& out);
	// "<x y z w h d tex>" 一个部件：像素单位 -> 格内坐标。
	static bool parseCellPart(const std::vector<std::string>& tok, ModBlockPart& out);

	// Append a line to the mod log file.
	//
	// Runtime logging is OFF by default (see g_modLogStartupPhase /
	// g_modLogF3Debug below): every call used to fopen/fprintf/fflush the log
	// file synchronously, which is real in-frame disk I/O. Logs are written
	// during the startup phase (mod loading, world gen/load) and whenever the
	// player has F3 debug open; otherwise they are dropped.
	// Crash handlers write the log independently and are NOT affected.
	void log(const std::string& msg, bool force = false);
	// 模组显式打的日志（JS 的 modLog(...)）：不受"启动阶段 / F3"门控 ——
	// 模组主动要求记的就该记下来，同时每秒限流，免得刷爆磁盘。
	void modLog(const std::string& msg);
	// Same as log() but at most one entry per (event, 2s) window. Prevents
	// per-tick event errors (onTick, tile ticks) from flooding the log file
	// with disk writes, which was a major frame-spike source.
	void logThrottled(const std::string& event, const std::string& msg);

	// Duktape context (opaque).
	void* getContext() const { return _ctx; }

	// Ask JS whether the player may eat this food item (overrides the
	// vanilla "only while hurt" rule). Returns true when no mod overrides it.
	bool canEatFood(int itemId);

	// Ask JS whether the player may break this block / place a block here /
	// attack this entity. JS defines __mod_can_break / __mod_can_place /
	// __mod_can_attack global functions; absence means "allowed". These are
	// checked BEFORE the action happens so a mod can veto it.
	bool canBreakBlock(int x, int y, int z, int face);
	bool canPlaceBlock(int x, int y, int z, int face, int itemId);
	bool canAttack(int victimEntityId);

	// 鼠标事件：JS onMouse(button, down, heldMs) -> 引擎该不该把这次操作交给原版。
	// 语义和上面的 veto 一致：返回 false = 模组接手这个键（原版就别挖方块/放方块）。
	//   button 0 = 左键，1 = 右键（不是 MouseAction 的 1/2，模组侧用 0/1 更直观）
	//   down   1 = 按下，0 = 抬起
	//   heldMs 该键按住时长（毫秒）：按下那刻是 0，抬起时是总时长 —— 模组据此做长按
	// 只在按键【状态变化】时报一次（见 Minecraft::tickInput 的边沿检测），
	// 所以不会每 tick 进 JS。没人写 onMouse 时行为与改动前完全一致。
	bool fireMouseEvent(int button, bool down, int heldMs);
	// 某个鼠标键当前是不是正被模组接管（= 上次 fireMouseEvent 返回 true）。
	// 原版挖方块/放方块前查它 —— 接管了就跳过，免得开枪时顺手把方块挖了。
	bool mouseTaken(int button) const;

	// Ask JS whether this hit is a critical (target jumped within 1s).
	// Used by Player::attack to boost damage. Returns false when unhandled.
	bool isCriticalHit(int victimEntityId);

	// Ask JS whether this damage to the player should be blocked (i.e. the
	// player takes no damage). JS defines __mod_on_player_hurt(dmg, sourceId)
	// -> bool; absence = take damage normally. A blocking mod converts the
	// damage into sword durability and returns false.
	bool onPlayerHurtBlocked(int dmg, int sourceEntityId);

	// First-person item pose id set from JS via player.setItemPose(id).
	// 0 = normal, 1 = blocking stance (item held up left-front).
	int getItemPose() const { return _itemPose; }
	void setItemPose(int pose) { _itemPose = pose; }

	// Camera zoom set from JS via player.setZoom(z) —— 瞄准镜/开镜用。
	// z <= 1（或非法值）= 正常；z > 1 = 把投影视锥收窄 z 倍（真变焦，不是把画面
	// 拉伸放大）。客户端实现会顺手转给 GameRenderer::zoomRegion，那个状态本来
	// 就存在、只是从来没被调用过；副作用是 zoom != 1 时引擎跳过第一人称手部渲染
	// —— 开镜时正好看不见枪身。
	float getCameraZoom() const { return _cameraZoom; }
	void setCameraZoom(float z);

	// Fire a JS event to every mod that registered a handler for it (multi-
	// mod support: each mod's onTick/onChat/... runs independently). Falls
	// back to the global function if nothing was registered. Safe: missing
	// functions are no-ops and JS errors are logged without crashing.
	template <typename... Args>
	bool fireEvent(const char* name, Args... args) {
		// Duktape 不是线程安全的："世界加载线程"会在生成脚本维度地形时调
		// getHeight/onChunkGenerate，而主线程同时在跑 onTick。所有进 JS 的入口
		// 都在这把锁下面串行化（用递归锁，因为事件里可能再触发别的 JS 入口）。
		std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
		duk_context* ctx = (duk_context*)_ctx;
		if (!ctx) return false;
		// Fast path: if no mod ever registered a handler for this event
		// (checked at load time in registerModEvents), skip everything —
		// not even a heap-string allocation. onTick fires every game tick,
		// so this avoids 3+ Duktape stack ops + one std::string build per
		// sandbox per tick when idle.
		bool any = false;
		if (_registeredEvents.count(name)) {
			// Key is only needed once we know a handler exists.
			std::string key = std::string("__mod_evt_") + name;
			// Per-mod heaps: each mod's handlers live in its own heap stash.
			for (size_t s = 0; s < _sandboxes.size(); ++s) {
				duk_context* sctx = _sandboxes[s].ctx;
				if (!sctx) continue;
				duk_push_heap_stash(sctx);
				duk_get_prop_string(sctx, -1, key.c_str());
				if (!duk_is_array(sctx, -1)) {
					duk_pop_2(sctx);   // pop value + stash (fix: was leaking stash)
					continue;
				}
				int n = (int)duk_get_length(sctx, -1);
				for (int i = 0; i < n; ++i) {
					duk_get_prop_index(sctx, -1, i);
					if (duk_is_function(sctx, -1)) {
						lastEventName = name;
						lastEventMod = _sandboxes[s].name;
						int argc = 0;
						int dummy[] = { 0, (modPushArg(sctx, args), ++argc, 0)... };
						(void)dummy;
						if (duk_pcall(sctx, argc) != 0) {
							const char* err = duk_safe_to_string(sctx, -1);
							logThrottled(name, std::string("JS error in ") + name + ": " + err);
							duk_pop(sctx);
						} else {
							duk_pop(sctx);
						}
						any = true;
					} else {
						duk_pop(sctx);
					}
				}
				duk_pop_2(sctx);
			}
		}
		if (any)
			return true;
		// Fallback: unregistered global handler (single-mod behaviour).
		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, name);
		if (!duk_is_function(ctx, -1)) {
			duk_pop_2(ctx);  // fn + global (no stash pushed in this build)
			return false;
		}
		int n2 = 0;
		int dummy2[] = { 0, (modPushArg(ctx, args), ++n2, 0)... };
		(void)dummy2;
		if (duk_pcall(ctx, n2) != 0) {
			const char* err = duk_safe_to_string(ctx, -1);
			logThrottled(name, std::string("JS error in ") + name + ": " + err);
			duk_pop_2(ctx);  // err + global
			return false;
		}
		duk_pop_2(ctx);  // result + global
		return true;
	}

	// Process-wide singleton set by init(); lets non-Minecraft code
	// (GameMode, Screens, ...) dispatch events without a Minecraft ref.
	static ModEngine* instance;
	// 当前玩家皮肤是否为 64x64 双层(渲染玩家时决定 outer 层是否显示)。
	static bool s_playerSkinIs64;

	Minecraft* minecraft() const { return _minecraft; }

	// 事件上下文玩家：服务器触发事件（例如某玩家聊天触发的 onChat）时由调用方
	// 设置，让 mod 里的 player.getX()/getY()/… 指向“触发这个事件的玩家”，而不是
	// 本机玩家 —— 独立服务器没有本机玩家（mc->player 恒为 NULL），否则模组拿到的
	// 坐标永远是 (0,0,0)、生物会被生成在世界原点。
	void setEventPlayer(Player* p) { _eventPlayer = p; }
	Player* eventPlayer() const { return _eventPlayer; }

	// Path helper: mods dir relative to the working directory.
	static std::string modsDir() { return "mods"; }

	// Item ids registered by mods via Item.defineItem; appended to the
	// creative inventory when a player inventory is set up.
	const std::vector<int>& creativeItems() const { return _creativeItems; }
	void addCreativeItem(int id) { _creativeItems.push_back(id); }

	// --- Infrastructure (M-infra) ---
	// setTimeout/setInterval timers; tick() is called each game tick.
	void addTimer(int handle, const std::string& fnKey, int ms, bool repeat);
	void clearTimer(int handle);
	void tick();

	// Config persistence (mods/modconfig.json).
	std::string getConfig(const std::string& key, const std::string& def) const;
	void setConfig(const std::string& key, const std::string& value);

	// --- Block-id snapshot (persisted block-id stability) ---
	// Called when a world is loaded (before it generates): the level.dat
	// snapshot (logical -> physical) lets already-defined mod blocks move
	// back to the physical ids that world's saves were written with, so
	// changing the mod combination doesn't shift existing blocks.
	void applyBlockSnapshot(const std::map<int, int>& snapshot);
	// Called when a world saves: collect the current logical->physical
	// assignments so the snapshot stays fresh on disk.
	void snapshotBlockIds(std::map<int, int>& out) const;
	const std::map<int, int>& blockSnapshot() const { return _blockSnapshot; }

	// Last script error per mod file (displayed in the mod manager UI).
	std::string getModError(const std::string& file) const;
	void clearModErrors();

	// --- Scripted mobs (M6) ---
	// JS Mob.defineMob registers a mob def here; spawnMob / MobFactory create
	// ScriptedMob instances from it.
	struct ScriptedMobDef {
		std::string name;
		std::string texture;
		std::string animKey;   // JS global holding the anim callback
		int health;
		float sizeW, sizeH;
		float scale;           // model render scale (default 1)
		bool hostile;          // run Monster engine AI (seek+attack player) - default true
		std::vector<ScriptedBoxDef>* boxes;  // owned (see defineMob)
		ScriptedModel* model;                // owned
	};
	void defineScriptedMob(int typeId, const ScriptedMobDef& def);
	bool getScriptedMobDef(int typeId, std::string& texture, float& sizeW, float& sizeH, int& maxHealth, float& scale, bool& hostile, ScriptedModel*& model) const;
	Mob* createScriptedMob(Level* level, int typeId);
	// Spawns a scripted mob and returns the Entity (for the JS entity-
	// handle API: id = entity->entityId). Returns NULL on failure.
	Mob* spawnScriptedMob(int typeId, float x, float y, float z);
	// Per-frame animation: call the JS anim callback of a mob def and fetch
	// the returned rotation for one named part. Returns false if no anim.
	bool getAnimRotation(const std::string& animKey, const std::string& partName, float age, float time, float& rx, float& ry, float& rz);

	// ==================================================================
	// 自定义方块：模型 / 动画 / 微方块 / 放置
	// ==================================================================
	// 设置方块的几何部件（jsBlockDefineBlock 解析 box/height/model 后调用）。
	void setBlockModel(int tileId, const std::vector<ModBlockPart>& parts);
	// 记下模组方块的显示名（物理 id -> 名字）。
	void rememberBlockName(int physId, int logicalId, const std::string& name);
	// Block.setName(id, name)：给模组方块换显示名（物品栏 / 掉落物 / 聊天）。
	void setBlockName(int tileId, const std::string& name);
	// 把一个 png 路径注入 terrain 贴图集，返回槽号（部件贴图用）。
	int injectBlockTexture(const std::string& path, int blockId);

	// --- 动画方块 ---
	// 动画方块的几何每帧重画（不进区块网格），所以不做区块重建，姿态由
	// JS anim 回调每帧给出。登记时机：放置时 + 首次渲染时补登记（存档里
	// 已有的方块不走 onPlace）。
	bool registerAnimatedBlockAt(int tileId, int x, int y, int z);   // 返回 true = 新登记
	void unregisterAnimatedBlockAt(int x, int y, int z);
	void clearAnimatedBlocks();
	const std::vector<AnimBlockRef>& animatedBlocks() const { return _animBlocks; }
	// 任意线程：看到一个动画方块（区块重建时遇到）-> 投递给主线程登记。
	void noteAnimatedBlockSeen(int tileId, int x, int y, int z);
	// 主线程：消费投递队列 -> 登记 + 标脏（把它的静态几何从区块网格里去掉）。
	void pumpAnimatedBlocks();
	// 这一帧该部件要不要用动画姿态覆盖（true = out 有效）。
	bool getBlockPartPose(int tileId, const std::string& partName, int x, int y, int z, ModPartPose& out);
	// 世界时间（秒）（动画回调的 time 参数）。
	float blockAnimTime();

	// --- 模组物品的 3D 模型 / 动画（物品渲染器用）---
	// 有模型部件的物品返回部件表；没有模型（按 2D 图标画）返回 NULL。
	const std::vector<ModBlockPart>* itemModelParts(int itemId) const;
	// 这个物品挂了动画回调吗（手持渲染据此每帧重烘 VBO）。
	bool itemIsAnimated(int itemId) const;
	// 物品模型的独立大贴图路径（Item.defineItem 的 modelTexture）；NULL = 走 terrain 图集。
	const std::string* itemModelTexture(int itemId) const;

	// ---- 投射物（Projectile.defineProjectile / level.spawnProjectile）----
	// 投射物做成和方块/物品/生物同级的"可定义类型"：任何模组都能定义自己的投射物
	// （子弹、火球、飞刀……），模型走和方块/物品同一套 ModBlockPart。
	// 定义数据存在 ModEngine.cpp 的表里，这里只暴露按类型号取值的接口。
	const std::vector<ModBlockPart>* projectileModelParts(int typeId) const;  // NULL = 没给模型
	const std::string* projectileTexture(int typeId) const;                   // NULL = 走 terrain 图集
	bool  hasProjectileDef(int typeId) const;
	float projectileGravity(int typeId) const;
	float projectileDrag(int typeId) const;
	float projectileDamage(int typeId) const;
	int   projectileLife(int typeId) const;
	float projectileSize(int typeId) const;
	// 取出这一帧该部件的动画姿态（true = out 有效）。age/time 由调用方给。
	// firstPersonFlag: -1 = 不传第 4 参数（旧行为保持不变）；0/1 = 第三人称/第一人称。
	// 物品的第一人称手持渲染传 1 —— 模组才能把 first_person.* 和 third_person.*
	// 分开写（物品定义里的 anim 回调可选地接第 4 个布尔参数）。
	bool getItemPartPose(int itemId, const std::string& partName, float age, float time, ModPartPose& out,
	                     int firstPersonFlag = -1);
	// 骨骼动画：按骨架 bones + animKey 回调算出一帧姿态，写进 out（out 与 src 等长）。
	// 没有 bone 的部件行为与以前完全一致（仍然走 getItemPartPose 那套逐部件增量姿态），
	// 所以老模组一行不用改。bones 为空时整个函数就是旧行为。
	// firstPersonFlag: -1 = 不传第 4 参数（旧行为）；0/1 = 第三人称/第一人称。
	void applyItemAnim(const std::vector<ModBoneDef>& bones,
	                   const std::vector<ModBlockPart>& src,
	                   const std::string& animKey,
	                   float age, float time, int firstPersonFlag,
	                   std::vector<ModBlockPart>& out);
	// 物品版便捷入口：内部取该物品自己的骨架 / anim 回调。返回 false = 物品不存在
	// （out 仍被填成 src，调用方可以无脑用 out）。
	bool applyItemAnimFor(int itemId, const std::vector<ModBlockPart>& src,
	                      float age, float time, int firstPersonFlag,
	                      std::vector<ModBlockPart>& out);
	// 游戏刻计数（每 tick +1）。给模组做节流/计时用 —— 注意 `level.getTime()`
	// 返回的是存档里的世界时间（几乎不变），不能拿来计时。
	long long getTickCount() const { return _tickCount; }

	// --- 微方块（容器格：一个方块格里摆若干小方块）---
	// 格内几何存在方块实体的数据键 "geom" 里，是一串用 '|' 分隔的部件
	// 描述："x y z w h d tex"（像素单位，tex = 贴图槽号或 -1），可选后缀
	// " r rx ry rz"、“ m ox oz”（平移）、“ c”（裁剪 UV）。解析结果按原始
	// 串缓存 —— 重建可能在后台线程，这里全程只读字符串，不调 JS。
	bool getCellParts(int x, int y, int z, const std::vector<ModBlockPart>*& out);
	// 微方块的格内碰撞箱（由格内几何推出）。append 进 boxes；true = 已处理。
	bool getCellBoxes(int x, int y, int z, const AABB* box, std::vector<AABB>& boxes);
	// 主线程：处理"待补算"的微方块几何（读方块实体数据 -> 解析 -> 缓存 -> 标脏）。
	void pumpCellGeometry();
	// 某一格的微方块数据被改写（模组写了 "geom"）：缓存作废 + 排队重生 + 标脏。
	void noteCellGeomChanged(int x, int y, int z);

	// --- 放置 ---
	// placeData 回调：模组按"点在哪一面 + 点在方块上的位置"算数据值。
	bool callBlockPlaceData(int tileId, int x, int y, int z, int face, float cx, float cy, float cz, int itemValue, int& out);
	// 放置接管（事件 onPlaceAttempt）：true = 模组处理了这次放置，引擎不再走原逻辑。
	bool tryModPlace(Player* player, Level* level, int x, int y, int z, int face, float cx, float cy, float cz, ItemInstance* item);
	// 破坏接管（事件 onBreakAttempt）：true = 模组处理了这次破坏，整个方块保持原样
	// （微方块靠它只挖掉格内的一个小方块）。cx/cy/cz 是命中点在格内的位置 0..1。
	bool tryModBreakBlock(int x, int y, int z, int face, float cx, float cy, float cz);

	// --- Player actions (模组给玩家加动作，按玩家分别) ---
	// Player.defineAction(name, fn) 注册：把动作名映射到该 mod heap 里存回调
	// 的全局 key（和 defineMob 的 anim 一样存法：每个 mod 一个独立堆）。
	void definePlayerAction(const std::string& name, const std::string& animKey);
	// 给某个玩家挂动作（actionName 为空 = 取消）。会按需联网同步：房主/服务器
	// 广播，连服务器的客户端上报，单机本地生效。
	bool setPlayerAction(int playerId, const std::string& actionName);
	// 只改本地状态、不发包（收服务器广播 / 服务器自己记录时用）。
	void applyPlayerAction(int playerId, const std::string& actionName);
	// 这个玩家当前的动作名（没有 = 空串）。
	std::string getPlayerAction(int playerId);
	// 每帧渲染：该玩家这个部件要不要覆盖姿态。没有动作、或该动作不管这个
	// 部件，就返回 false（部件保持原版姿态）。
	bool getPlayerAnim(int playerId, const std::string& partName, float age, float time, bool firstPerson, ModPartPose& out);
	// 这个玩家有没有挂动作（渲染前先问一次：没挂就完全不进 JS）。
	bool hasPlayerAction(int playerId);
	// mod 重载时清空动作注册与挂载状态。
	void clearPlayerActions();

	// --- 玩家外形（模组：让玩家变成自定义生物的样子） ---
	// Player.setModel(id, typeId)：把玩家挂到一份外形定义（Mob.defineMob 定义过的，
	// 不用真的生成那只生物）。渲染时临时换上那份模型，画完还原；
	// 体型（碰撞箱）跟着外形定义里的 sizeW/sizeH 走。spec 为空 = 还原成人形。
	bool setPlayerModel(int playerId, const std::string& spec);
	void applyPlayerModel(int playerId, const std::string& spec);
	std::string getPlayerModel(int playerId);
	// 渲染热路径用：只要个 yes/no（不构造字符串）。
	bool hasPlayerModel(int playerId);
	// 收到同步包时一次性应用“动作 + 外形”（两边都不再回发，避免环）。
	void applyAppearance(int playerId, const std::string& actionName, const std::string& modelSpec);
	// 渲染用：该玩家此刻该用哪个模型 / 哪张贴图（没有外形 = NULL / 空串）。
	Model* getPlayerModelObject(int playerId);
	std::string getPlayerModelTexture(int playerId);

	// --- 模组用的 op 名单（每个世界一份，跟着存档走；原版游戏不用它） ---
	// 存在 <世界目录>/ops.txt：一行一个名字（UTF-8，# 开头的行忽略）。
	// 名单为空时，第一个进世界的玩家自动成为 op（否则模组没得判权限）。
	// 联机连别人服务器时名单只在内存里（那种情况该由服务器定权限）。
	std::set<std::string>& worldOps();
	bool isOp(const std::string& name);
	bool addOp(const std::string& name);
	bool removeOp(const std::string& name);
	std::string worldOpsPath();
	void saveWorldOps();

	// Spawn a registered scripted mob (JS level.spawnMob).


private:
	// 跨 heap 调用一个“部件动画”回调（生物 anim 与玩家动作共用）。
	// firstPersonFlag < 0 = 不传第 4 个参数（生物）；>= 0 = 把“是不是第一人称”
	// 当第 4 个参数传给 JS（玩家动作：第一人称手臂可以写得收敛些）。
	bool callAnimKey(const std::string& animKey, const std::string& partName, float age, float time, int firstPersonFlag, ModPartPose& out);

	Minecraft* _minecraft;
	Player* _eventPlayer;   // 事件上下文玩家（见 setEventPlayer）
	void* _ctx;             // duk_context* (main heap: config, engine-side ops)

	// One independent Duktape heap per enabled mod. Mods evaluate into their
	// own heap, so top-level vars/functions/event handlers never collide
	// between mods (no global namespace sharing). Event handlers and veto
	// callbacks are looked up per-mod across these heaps.
	struct ModSandbox {
		std::string name;   // mod zip name (display)
		duk_context* ctx;   // independent heap
	};
	std::vector<ModSandbox> _sandboxes;

	// Event names that at least one mod registered a handler for (filled by
	// registerModEvents at mod load). Lets fireEvent skip the per-sandbox
	// heap-stash scan entirely for events nobody listens to (onTick fires
	// every tick).
	std::set<std::string> _registeredEvents;

	std::string _logPath;
	std::vector<int> _creativeItems;
	float _skyR, _skyG, _skyB;  // stage 6: custom sky tint (-1 = default)
	int _itemPose = 0;          // first-person item pose (0 normal, 1 blocking)
	float _cameraZoom = 1.0f;   // player.setZoom()：>1 = 开镜变焦（客户端转 GameRenderer）

	// Register the built-in JS bindings (modLog, player, level, Mob, Item,
	// Recipes, timers, config) on the given heap. Called for the main heap
	// at init() and for every mod sandbox heap at load time.
	void registerBindings(duk_context* ctx);

	// Look up a global function named `name` across every mod sandbox and
	// call it with `args`/`argc` ints. Returns true if at least one mod
	// defined it and it ran without a fatal error; `outResult` (optional)
	// receives the boolean result of the FIRST defining mod. Used by veto
	// hooks, ModTile/ModItem callbacks, timers and crit/eat queries.
	bool callModGlobal(const char* name, const int* args, int argc, bool* outResult = NULL);

	// Veto query across ALL mod sandboxes: legacy fixed name + namespaced
	// variants (__mod_<mod>_<suffix>); any mod returning false blocks.
	bool vetoAll(const char* baseName, const int* args, int argc);

	// Dispatch a heap-stash callback (e.g. __mod_tile_tick_123) across the
	// main heap and every mod sandbox; each function found is called with
	// the given args. Returns true if at least one callback ran.
	template <typename... Args>
	bool dispatchStashAll(const char* key, Args... args) {
		bool any = false;
		duk_context* mctx = (duk_context*)_ctx;
		if (mctx) {
			duk_push_heap_stash(mctx);
			duk_get_prop_string(mctx, -1, key);
			if (duk_is_function(mctx, -1)) {
				int argc = 0;
				int dummy[] = { 0, (modPushArg(mctx, args), ++argc, 0)... };
				(void)dummy;
				if (duk_pcall(mctx, argc) != 0)
					logThrottled(key, std::string("JS error in ") + key + ": " + duk_safe_to_string(mctx, -1));
				else
					any = true;
				duk_pop_2(mctx);
			} else {
				duk_pop_2(mctx);
			}
		}
		for (size_t s = 0; s < _sandboxes.size(); ++s) {
			duk_context* ctx = _sandboxes[s].ctx;
			if (!ctx) continue;
			duk_push_heap_stash(ctx);
			duk_get_prop_string(ctx, -1, key);
			if (duk_is_function(ctx, -1)) {
				int argc = 0;
				int dummy[] = { 0, (modPushArg(ctx, args), ++argc, 0)... };
				(void)dummy;
				if (duk_pcall(ctx, argc) != 0)
					logThrottled(key, std::string("JS error in ") + key + ": " + duk_safe_to_string(ctx, -1));
				else
					any = true;
				duk_pop_2(ctx);
			} else {
				duk_pop_2(ctx);
			}
		}
		return any;
	}

	// True if a heap-stash function with `key` exists in any heap.
	bool hasStashFnAll(const char* key) const;

	// Timers
	struct ModTimer {
		int handle;
		std::string fnKey;
		int interval;
		long long nextMs;
		bool repeat;
	};
	std::vector<ModTimer> _timers;
	// Stage 5: screen opened from a JS callback, applied on the next tick
	// (so the caller's setScreen(NULL) doesn't clobber it).
	class ScriptedScreen* _pendingScreen;

	// Config
	std::map<std::string, std::string> _config;
	std::map<std::string, std::string> _comm;  // mods.comm shared map
	void loadConfig();
	void saveConfig();
private:

	// Per-mod script errors (file name -> error text)
	std::map<std::string, std::string> _modErrors;

	// Scripted mob registration table (typeId -> def)
	std::map<int, ScriptedMobDef> _scriptedMobs;

	// 玩家动作（模组）：动作名 -> 该 mod heap 里存回调的全局 key。
	std::map<std::string, std::string> _playerActionKeys;
	// 玩家动作（模组）：玩家 entityId -> 动作名（按玩家分别）。
	std::map<int, std::string> _playerActionByPlayer;
	// 玩家外形（模组）：玩家 entityId -> 外形标识（脚本生物 typeId 的十进制串；空 = 没外形）。
	std::map<int, std::string> _playerModelByPlayer;

	// 按实体 id 找玩家（本地玩家或连进来的远程玩家）。
	Player* findPlayerEntity(int playerId);
	// 外形定义里的碰撞箱尺寸（自定义生物）。
	bool scriptedModelSize(const std::string& spec, float& w, float& h);
	// 把这个玩家的“动作 + 外形”发给需要知道的人
	// （房主/服务器广播；连服务器的客户端上报）。
	void sendPlayerAppearance(int playerId);

	// 模组 op 名单（当前世界）。
	std::set<std::string> _worldOps;
	std::string _worldOpsWorld;   // 这份名单属于哪个世界（世界名变了就重读）
	bool _worldOpsValid;

	// mod 状态（level.setModState / getModState）的内存缓存 + 延迟落盘。
	// 原来是“每次 set 都要读整个文件再整份重写、每次 get 都要读一遍文件”——
	// mod 只要每 tick 调一次就是灾难（每帧多次磁盘 IO）。
	std::map<std::string, std::string> _modState;   // 当前世界的 key=value
	std::string _modStateWorld;
	bool _modStateLoaded;
	bool _modStateDirty;
	int _modStateTicksSinceWrite;
	void loadModStateIfNeeded();
	void flushModState(bool force);

	// Textures registered by zip mod packages (zip-internal path -> GL id).
	std::map<std::string, unsigned int> _modTextures;
	// Mod manager cover textures (mod file name -> GL id), extracted from the
	// package's cover/ folder and cached.
	std::map<std::string, unsigned int> _modCovers;
	// Mod manager cover sizes (mod file name -> w,h), filled when decoded.
	std::map<std::string, std::pair<int,int> > _modCoverSizes;
	// PCM data backing custom sounds (owned by the engine, lives as long as the mod).
	std::vector<std::vector<unsigned char> > _modSoundPcm;
	// Names of custom sounds registered into the SoundRepository (so a mod
	// reload can unregister them and avoid accumulating duplicates).
	std::vector<std::string> _modSoundNames;

	// Registered custom block display names (block id -> name).
	std::map<int, std::string> _blockNames;

	// JS-defined particles (name -> def).
	std::map<std::string, ScriptedParticleDef> _scriptedParticles;

	std::string _loadingText;
	// Duktape is re-entrant: remember the currently active dimension so
	// resetChunks(id) can persist it and onJoinWorld can restore it.
	int _currentDim;
	std::string _currentLevelName;

	// HUD 触摸按钮：指针 → 正在按住的按钮 key（可能多根手指同时按）
	std::map<int, std::string> _hudPointerKeys;
	// 模组给原版控件设的样式，以及原版控件当前矩形（GUI 坐标）
	std::map<std::string, ControlStyle> _controlStyles;
	std::map<std::string, ControlRect>  _controlRects;
	int _hudLayoutVersion;               // 模组增删 HUD 按钮时自增
	// HUD 按键映射按钮按住期间注入/回收的输入状态
	int _hudMouseHeld;                   // bit0 = 左键, bit1 = 右键
	std::vector<int> _hudKeyCodesHeld;   // 按住的键盘键码（抬起时要 feed(false)）

	// JS-defined dimensions (id -> def).
	std::map<int, ScriptedDimensionDef> _scriptedDimensions;

	// --- 模组群系 ---
	std::map<int, ScriptedBiomeDef> _scriptedBiomes;   // 编号 -> 定义
	std::map<int, Biome*> _scriptedBiomeObjs;          // 编号 -> ScriptedBiome 实例（本引擎持有）
	std::set<int> _biomeDistDims;                      // 挂了分布回调的维度
	// 群系分布查询缓存：key = packBiomeKey(dimId, x, z)（4 格对齐）-> 群系编号。
	std::map<long long, int> _biomeCache;
	static long long packBiomeKey(int dimId, int x, int z);
	// 跨群系事件用：上次看到的群系编号。-2 = 还没记录过（首次只记不触发）。
	int _lastBiomeId = -2;

	// Duktape 不是线程安全的：mod 的 JS（事件/回调）可能被主线程和"世界加载
	// 线程"同时调用（加载脚本维度地形时要跑 getHeight/onChunkGenerate，而主线程
	// 同时在跑 onTick）。所有进 JS 的入口（fireEvent / tick / callDimension* /
	// log）都在这把递归锁下面串行化。
	std::recursive_mutex _jsMutex;
	// Deferred dimension switch (consumed in tick()).

	// Decoded RGBA pixels of every png in the loaded packages (path -> data),
	// kept so custom block textures can be injected into the terrain atlas.
	struct ModPixelData {
		std::vector<unsigned char> rgba;
		int w, h;
	};
	std::map<std::string, ModPixelData> _modTexturePixels;
	// Texture path -> allocated terrain slot. Kept across mod reloads so a
	// reload reuses the same slot instead of burning a new one each time
	// (g_nextBlockSlot only ever decreases within a process).
	std::map<std::string, int> _modBlockSlotByPath;

	// Register a just-loaded mod's global event functions (from the given
	// sandbox heap) into its own per-event stash registries (multi-mod,
	// per-mod isolation).
	void registerModEvents(duk_context* ctx);

	// Evaluate raw JS source into a given heap (shared by runScript /
	// runScriptZip / sandbox loading).
	bool evalScriptInto(duk_context* ctx, const std::string& src, const std::string& displayName);

	// Evaluate raw JS source (main heap; config-side scripts).
	bool evalScript(const std::string& src, const std::string& displayName);
};

#endif /*NET_MINECRAFT_MOD_MODENGINE_H__*/
