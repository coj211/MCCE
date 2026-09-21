#ifndef NET_MINECRAFT_CLIENT__Minecraft_H__
#define NET_MINECRAFT_CLIENT__Minecraft_H__

#include "Options.h"
#ifndef STANDALONE_SERVER
#include "MouseHandler.h"
#endif
#include "Timer.h"
#include "player/input/ITurnInput.h"
#ifndef STANDALONE_SERVER
#include "gui/Gui.h"
#include "gui/screens/ScreenChooser.h"
#endif
//#include "../network/RakNetInstance.h"
#include "../world/phys/HitResult.h"

#include <map>

class User;
class Level;
class ServerLevel;
class LocalPlayer;
class IInputHolder;
class Mob;
class Player;
class LevelRenderer;
class GameRenderer;
class ParticleEngine;
class Entity;
class ICreator;
class GameMode;
class ModEngine;
class Textures;
class CThread;
class SoundEngine;
class Screen;
class Font;
class LevelStorageSource;
class BuildActionIntention;
class PerfRenderer;
class LevelSettings;
class IRakNetInstance;
struct ExternalServerFile;
struct MojangConnector;
class NetEventCallback;
class CommandServer;
struct PingedCompatibleServer;
//class ExternalFileLevelStorageSource;


#include "../App.h"
#include "PixelCalc.h"
class AppPlatform;
class AppPlatform_android;

class Minecraft: public App
{protected:
	Minecraft();
public:
	virtual ~Minecraft();

	void init();
	void setSize(int width, int height);
	void reloadOptions();

	bool supportNonTouchScreen();
	bool useTouchscreen();
	void grabMouse();
	void releaseMouse();

	void handleBuildAction(BuildActionIntention*);

	void toggleDimension(){}
	bool isCreativeMode();
	void setIsCreativeMode(bool isCreative);
	void setScreen(Screen*);

	virtual void selectLevel(const std::string& levelId, const std::string& levelName, const LevelSettings& settings);

	// "Ignore and continue" from the mod-guard warning screen: take over the
	// level object that selectLevel() built (and kept aside) instead of
	// deleting it, then finish the normal entry path (setLevel + creative
	// mode + running flag).
	void confirmWorldEntry();
	Level* _pendingLevel;   // world kept while the warning screen shows

	// Mod (dimension travel) sets this to force synchronous world
	// generation: scripted dimensions call into Duktape from chunk
	// generation, which must not happen on the CThread.

	virtual void setLevel(Level* level, const std::string& message = "", LocalPlayer* forceInsertPlayer = NULL);

	void generateLevel( const std::string& message, Level* level );
	LevelStorageSource* getLevelSource();

	bool isLookingForMultiplayer;
	void locateMultiplayer();
	void cancelLocateMultiplayer();
	bool joinMultiplayer(const PingedCompatibleServer& server, int a2 = 0);
	void hostMultiplayer(int port=19132);

	// ---- multiplayer per-player dimension worlds (host) ----
	// The host process may hold several server worlds at once (one per
	// dimension). dimension 0 / the world picked in selectLevel() is the
	// persistent main world; other dimensions are created lazily.
	Level* getOrCreateDimensionLevel(int dimensionId);
	// Host player dimension travel: swap the locally rendered world without
	// re-running the full world-entry pipeline. The player entity must
	// already have been moved into `newLevel` by the caller.
	void switchActiveLevel(Level* newLevel);
	// Host / local player dimension travel (mod API level.changePlayerDimension):
	// routes through the multiplayer server when present, else single-player.
	void teleportPlayerToDimension(int dimensionId, float x, float y, float z);

	// 本地玩家切换维度（mod API level.resetChunks / 单机维度传送）：把目标维度当成
	// "另一个世界"来进 —— 先保存当前世界，再打开（首次则是创建）该维度自己的存档
	// （主世界文件夹下的 dim<N>/ 子目录，自带 level.dat / 区块 / 玩家数据），然后走
	// 和"进入世界"完全相同的后台加载流程（后台线程 + 加载界面）。
	// 不再"清空区块缓存 + 换生成器 + 同步预生成"——那一步正是切维度卡顿的来源。
	// 返回 false 表示这个维度建不起来（没有对应生成器等）。
	bool beginDimensionTravel(int dimensionId, float x, float y, float z);
	// 进世界完成后：上次退出时在别的维度就自动切回去（落点读那个子世界的
	// 存档）。引擎接管“上次在哪个维度”，mod 不必自己再记。
	void maybeReturnToLastDimension();
	// 从某个维度子世界的存档里读“玩家在该维度的位置”（NBT 里的 Pos）。
	bool getDimensionSavedPos(int dimensionId, float& x, float& y, float& z);
	// Delete every server world in the dimension pool (leaving the game).
	void destroyDimensionWorlds();
	Player* respawnPlayer(int playerId);
	void respawnPlayer();
	void resetPlayer(Player* player);
	void doActuallyRespawnPlayer();

	void update();

	void tick(int nTick, int maxTick);
	void tickInput();

	bool isOnlineClient();
	bool isOnline();
	void pauseGame(bool isBackPaused);
	void gameLostFocus();

	void prepareLevel(const std::string& message);

	void leaveGame(bool renameLevel = false);

	int getProgressStatusId();
	const char* getProgressMessage();

	ICreator* getCreator();

	// void onGraphicsLost() {}
	void onGraphicsReset();

	bool isLevelGenerated();
	int getLicenseId();

    void audioEngineOn();
    void audioEngineOff();
    
	bool isPowerVR() { return _powerVr; }
	bool isKindleFire(int kindleVersion);
	bool transformResolution(int* w, int* h);
	void optionUpdated(const Options::Option* option, bool value);
	void optionUpdated(const Options::Option* option, float value);
	void optionUpdated(const Options::Option* option, int value);
#ifdef __APPLE__
    bool _isSuperFast;
    bool isSuperFast() { return _isSuperFast; }
#endif

protected:
	void _levelGenerated();

private:
	static void* prepareLevel_tspawn(void *p_param);

	void handleMouseClick(int button);
	void handleMouseDown(int button, bool down);

	void _reloadInput();
public:
	int width;
	int height;

	// Vars that the platform is allowed to use in the future
	int commandPort;
	int reserved_d1, reserved_d2;
	float reserved_f1, reserved_f2;

	Options options;

	static bool useAmbientOcclusion;
	//static bool threadInterrupt;

	volatile bool pause;

	LevelRenderer*  levelRenderer;
	GameRenderer*   gameRenderer;
	ParticleEngine* particleEngine;
	SoundEngine*    soundEngine;

	GameMode* gameMode;
#ifndef STANDALONE_SERVER
	Textures* textures;
	ScreenChooser screenChooser;
	Font* font;
#endif
#if defined(_WIN32) || defined(__ANDROID__)
	// JS mod 引擎（Duktape）：**客户端与独立服务器都持有** —— 同一份 mod zip
	// 双端加载，服务器跑权威逻辑、客户端跑表现。
	ModEngine* modEngine;
#endif
	IRakNetInstance*  raknetInstance;
	NetEventCallback* netCallback;
	// 0.8.1 GUI 移植：外部服务器文件 + MCO(Realms)连接器（MCO 为 stub，恒为 0）
	ExternalServerFile* externalServerFile;
	MojangConnector* mojangConnector;

	int lastTime;
	int lastTickTime;
	float ticksSinceLastUpdate;

	User*  user;
	Level* level;

	// Server world pool (multiplayer dimensions). Keyed by dimension id;
	// _dimensionLevels[d] holds every world the host process manages.
	Level* _mainWorld;              // persistent main world (the selectLevel world)
	std::string _mainLevelId;
	std::string _mainLevelName;
	std::map<int, Level*> _dimensionLevels;

	// 维度切换：目标世界后台加载完成后，本地玩家要落在哪个点。由
	// beginDimensionTravel() 设置、_levelGenerated() 消费。后台加载里的
	// resetPos() 只会把玩家往上抬出方块，不会还原任意落点，所以得自己记。
	bool  _hasPendingTravel;
	// 进游戏后的“自动切回上次维度”只做一次。
	bool  _dimAutoReturnDone;
	int   _pendingTravelDim;
	float _pendingTravelX, _pendingTravelY, _pendingTravelZ;

	LocalPlayer*	player;
	IInputHolder*	inputHolder;
	Mob*			cameraTargetPlayer;
#ifndef STANDALONE_SERVER
	Gui gui;
#endif
	CThread* generateLevelThread;
	Screen* screen;
	static int customDebugId;

	static const int CDI_NONE = 0;
	static const int CDI_GRAPHICS = 1;
#ifndef STANDALONE_SERVER
	MouseHandler mouseHandler;
#endif
	bool mouseGrabbed;

    PixelCalc pixelCalc;
    PixelCalc pixelCalcUi;

	HitResult hitResult;
	volatile int progressStagePercentage;

	// This field is initialized in main()
	// It sets the base path to where worlds can be written (sdcard on android)
	std::string externalStoragePath;
	std::string externalCacheStoragePath;
protected:
	Timer timer;
    // @note @attn @warn: this is dangerous as fuck!
	volatile bool isGeneratingLevel;
	bool _hasSignaledGeneratingLevelFinished;

	LevelStorageSource* storageSource;
	bool _running;
	bool _powerVr;

private:
	volatile int progressStageStatusId;
	static const char* progressMessages[];

	int missTime;
	int ticks;
	bool screenMutex;
	bool hasScheduledScreen;
	Screen* scheduledScreen;

	int _licenseId;
	bool _supportsNonTouchscreen;

	bool _isCreativeMode;
	//int _respawnPlayerTicks;
	Player* _pendingRemovePlayer; // @attn @todo @fix: remove this shait and fix the respawn behaviour

	PerfRenderer* _perfRenderer;
	CommandServer* _commandServer;
};

#endif /*NET_MINECRAFT_CLIENT__Minecraft_H__*/
