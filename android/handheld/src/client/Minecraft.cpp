#include "Minecraft.h"
#include "../locale/I18n.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/OptionsScreen.h"
#include "gui/screens/ModWarningScreen.h"
#include "gui/screens/ProgressScreen.h"
#endif
#if (defined(_WIN32) || defined(__ANDROID__)) && !defined(STANDALONE_SERVER)
#include "../mod/ModEngine.h"
#endif

#if defined(APPLE_DEMO_PROMOTION)
    #define NO_NETWORK
#endif

#if defined(RPI)
	#define CREATORMODE
#endif

#include "../network/RakNetInstance.h"
#include <ExternalServerFile.hpp> // 0.8.1 GUI 移植：外部服务器文件
#include "../network/ClientSideNetworkHandler.h"
#include "../network/ServerSideNetworkHandler.h"
//#include "../network/Packet.h"
#include "../world/entity/player/Inventory.h"
#include "../world/level/chunk/ChunkCache.h"
#include "../world/level/tile/Tile.h"
#include "../world/level/storage/LevelStorageSource.h"
#include "../world/level/storage/LevelStorage.h"
#include "../world/level/storage/ExternalFileLevelStorage.h"
#include "player/input/KeyboardInput.h"
#ifndef STANDALONE_SERVER
#include "player/input/touchscreen/TouchInputHolder.h"
#endif
#include "player/LocalPlayer.h"
#include "gamemode/CreativeMode.h"
#include "gamemode/SurvivalMode.h"
#include "player/LocalPlayer.h"
#ifndef STANDALONE_SERVER
#include "particle/ParticleEngine.h"
#include "gui/Screen.h"
#include "gui/Font.h"
#include <gui/screens/RenameMPLevelScreen.hpp>
#include "sound/SoundEngine.h"
#endif
#include "../platform/CThread.h"
#include "../platform/input/Mouse.h"
#include "../AppPlatform.h"
#include "../Performance.h"
#include "../LicenseCodes.h"
#include "../util/PerfTimer.h"
#include "../util/FrameProf.h"
#include "../util/PerfRenderer.h"
#include "player/input/MouseBuildInput.h"

#include "../world/Facing.h"

#include "../network/packet/PlaceBlockPacket.h"

#include "player/input/IInputHolder.h"
#ifndef STANDALONE_SERVER
#include "player/input/touchscreen/TouchscreenInput.h"

#include "player/input/ControllerTurnInput.h"
#include "player/input/XperiaPlayInput.h"

#endif

#include "player/input/MouseTurnInput.h"
#include "../world/entity/MobFactory.h"
#include "../world/level/MobSpawner.h"
#include "../util/Mth.h"
#include "../network/packet/InteractPacket.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/PrerenderTilesScreen.h"
#include "renderer/Textures.h"
#include "gui/screens/DeathScreen.h"
#endif

#include "../network/packet/RespawnPacket.h"
#include "IConfigListener.h"
#include "../world/entity/MobCategory.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/FurnaceScreen.h"
#endif
#include "../world/Difficulty.h"
#include "../server/ServerLevel.h"
#if (defined(_WIN32) || defined(__ANDROID__)) && !defined(STANDALONE_SERVER)
#include "../mod/ModEngine.h"
#endif
#include "../world/level/dimension/Dimension.h"
#ifdef CREATORMODE
#include "../server/CreatorLevel.h"
#endif
#include "../network/packet/AdventureSettingsPacket.h"
#include "../network/packet/SetSpawnPositionPacket.h"
#include "../network/command/CommandServer.h"
#include "gamemode/CreatorMode.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/ArmorScreen.h"
#endif
#include "../world/level/levelgen/synth/ImprovedNoise.h"
#ifndef STANDALONE_SERVER
#include "renderer/tileentity/TileEntityRenderDispatcher.h"
#endif

#ifndef STANDALONE_SERVER
#include "renderer/ptexture/DynamicTexture.h"
#include "renderer/GameRenderer.h"
#include "renderer/ItemInHandRenderer.h"
#include "renderer/LevelRenderer.h"
#include "renderer/entity/EntityRenderDispatcher.h"
#include "gui/Screen.h"
#include "gui/Font.h"
#include <gui/screens/RenameMPLevelScreen.hpp>
#include "sound/SoundEngine.h"
#endif

static void checkGlError(const char* tag) {
#ifdef GLDEBUG
	while (1) {
        const int errCode = glGetError();
        if (errCode == GL_NO_ERROR) break;

		LOGE("################\nOpenGL-error @ %s : #%d\n", tag, errCode);
	}
#endif /*GLDEBUG*/
}

/*static*/
const char* Minecraft::progressMessages[] = {
	"Locating server",
	"Building terrain",
	"Preparing",
	"Saving chunks"
};

int Minecraft::customDebugId = Minecraft::CDI_NONE;

#if defined(_MSC_VER)
	#pragma warning( disable : 4355 ) // 'this' pointer in initialization list which is perfectly legal
#endif

bool Minecraft::useAmbientOcclusion = false;

Minecraft::Minecraft()
:	user(NULL),
	level(NULL),
	_mainWorld(NULL),
	_dimAutoReturnDone(false),
	player(NULL),
	cameraTargetPlayer(NULL),
	_pendingLevel(NULL),  // 必须初始化：未初始化会在 selectLevel 里 delete 垃圾指针随机崩
	levelRenderer(NULL),
	gameRenderer(NULL),
#ifndef STANDALONE_SERVER
	particleEngine(NULL),
	_perfRenderer(NULL),
#endif
	_commandServer(NULL),
#ifndef STANDALONE_SERVER
	textures(NULL),
#endif
	lastTickTime(-1),
	lastTime(0),
	ticksSinceLastUpdate(0),
	gameMode(NULL),
	mouseGrabbed(true),
	missTime(0),
	pause(false),
	_running(false),
	timer(20),
#ifndef STANDALONE_SERVER
	gui(this),
#endif
	netCallback(NULL),
#ifndef STANDALONE_SERVER
	screen(NULL),
	font(NULL),
#endif
	screenMutex(false),
#ifndef STANDALONE_SERVER
	scheduledScreen(NULL),
	hasScheduledScreen(false),
	soundEngine(NULL),
#endif
	ticks(0),
	isGeneratingLevel(false),
	_hasSignaledGeneratingLevelFinished(true),
	generateLevelThread(NULL),
	progressStagePercentage(0),
	progressStageStatusId(0),
	isLookingForMultiplayer(false),
	_licenseId(LicenseCodes::WAIT_PLATFORM_NOT_READY),
	inputHolder(0),
	_supportsNonTouchscreen(false),
#ifndef STANDALONE_SERVER
	screenChooser(this),
#endif
	width(1), height(1),
	//_respawnPlayerTicks(-1),
#ifdef __APPLE__
    _isSuperFast(false),
#endif
	_powerVr(false),
	commandPort(4711),
	reserved_d1(0),reserved_d2(0),
	reserved_f1(0),reserved_f2(0),
	externalServerFile(0),
	mojangConnector(0)
{
//#ifdef ANDROID

#if defined(NO_NETWORK)
    raknetInstance = new IRakNetInstance();
#else
	raknetInstance = new RakNetInstance();
#endif
	// 0.8.1 GUI 移植：外部服务器文件（存于运行目录 external_servers.txt）
#ifndef STANDALONE_SERVER
	externalServerFile = new ExternalServerFile(".");
	externalServerFile->load();
#endif
	// 0.8.1 背包移植：Options 全局单例（CreativeInventoryScreen 等读取）
	Options::instance = &options;
#ifndef STANDALONE_SERVER
	soundEngine = new SoundEngine(20.0f);
	soundEngine->init(this, &options);
#endif
	//setupPieces();
}

Minecraft::~Minecraft()
{
	delete netCallback;
	delete raknetInstance;
#ifndef STANDALONE_SERVER
	delete levelRenderer;
	delete gameRenderer;
	delete particleEngine;

	delete soundEngine;
#endif
	delete gameMode;
#ifndef STANDALONE_SERVER
	delete font;
	delete textures;
#if defined(_WIN32) || defined(__ANDROID__)
	delete modEngine;
	modEngine = NULL;
#endif

	if (screen != NULL) {
		delete screen;
		screen = NULL;
	}
#endif
	if (level != NULL) {
		level->saveGame();
		if (level->getChunkSource())
			level->getChunkSource()->saveAll(true);
		delete level->getLevelStorage();
		delete level;
		level = NULL;
	}

	//delete player;
	delete user;
	delete inputHolder;

	delete storageSource;
	delete _perfRenderer;
	delete _commandServer;

	MobFactory::clearStaticTestMobs();
#ifndef STANDALONE_SERVER
	EntityRenderDispatcher::destroy();
#endif
}

// Only called by server
void Minecraft::selectLevel( const std::string& levelId, const std::string& levelName, const LevelSettings& settings )
{
	// Drop any level left over from a previous mod-guard block (e.g. the
	// player dismissed the warning screen and picked another world).
	if (_pendingLevel) {
		delete _pendingLevel;
		_pendingLevel = NULL;
	}

	// Remember which world this process treats as the persistent main world.
	_mainLevelId = levelId;
	_mainLevelName = levelName;
#if defined(CREATORMODE)
	level = new CreatorLevel(
#else
	// Mod-defined dimension (Dimension.define) can be picked at world
	// creation; getNew returns NULL for unknown ids.
	Dimension* fixedDim = (settings.getDimensionId() != 0) ? Dimension::getNew(settings.getDimensionId()) : NULL;
	level = new ServerLevel(
#endif
		storageSource->selectLevel(levelId, false),
		levelName,
		settings,
		SharedConstants::GeneratorVersion,
		fixedDim);

	// World mod-guard: check the recorded mod list BEFORE the world starts
	// generating/loading. If the enabled mod set differs from what this
	// world was created with, block entry entirely (no level generation, no
	// game thread) and show the warning screen instead.
#if defined(_WIN32) || defined(__ANDROID__)
	if (modEngine && !level->getLevelData()->getModList().empty()) {
		std::string warning = modEngine->checkWorldMods(level->getLevelData()->getModList());
		if (!warning.empty()) {
#ifndef STANDALONE_SERVER
			modEngine->log("[worldguard] blocking world entry (before load)");
			// Keep the freshly built level aside; the warning screen's
			// "ignore and continue" takes it over via confirmWorldEntry().
			_pendingLevel = level;
			level = NULL;
			setScreen(new ModWarningScreen(warning));
			return;
#else
			// 服务器是唯一权威：存档 mod 列表与当前不一致时直接写回存档
			modEngine->log("[worldguard] " + warning + " -> 按当前 mod 列表更新存档");
			if (level && level->getLevelData()) {
				level->getLevelData()->setModList(modEngine->buildModNameList());
				level->saveLevelData();
			}
#endif
		}
	}
#endif

	// note: settings is useless beyond this point, since it's
	//       either copied to LevelData (or LevelData read from file)
	setLevel(level, "Generating level");
	setIsCreativeMode(level->getLevelData()->getGameType() == GameType::Creative);
	_running = true;

	// The selected world becomes the persistent main world (dimension pool).
	_mainWorld = level;
	_dimensionLevels.clear();
	_dimensionLevels[level->dimension ? level->dimension->id : 0] = level;
}

void Minecraft::confirmWorldEntry() {
	if (!_pendingLevel)
		return;
	level = _pendingLevel;
	_pendingLevel = NULL;
	LOGI("confirmWorldEntry: entering world despite mod list mismatch");
	setLevel(level, "Generating level");
	setIsCreativeMode(level->getLevelData()->getGameType() == GameType::Creative);
	_running = true;
	_mainWorld = level;
	_dimensionLevels.clear();
	_dimensionLevels[level->dimension ? level->dimension->id : 0] = level;
}

void Minecraft::setLevel(Level* level, const std::string& message /* ="" */, LocalPlayer* forceInsertPlayer /* = NULL */) {
	cameraTargetPlayer = NULL;
	LOGI("Seed is %ld\n", level->getSeed());

	// Block-id snapshot: load the logical->physical mapping this save was
	// written with so already-registered mod blocks can reuse those physical
	// ids (stable saves even across mod-combination changes).
#if defined(_WIN32) || defined(__ANDROID__)
	// 服务器同样要复用存档里记录的物理 id 映射，否则 mod 方块在服务器会错位。
	if (modEngine && level && level->getLevelData())
		modEngine->applyBlockSnapshot(level->getLevelData()->getBlockIdSnapshot());
#endif

	if (level != NULL) {
		level->raknetInstance = raknetInstance;
        gameMode->initLevel(level);

		if (!player && forceInsertPlayer)
		{
			player = forceInsertPlayer;
			player->resetPos(false);
			//level->addEntity(forceInsertPlayer);
		}
		else if (player != NULL) {
			player->resetPos(false);
			if (level != NULL) {
				// 只有玩家还不属于这个世界时才加入。维度切换时玩家已经由
				// Level::moveEntityTo() 放进目标世界，若这里再 add 一次，
				// EntityList 里就会存两份、每 tick 被 tick 两次。
				if (level->getEntity(player->entityId) != player)
					level->addEntity(player);
			}
		}
		this->level = level;
		_hasSignaledGeneratingLevelFinished = false;
#ifdef STANDALONE_SERVER
		const bool threadedLevelCreation = false;
#else
		const bool threadedLevelCreation = true;
#endif

		if (threadedLevelCreation) {
			// Threaded
			// "Lock"
			isGeneratingLevel = true;
			generateLevelThread = new CThread(Minecraft::prepareLevel_tspawn, this);
		} else {
			// Non-threaded
			generateLevel("Currently not used", level);
		}
    } else {
        player = NULL;
    }

    this->lastTickTime = 0;
	this->_running = true;
}

void Minecraft::leaveGame(bool renameLevel /*=false*/)
{
    if (isGeneratingLevel || !_hasSignaledGeneratingLevelFinished)
        return;
    
	isGeneratingLevel = false;
	// 维度切换的落点状态不能跨世界残留：否则下次进入任何世界，prepareLevel()
	// 会拿上一个维度的落点去做小范围预加载，出生点附近区块不生成 → 掉虚空。
	_hasPendingTravel = false;
	_dimAutoReturnDone = false;
#ifndef STANDALONE_SERVER
	// 离开这个世界就把聊天记录清掉：换个世界不该还显示上个世界的聊天。
	// （主世界 ↔ 维度之间的切换走的是 beginDimensionTravel，不经过这里，
	//   所以同一个存档里切维度时聊天记录会保留。）
	gui.clearMessages();
#endif
	bool saveLevel = level && (!level->isClientSide || renameLevel);

	// 用户定制：保存世界时先截一帧纯游戏画面，存到世界目录 screenshot.png（大厅左侧显示用）
#ifndef STANDALONE_SERVER
	if (saveLevel && gameRenderer && levelRenderer && level) {
		bool oldHideGui = options.hideGui;
		options.hideGui = true;
		gameRenderer->renderLevel(0);
		options.hideGui = oldHideGui;
		ExternalFileLevelStorage* st =
			dynamic_cast<ExternalFileLevelStorage*>(level->getLevelStorage());
		if (st) {
			std::string shotPath = st->getLevelPath() + "/screenshot.png";
			platform()->saveScreenshot(shotPath, width, height);
		}
	}
#endif

	raknetInstance->disconnect();
	if (saveLevel) {
		// If server or wanting to save level as client, save all unsaved chunks!
		level->getChunkSource()->saveAll(true);
	}

	LOGI("Clearing levels\n");

	cameraTargetPlayer = NULL;
#ifndef STANDALONE_SERVER
	levelRenderer->setLevel(NULL);
	particleEngine->setLevel(NULL);
	// Stop every sound source (incl. mod background music) so the track
	// doesn't keep playing on the main menu / in the next world.
	if (soundEngine) soundEngine->stopAllSounds();
#endif
	LOGI("Erasing callback\n");
	delete netCallback;
	netCallback = NULL;

	LOGI("Erasing dimension worlds\n");
	destroyDimensionWorlds();

	LOGI("Erasing level\n");
	if (level != NULL) {
		delete level->getLevelStorage();
		delete level;
		level = NULL;
	}
#if (defined(_WIN32) || defined(__ANDROID__)) && !defined(STANDALONE_SERVER)
	// Leave-world event for mods: the level is gone (no level.* access),
	// mods should only clean up their own state (e.g. hide a lingering
	// loading overlay / cancel a dimension pregen that would otherwise
	// keep flashing on the main menu).
	if (modEngine)
		modEngine->fireEvent("onLeaveWorld");
#endif
	//delete player;
	player = NULL;
	cameraTargetPlayer = NULL;

	_running = false;
#ifndef STANDALONE_SERVER
	if (renameLevel) {
		setScreen(new RenameMPLevelScreen(LevelStorageSource::TempLevelId, "world"));
	}
	else
		screenChooser.setScreen(SCREEN_STARTMENU);
#endif
}

void Minecraft::prepareLevel(const std::string& title) {
	LOGI("status: 1\n");
	progressStageStatusId = 1;

	Stopwatch A, B, C, D;
	A.start();

	Stopwatch L;

	// Dont update lights if we load the level (ok, actually just with leveldata version=1.+(?))
	if (!level->isNew())
		level->setUpdateLights(false);

	// 要预加载的区块网格（cx/cz 是区块坐标）：
	//  - 正常进入世界：spawn 区 16×16 区块（原版行为，cx/cz 0..15）。
	//  - 维度切换：只加载传送落点周围的 7×7。整片 16×16 对脚本维度要跑几万次
	//    JS，能让人干等几十秒；剩下的区块玩家走动时按需生成。
	//    （本函数跑在加载线程上，而脚本维度的地形就是 mod 的 JS 生成的；ModEngine
	//     的 JS 入口都加了互斥锁，与主线程的 onTick 串行化 —— 见 ModEngine::_jsMutex。）
	int cx0 = 0, cx1 = CHUNK_CACHE_WIDTH - 1;
	int cz0 = 0, cz1 = CHUNK_CACHE_WIDTH - 1;
	if (_hasPendingTravel && level->dimension &&
	    level->dimension->id == _pendingTravelDim) {
#ifndef STANDALONE_SERVER
		if (modEngine)
			modEngine->log("prepareLevel: dim-travel pregen 7x7 around "
			               + std::to_string((int)_pendingTravelX) + "," + std::to_string((int)_pendingTravelZ));
#endif
		int baseCx = Mth::floor(_pendingTravelX / (float)CHUNK_WIDTH);
		int baseCz = Mth::floor(_pendingTravelZ / (float)CHUNK_WIDTH);
		// 半径 8 → 17×17 区块，覆盖范围与原版"进入世界"预加载的 16×16 相当。
		// 早先只预热 7×7，结果是进度条走完之后视野里其余区块全靠主线程按需生成，
		// 而脚本维度每块都要跑 mod 的 JS —— 于是"进度条过了还是一直卡"。
		// 宁可让进度条多走一会儿，也别把地形生成摊到游戏里。
		cx0 = baseCx - 8; cx1 = baseCx + 8;
		cz0 = baseCz - 8; cz1 = baseCz + 8;
	}
	int Max = (cx1 - cx0 + 1) * (cz1 - cz0 + 1);
	int pp = 0;
	for (int cx = cx0; cx <= cx1; ++cx) {
        for (int cz = cz0; cz <= cz1; ++cz) {
		    progressStagePercentage = 100 * pp++ / Max;
            //printf("level generation progress %d\n", progressStagePercentage);
			B.start();
            level->getTile(cx * CHUNK_WIDTH + 8, 64, cz * CHUNK_WIDTH + 8);
			B.stop();
			L.start();
			if (level->isNew())
				while (level->updateLights())
					;
			L.stop();
        }
    }
	A.stop();
	level->setUpdateLights(true);

	// All chunks have been loaded from the region file. Release the in-memory
	// file cache that was populated by ExternalFileLevelStorage::load() on the
	// first chunk access, freeing the RAM now that sequential reads are done.
	level->getLevelStorage()->finishPreload();

	C.start();
	for (int cx = cx0; cx <= cx1; ++cx)
	{
		for (int cz = cz0; cz <= cz1; ++cz)
		{
			LevelChunk* chunk = level->getChunk(cx, cz);
			if (chunk && !chunk->createdFromSave)
			{
				chunk->unsaved = false;
				chunk->clearUpdateMap();
			}
		}
	}
	C.stop();

	LOGI("status: 3\n");
	progressStageStatusId = 3;
	if (level->isNew()) {
		level->setInitialSpawn(); // @note: should obviously be called from Level itself
		level->saveLevelData();
		level->getChunkSource()->saveAll(false);
		level->saveGame();
	} else {
		level->saveLevelData();
		level->loadEntities();
	}

	progressStagePercentage = -1;
	progressStageStatusId = 2;
	LOGI("status: 2\n");

	D.start();
	level->prepare();
	D.stop();

	A.print("Generate level: ");
	L.print(" - light: ");
	B.print(" - getTl: ");
	C.print(" - clear: ");
	D.print(" - prepr: ");
	progressStageStatusId = 0;
}

void Minecraft::update() {
	//LOGI("Enter Update\n");

	if (Options::debugGl)
		LOGI(">>>>>>>>>>\n");

	TIMER_PUSH("root");

	//if (level) {
	//	LOGI("numplayers: %d\n", level->players.size());
	//	for (int i = 0; i < level->players.size(); ++i) {
	//		Player* p = level->players[i];
	//		bool inEnt = std::find(level->entities.begin(), level->entities.end(), p) != level->entities.end();
	//		LOGI("  %p, %d, %d - in? %d\n", p, p->entityId, p->owner.ToUint32(p->owner), inEnt);
	//	}
	//}

	if (pause && level != NULL) {
		float lastA = timer.a;
		timer.advanceTime();
		timer.a = lastA;
	} else {
		timer.advanceTime();
	}

	if (raknetInstance) {
		raknetInstance->runEvents(netCallback);
		if (netCallback) {
			netCallback->tick();
		}
	}

	TIMER_PUSH("tick");
	double _tickProfStart = FrameProf::now();
	int toTick = timer.ticks;
	Stopwatch _tickBudget;
	_tickBudget.start();
	for (int i = 0; i < toTick; ++i, ++ticks) {
		tick(i, toTick-1);
		// Break out if ticks are taking too long — better to run the game
		// slightly slow than to freeze at 1fps in a feedback loop where
		// slow frames cause more ticks which cause slower frames.
		if (_tickBudget.stopContinue() > 0.05f) // 50ms budget for all ticks
			break;
	}
	FrameProf::add(FrameProf::SEC_TICK, _tickProfStart, FrameProf::now());

	TIMER_POP_PUSH("updatelights");
	double _ulProfStart = FrameProf::now();
	if (level && !isGeneratingLevel) {
		level->updateLights();
	}
	FrameProf::add(FrameProf::SEC_UPDATELIGHTS, _ulProfStart, FrameProf::now());
	TIMER_POP();

	#ifndef STANDALONE_SERVER
		if (gameMode != NULL) gameMode->render(timer.a);
		TIMER_PUSH("sound");
		soundEngine->update(player, timer.a);
		TIMER_POP_PUSH("render");
		gameRenderer->render(timer.a);
		TIMER_POP();
	#else
	CThread::sleep(1);
	#endif
#ifndef STANDALONE_SERVER
	Multitouch::resetThisUpdate();
#endif
	TIMER_POP();
#ifndef STANDALONE_SERVER
	checkGlError("Update finished");

	// Runtime mod logging is off unless the F3 debug overlay is open (see
	// ModEngine::log). Cheap: one bool copy per frame.
	g_modLogF3Debug = options.renderDebug;

	if (options.renderDebug) {
		if (!PerfTimer::enabled) {
			PerfTimer::reset();
			PerfTimer::enabled = true;
		}

		//TIMER_PUSH("debugfps");
		_perfRenderer->renderFpsMeter(1);
		checkGlError("render debug");
		//TIMER_POP();
	} else {
		PerfTimer::enabled = false;
	}
#endif
	//LOGI("Exit Update\n");
}

void Minecraft::tick(int nTick, int maxTick) {
#if defined(_WIN32) || defined(__ANDROID__)
	// 服务器同样要跑 mod 的 onTick 与 setTimeout/setInterval —— 服务器 mod 的定时
	// 逻辑（刷怪、经济、任务判定）都挂在这里。
	//
	// 但**世界加载期间必须跳过**：加载线程正在生成地形，而脚本维度的地形要调
	// mod 的 JS —— 两边都只能串行进 Duktape（ModEngine 的互斥锁）。主线程若还在
	// 每 tick 跑 mod 事件，就得排队等加载线程，而生成一块地形要几十毫秒到几秒，
	// 表现就是整个游戏一直是"未响应"。（原版进入世界用的是 C++ 生成器、不碰
	// JS，所以这个顺序问题从来没暴露过。）
	if (modEngine && !isGeneratingLevel) {
		modEngine->fireEvent("onTick");
		modEngine->tickBiomeTracking();   // 跨群系 → onBiomeEnter / onBiomeLeave
		modEngine->tick();  // setTimeout/setInterval
	}
#endif
	if (missTime > 0) missTime--;
#ifndef STANDALONE_SERVER
	if (!screen && player) {
		if (player->health <= 0) {
			// 世界规则「死亡立刻重生」：跳过死亡界面，血量回满后当场复活。
			if (level && level->getLevelData() && level->getLevelData()->getImmediateRespawn()) {
				player->health = Player::MAX_HEALTH;
				player->lastHealth = Player::MAX_HEALTH;
				respawnPlayer();
			} else {
				setScreen(new DeathScreen());
			}
		}
	}
#endif
	TIMER_PUSH("gameMode");
	if (level && !pause) {
		gameMode->tick();
	}

	TIMER_POP_PUSH("commandServer");
	if (level && _commandServer) {
		_commandServer->tick();
	}

	TIMER_POP_PUSH("input");
	tickInput();
#ifndef STANDALONE_SERVER
	TIMER_POP_PUSH("gui");
	gui.tick();
#endif
	//
	// Ongoing level generation in a (perhaps) different thread. When it's
	// ready, _levelGenerated() is called once and any threads are deleted.
	//
	if (isGeneratingLevel) {
		return;
	} else if (!_hasSignaledGeneratingLevelFinished) {
		if (generateLevelThread) {
			delete generateLevelThread;
			generateLevelThread = NULL;
		}
		_levelGenerated();
		// Skip the rest of this tick — let the next frame start fresh
		// so we don't process a backlog of ticks from generation time.
		return;
	}

	//
	// Normal game loop, run before or efter level generation
	//
	if (level != NULL)
	{
		if (!pause) {
#ifndef STANDALONE_SERVER
			TIMER_POP_PUSH("gameRenderer");
			gameRenderer->tick(nTick, maxTick);

			TIMER_POP_PUSH("levelRenderer");
			levelRenderer->tick();
#endif
#ifndef STANDALONE_SERVER
			// 难度按世界保存（创建世界 / 世界设置里选的）。旧世界存档没有难度
			// 字段 -> 退回玩家全局选项。联机客户端用简单难度。
			if (level->isClientSide) {
				level->difficulty = Difficulty::EASY;
			} else {
				LevelData* ld = level->getLevelData();
				int d = ld ? ld->getDifficulty() : -1;
				if (d < 0) d = options.difficulty;
				level->difficulty = d;
			}
#else
			// 独立服务器：难度由 server.properties 的 difficulty 决定，已经在
			// ServerApp::applyWorldRules() 里设好。这里**绝不能**再每 tick 从
			// 客户端选项（options.difficulty）覆盖 —— 客户端的设置不该影响服务器，
			// 而且那样一来难度会随最后一个连进来的客户端变。
#endif

			TIMER_POP_PUSH("level");
			level->tickEntities();
			level->tick();
#ifndef STANDALONE_SERVER
			TIMER_POP_PUSH("animateTick");
			if (player) {
				level->animateTick(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));
			}
#endif
		}
	}
#ifndef STANDALONE_SERVER
	textures->loadAndBindTexture("terrain.png");
	if (!pause && !(screen && !screen->renderGameBehind())) {
		#if !defined(RPI)
			#ifdef __APPLE__
			if (isSuperFast())
			#endif
			{
			if (nTick == maxTick) {
				TIMER_POP_PUSH("textures");
				textures->tick(true);
			}
			}
		#endif
	}
	TIMER_POP_PUSH("particles");
	particleEngine->tick();
	if (screen) {
		screenMutex = true;
		screen->tick();
		screenMutex = false;
	}

	// @note: fix to keep "isPressed" and "isReleased" as long as necessary.
	//      Most likely keyboard and mouse could/should be reset here as well.
	Multitouch::reset();
#endif
	TIMER_POP();
}

class InputRAII {
public:
	~InputRAII() {
#ifndef STANDALONE_SERVER
		Mouse::reset();
		Keyboard::reset();
#endif
	}
};

void Minecraft::tickInput() {
#ifndef STANDALONE_SERVER
	InputRAII raiiInput;

	if (screen && !screen->passEvents) {
		screenMutex = true;
		screen->updateEvents();
		//screen->updateSetScreen();
		screenMutex = false;		if (hasScheduledScreen) {
			setScreen(scheduledScreen);
			scheduledScreen = NULL;
			hasScheduledScreen = false;
		}
		return;
	}

	if (!player) {
		return;
	}

	// 鼠标左右键的“按下/抬起”报给模组的 onMouse(button, down, heldMs)。
	// 用【轮询键状态】做边沿检测，不走 Mouse::next() 那个事件队列 —— 游戏内
	// 那条队列收不到点击（原版自己那两行挖/放逻辑就是因此被注释掉的），而
	// 下面挖方块用的就是 isButtonDown，跟它同源最可靠。
	// 只在状态【变化】时报一次，所以不会每 tick 进 JS。
	if (modEngine) {
		static bool sPrevMouseLeft = false, sPrevMouseRight = false;
		static int  sMouseDownAt[2] = { 0, 0 };   // 按下时刻，用来算 heldMs
	// 模组刚注册/改了 HUD 按钮时要重建一次触摸布局：这些矩形得进
	// UnifiedTurnBuild 的「不算转向区」列表，否则按着按钮会被当成按住转向区
	//（转视角，按住 400ms 还会开始挖方块）。
	if (modEngine) {
		static int sHudLayoutVersion = -1;
		const int hudVersion = modEngine->hudLayoutVersion();
		if (hudVersion != sHudLayoutVersion) {
			sHudLayoutVersion = hudVersion;
			if (inputHolder)
				inputHolder->onConfigChanged(createConfig(this));
		}
	}

		const int nowMs = getTimeMs();
		bool curMouse[2] = {
			Mouse::isButtonDown(MouseAction::ACTION_LEFT) != 0,
			Mouse::isButtonDown(MouseAction::ACTION_RIGHT) != 0
		};
		// 触摸屏：报给模组的鼠标键只来自 HUD 上的「按键映射」按钮。手指本身在
		// Multitouch 里会被镜像成左键，若不覆盖，点屏幕任何地方都等于“按住左键”
		//（模组就一直收到 onMouse(0,down)：按钮高亮乱跳、一碰屏幕就开火）。
		// 挖掘/放置不受影响 —— 那两处直接读 Mouse::isButtonDown（真实触摸）。
		if (useTouchscreen() && modEngine) {
			curMouse[0] = modEngine->hudMouseKeyHeld(0);
			curMouse[1] = modEngine->hudMouseKeyHeld(1);
		}
		bool* prevMouse[2] = { &sPrevMouseLeft, &sPrevMouseRight };
		for (int b = 0; b < 2; ++b) {
			if (curMouse[b] == *prevMouse[b])
				continue;
			int heldMs = 0;
			if (curMouse[b])
				sMouseDownAt[b] = nowMs;          // 按下：heldMs = 0
			else
				heldMs = nowMs - sMouseDownAt[b];  // 抬起：给出总共按了多久
			*prevMouse[b] = curMouse[b];
			// 被 HUD 按钮占用的那根手指不算「鼠标按下」：否则按【换弹】也会被模组
			// 当成按住左键 → 顺手开一枪。抬起时照常报 down=0，模组状态不会卡住。
			const bool reportDown = curMouse[b] && !(b == 0 && modEngine->hudFirstPointer() >= 0);
			modEngine->fireMouseEvent(b, reportDown, heldMs);
		}
	}

#ifdef RPI
	bool mouseDiggable = true;
	bool allowGuiClicks = !mouseGrabbed;
#else
	bool mouseDiggable = !gui.isInside(Mouse::getX(), Mouse::getY());
	bool allowGuiClicks = true;
#endif

#ifdef ANDROID
	// 世界内的 HUD 点击（右上角聊天键、物品栏格子…）。
	//
	// 为什么不用 Mouse 事件队列：Android 上 Mouse 事件队列收不到触摸点击
	// （见上面挖/放那段注释），而 gui.handleClick 原先只挂在那个队列上，
	// 于是世界里的 HUD 全点不动。这里改从 Multitouch 直接取「刚按下」的
	// 指针（触摸 UI 的挖方块/转向用的就是它，来源最可靠），补一次 handleClick。
	{
		static bool sPrevDown[Multitouch::MAX_POINTERS] = { false };
		for (int p = 0; p < Multitouch::MAX_POINTERS; ++p) {
			const bool down = Multitouch::isPointerDown(p);
			if (down && !sPrevDown[p]) {
				const short px = Multitouch::getX(p);
				const short py = Multitouch::getY(p);
				// 模组注册的 HUD 按钮优先：命中就派发给模组，并把这根手指「归模组」
				// （mouseTaken(0) → true 不会挖方块；capturedPointer() 会返回它 →
				// 不会被当成转向/挖掘），不再走原版的 HUD 点击。
				const float gx = (float)px * Gui::InvGuiScale;
				const float gy = (float)py * Gui::InvGuiScale;
				std::string hudKey;
				// 模组的 HUD 按钮/按键映射只在世界里生效：菜单、设置页上看不到这些
				// 按钮，不拦住就会误命中（屏幕上看不见，按下去却注入了鼠标键/键盘键，
				// 例如 R 映射在设置页里被当成按键，把页面顶走）。
				if (!screen && modEngine && modEngine->hudKeyAt(gx, gy, &hudKey)) {
					// 按键映射按钮：注入鼠标键 / 键盘键（模组收到的还是 onMouse / onKey）
					modEngine->hudPress(p, hudKey);
					LOGI("[hudclick] mod keymap '%s' DOWN (pid=%d at %d,%d)\n",
					     hudKey.c_str(), p, (int)px, (int)py);
				} else if (!screen && modEngine && modEngine->hudButtonAt(gx, gy, &hudKey)) {
					modEngine->hudPress(p, hudKey);
					LOGI("[hudclick] mod button '%s' DOWN (pid=%d at %d,%d)\n",
					     hudKey.c_str(), p, (int)px, (int)py);
				} else {
					LOGI("[hudclick] MT down pid=%d at %d,%d (inv=%.4f w=%d)\n",
					     p, (int)px, (int)py, (double)Gui::InvGuiScale, (int)width);
					gui.handleClick(MouseAction::ACTION_LEFT, px, py);
				}
			}
			else if (!down && sPrevDown[p] && modEngine && modEngine->hudPointerActive(p)) {
				// 手指抬起：告诉模组松开（射击停火 / 退出瞄准），再释放该指针
				modEngine->hudRelease(p);   // 按指针记账：回收这个指针注入的键 + 派发 onUp
				LOGI("[hudclick] mod keymap/button UP (pid=%d)\n", p);
			}
			sPrevDown[p] = down;
		}
	}
#endif

	TIMER_PUSH("mouse");
	while (Mouse::next()) {
		//if (Mouse::getButtonState(MouseAction::ACTION_LEFT))
		//	LOGI("mouse-down-at: %d, %d\n", Mouse::getX(), Mouse::getY());
        	int passedTime = getTimeMs() - lastTickTime;
        	if (passedTime > 200) continue; // @note: As long Mouse::clear CLEARS the whole buffer, it's safe to break here
		// But since it might be rewritten anyway (and hopefully there aren't a lot of messages, we just continue.

		const MouseAction& e = Mouse::getEvent();

#ifdef RPI // If clicked when not having focus, get focus @keyboard
		if (!mouseGrabbed) {
			if (!screen && e.data == MouseAction::DATA_DOWN) {
				grabMouse();
			}
		}
#endif

#ifndef ANDROID
		if (allowGuiClicks && e.action == MouseAction::ACTION_LEFT && e.data == MouseAction::DATA_DOWN) {
			gui.handleClick(MouseAction::ACTION_LEFT, e.x, e.y);
		}
#endif

		if (e.action == MouseAction::ACTION_WHEEL) {
			// A screen that scrolls (e.g. the mod manager) gets the wheel;
			// otherwise keep the vanilla hotbar-switch behaviour.
			if (screen) {
				screen->onMouseWheel(e.dy);
			} else {
				Inventory* v = player->inventory;
				int numSlots = gui.getNumSlots() - 1;
				int slot = (v->selected - e.dy + numSlots) % numSlots;
				v->selectSlot(slot);
			}
		}
		/*
		if (mouseDiggable && options.useMouseForDigging) {
			if (Mouse::getEventButton() == MouseAction::ACTION_LEFT && Mouse::getEventButtonState()) {
				handleMouseClick(MouseAction::ACTION_LEFT);
				lastClickTick = ticks;
			}
			if (Mouse::getEventButton() == MouseAction::ACTION_RIGHT && Mouse::getEventButtonState()) {
				handleMouseClick(MouseAction::ACTION_RIGHT);
				lastClickTick = ticks;
			}
		}
		*/
	}

	TIMER_POP_PUSH("keyboard");
	while (Keyboard::next()) {
		int key = Keyboard::getEventKey();
		bool isPressed = (Keyboard::getEventKeyState() == KeyboardAction::KEYDOWN);
		player->setKey(key, isPressed);

		// JS onKey(keyCode, down): 模组可监听按键编排行为(如"坐着按空格起身"、
		// "按 E 坐下")。按下与抬起都发, 键码为 win32 虚拟键/字符码(见 Keyboard.h)。
		if (modEngine)
			modEngine->fireEvent("onKey", key, isPressed ? 1 : 0);

		if (isPressed) {
			gui.handleKeyPressed(key);

			#if defined(WIN32) || defined(RPI) || defined(MACOS) || defined(LINUX)
				if (key >= '0' && key <= '9') {
					int digit = key - '0';
					int slot = digit - 1;

					if (slot >= 0 && slot < gui.getNumSlots()-1)
						player->inventory->selectSlot(slot);

					#if defined(WIN32)
						if (digit >= 1 && GetAsyncKeyState(VK_CONTROL) < 0) {
							// Set adventure settings here!
							AdventureSettingsPacket p(level->adventureSettings);
							p.toggle((AdventureSettingsPacket::Flags)(1 << slot));
							p.fillIn(level->adventureSettings);
							raknetInstance->send(p);
						}
						if (digit == 0) {
							Pos pos((int)player->x, (int)player->y-1, (int)player->z);
							SetSpawnPositionPacket p(pos);
							raknetInstance->send(p);
						}
					#endif
				}
			#endif
			#if defined(RPI) || defined(MACOS) || defined(LINUX)
				if (key == Keyboard::KEY_E) {
					screenChooser.setScreen(SCREEN_BLOCKSELECTION);
				}
				if (!screen && key == Keyboard::KEY_O || key == 250) {
					releaseMouse();
				}
			#endif
			#if defined(MACOS) || defined(LINUX)
				if (key == Keyboard::KEY_T) {
					options.thirdPersonView = !options.thirdPersonView;
				}
				if (key == Keyboard::KEY_F3) {
					options.renderDebug = !options.renderDebug;
				}
			#endif
			#if defined(WIN32)
				if (key == Keyboard::KEY_T) {
					options.thirdPersonView = !options.thirdPersonView;
					/*
					ImprovedNoise noise;
					for (int i = 0; i < 16; ++i)
						printf("%d\t%f\n", i, noise.grad2(i, 3, 8));
					*/
				}
				if (key == Keyboard::KEY_RETURN) {
					if (!screen)
						screenChooser.setScreen(SCREEN_CHAT);
				}

				if (key == Keyboard::KEY_O) {
					useAmbientOcclusion = !useAmbientOcclusion;
					options.ambientOcclusion = useAmbientOcclusion;
					levelRenderer->allChanged();
				}

				if (key == Keyboard::KEY_L)
					options.viewDistance = (options.viewDistance + 1) % 4;

				if (key == Keyboard::KEY_U) {
					onGraphicsReset();
					player->heal(100);
				}

				if (key == Keyboard::KEY_B || key == 108) // Toggle the game mode
					setIsCreativeMode(!isCreativeMode());

				if (key == Keyboard::KEY_P) // Step forward in time
					level->setTime( level->getTime() + 1000);

				if (key == Keyboard::KEY_G) {
					setScreen(new ArmorScreen());
					/*
					std::vector<AABB>& boxs = level->getCubes(NULL, AABB(128.1f, 73, 128.1f, 128.9f, 74.9f, 128.9f));
					LOGI("boxes: %d\n", (int)boxs.size());
					*/
				}

				if (key == Keyboard::KEY_Y) {
					textures->reloadAll();
					player->hurtTo(2);
				}

				if (key == Keyboard::KEY_C /*|| key == 4*/) {
					player->inventory->clearInventoryWithDefault();
					// @todo: Add saving here for benchmarking
				}
				if (key == Keyboard::KEY_H) {
					setScreen( new PrerenderTilesScreen() );
				}

				if (key == Keyboard::KEY_O) {
					for (int i = Inventory::MAX_SELECTION_SIZE; i < player->inventory->getContainerSize(); ++i)
						if (player->inventory->getItem(i))
							player->inventory->dropSlot(i, false);
				}
				if (key == Keyboard::KEY_F3) {
					options.renderDebug = !options.renderDebug;
				}
				if (key == Keyboard::KEY_M) {
					options.difficulty = (options.difficulty == Difficulty::PEACEFUL)?
						Difficulty::NORMAL : Difficulty::PEACEFUL;
					// 难度按世界保存：顺手写回当前世界，退出后仍然生效
					if (level && level->getLevelData())
						level->getLevelData()->setDifficulty(options.difficulty);
					//setIsCreativeMode( !isCreativeMode() );
				}

				if (options.renderDebug) {
					if (key >= '0' && key <= '9') {
						_perfRenderer->debugFpsMeterKeyPress(key - '0');
					}
				}
				if (key == Keyboard::KEY_E) {
					// 0.8.1 背包移植：E 打开背包（创造=分页创造背包，生存=物品栏选择屏即 0.8.1 生存背包，
					// 盔甲页从背包内的 Armor 按钮进入，而非直接打开盔甲页）
					if (isCreativeMode()) {
						screenChooser.setScreen(SCREEN_CREATIVE_INVENTORY);
					} else {
						screenChooser.setScreen(SCREEN_BLOCKSELECTION);
					}
				}
				if (key == 250) {  // Tab (mapped in transformKey_win32)
					releaseMouse();
				}
			#endif

			#if defined(MACOS) || defined(RPI) || defined(LINUX) || defined(WIN32)
				if (key == Keyboard::KEY_ESCAPE)
					pauseGame(false);
			#else
				if (key == 82)
					pauseGame(false);
			#endif

			#ifndef OPENGL_ES
				if (key == Keyboard::KEY_P) {
					static bool isWireFrame = false;
					isWireFrame = !isWireFrame;
					glPolygonMode(GL_FRONT, isWireFrame? GL_LINE : GL_FILL);
					//glPolygonMode(GL_BACK, isWireFrame? GL_LINE : GL_FILL);
				}
			#endif
		}
		#ifdef WIN32
			if (key == Keyboard::KEY_M) {
				for (int i = 0; i < 5 * SharedConstants::TicksPerSecond; ++i)
					level->tick();
			}
		#endif


		//if (!isPressed) LOGI("Key released: %d\n", key);

		if (!options.useMouseForDigging) {
			int passedTime = getTimeMs() - lastTickTime;
			if (passedTime > 200) continue;

			// Destroy and attack is on same button
			if (key == options.keyDestroy.key && isPressed) {
				BuildActionIntention bai(BuildActionIntention::BAI_REMOVE | BuildActionIntention::BAI_ATTACK);
				handleBuildAction(&bai);
			}
			else // Build and use/interact is on same button
			if (key == options.keyUse.key && isPressed) {
				BuildActionIntention bai(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
				handleBuildAction(&bai);
			}
		}
	}

	TIMER_POP_PUSH("tickbuild");
	BuildActionIntention bai;
	// @note: This might be a problem. This method is polling based here, but
	//        event based when using mouse (or keys..), and in java. Not quite
	//        sure just yet what way to go.
	// 模组接管了某个鼠标键时（看模组的 onMouse 返回值，ModEngine::mouseTaken），
	// 原版就别挖方块、别放方块/用物品 —— 否则拿枪开枪会顺手把方块挖了。
	// 没人接管时（空手、拿工具、吃东西）两个都是 false，行为与改动前完全一致。
	const bool modTookLeft  = modEngine && modEngine->mouseTaken(0);
	const bool modTookRight = modEngine && modEngine->mouseTaken(1);

	bool buildHandled = !modTookRight && inputHolder->getBuildInput()->tickBuild(player, &bai);
	if (buildHandled) {
		if (!bai.isRemoveContinue())
			handleBuildAction(&bai);
	}

	// 触摸界面里不能拿「鼠标左键」当挖掘依据：Multitouch::feed 会把 1 号手指
	// 镜像成鼠标左键按下（见 Multitouch.h 的 ANDROID 分支），于是「手指按在屏幕上」
	// 就等于「一直按住左键」。再加上 Options 里触摸平台也把 useMouseForDigging
	// 设成 true（Options.cpp），按方向键/跳跃键/升降键移动时就会走 PC 那套
	// continueDestroyBlock，把 hitResult（十字准星模式下就是屏幕中心准星）瞄准的
	// 方块一路挖掉。触摸界面的挖掘交回 UnifiedTurnBuild 的「按住不动」判定
	// （即下面的 bai.isRemove()），与 0.8.x 原版触摸行为一致。
	// 桌面（Win/Linux/Mac）/RPI 的 useTouchscreen() 为 false，行为不变。
	const bool useMouseDigging = options.useMouseForDigging && !useTouchscreen();
	bool isTryingToDestroyBlock = !modTookLeft && (useMouseDigging
			?	(Mouse::isButtonDown(MouseAction::ACTION_LEFT) && mouseDiggable)
			:	Keyboard::isKeyDown(options.keyDestroy.key))
		||	(buildHandled && bai.isRemove());

	TIMER_POP_PUSH("handlemouse");
#ifdef RPI
	handleMouseDown(MouseAction::ACTION_LEFT, isTryingToDestroyBlock);
	handleMouseClick(buildHandled && bai.isInteract()
		|| options.useMouseForDigging && Mouse::isButtonDown(MouseAction::ACTION_RIGHT));
#else
	// NOTE: pass interact state into the `down` flag too. handleMouseDown's
	// isUsingItem branch uses `down` to decide whether the player is still
	// holding the use button (eating / drawing a bow): right-click
	// place/interact keeps it true, so the use isn't cancelled every tick.
	// The destructive branch below additionally requires the LEFT button to
	// actually be down, so right-click never feeds digging progress (the old
	// "ten right-clicks mine a block" bug stays fixed).
	handleMouseDown(MouseAction::ACTION_LEFT, isTryingToDestroyBlock || (buildHandled && bai.isInteract()));
#endif

	lastTickTime = getTimeMs();

	// we have (hopefully) handled the keyboard & mouse queue and it
	// can now be emptied. If wanted, the reset could be changed to:
	// index -= numRead; // then this code doesn't have to be placed here
	// + it prepares for tick not handling all or any events.
	// update: RAII'ing instead, see above
	//Keyboard::reset();
	//Mouse::reset();

	TIMER_POP();
#endif
}

void Minecraft::handleMouseDown(int button, bool down) {
#ifndef STANDALONE_SERVER
#ifndef RPI
	if(player->isUsingItem()) {
		if(!down && !Keyboard::isKeyDown(options.keyUse.key)) {
			gameMode->releaseUsingItem(player);
		}
		return;
	}
#endif
	if(player->isSleeping()) {
		return;
	}
	// 左键被模组接管时（拿枪开枪）：既不挖方块、也不做那套向下挥手的动画，
	// 顺便把已经在跑的挖掘进度停掉（否则松手前会一直留着裂纹/进度）。
	if (button == MouseAction::ACTION_LEFT && modEngine && modEngine->mouseTaken(0)) {
		gameMode->stopDestroyBlock();
		return;
	}
    if (button == MouseAction::ACTION_LEFT && missTime > 0) return;
	// Destructive branch: the `down` flag may be true from right-click
	// interact (holding use to eat/draw a bow), so additionally require the
	// LEFT button to really be down before feeding digging progress.
	bool leftActuallyDown = Mouse::isButtonDown(MouseAction::ACTION_LEFT) != 0;
	if (down && leftActuallyDown && hitResult.type == TILE && button == MouseAction::ACTION_LEFT && !hitResult.indirectHit) {
        int x = hitResult.x;
        int y = hitResult.y;
        int z = hitResult.z;
        gameMode->continueDestroyBlock(x, y, z, hitResult.f);
        particleEngine->crack(x, y, z, hitResult.f);
		player->swing();
    } else {
        gameMode->stopDestroyBlock();
    }
#endif
}

void Minecraft::handleMouseClick(int button) {
//	BuildActionIntention bai(
//		(button == MouseAction::ACTION_LEFT)?
//			BuildActionIntention::BAI_REMOVE
//		:	BuildActionIntention::BAI_BUILD);
//
//	handleBuildAction(&bai);
}

void Minecraft::handleBuildAction(BuildActionIntention* action) {
#ifndef STANDALONE_SERVER
	// 左键被模组接管时（例如拿枪开枪），原版这一整套“左键动作”都别做：
	// 不挖方块、不近战攻击，自然也就没有那一套向下挥手（挖方块）的动画。
	// 没人接管时 modLeftTaken 恒为 false，行为与改动前一致。
	const bool modLeftTaken = modEngine && modEngine->mouseTaken(0);
	if (modLeftTaken && (action->isRemove() || action->isAttack()))
		return;
	if (action->isRemove()) {
		if (missTime > 0) return;
		player->swing();
	}
	if(player->isUsingItem())
		return;
    bool mayUse = true;

	if (!hitResult.isHit()) {
		if (action->isRemove() && !gameMode->isCreativeType()) {
			missTime = 10;
		}
    } else if (hitResult.type == ENTITY) {
        if (action->isAttack()) {
			player->swing();
			//LOGI("attacking!\n");
			InteractPacket packet(InteractPacket::Attack, player->entityId, hitResult.entity->entityId);
			raknetInstance->send(packet);
            gameMode->attack(player, hitResult.entity);
		} else if (action->isInteract()) {
			if (hitResult.entity->interactPreventDefault())
				mayUse = false;
			//LOGI("interacting!\n");
			InteractPacket packet(InteractPacket::Interact, player->entityId, hitResult.entity->entityId);
			raknetInstance->send(packet);
            gameMode->interact(player, hitResult.entity);
        }
    } else if (hitResult.type == TILE) {
		int x = hitResult.x;
        int y = hitResult.y;
        int z = hitResult.z;
        int face = hitResult.f;

		int oldTileId = level->getTile(x, y, z);
        Tile* oldTile = Tile::tiles[oldTileId];

		//bool tryDestroyBlock = false;

		if (action->isRemove()) {
			if (!oldTile)
				return;

			// 记下精确命中点：破坏时模组要用它判断点在方块内的哪一处
			// （微方块：一格里有好几个小方块，挖哪个看命中点）。
			gameMode->hitPointX = (float)hitResult.pos.x;
			gameMode->hitPointY = (float)hitResult.pos.y;
			gameMode->hitPointZ = (float)hitResult.pos.z;

			//LOGI("tile: %s - %d, %d, %d. b: %f - %f\n", oldTile->getDescriptionId().c_str(), x, y, z, oldTile->getBrightness(level, x, y, z), oldTile->getBrightness(level, x, y+1, z));
            level->extinguishFire(x, y, z, hitResult.f);
			gameMode->startDestroyBlock(x, y, z, hitResult.f);
        }
		else {
			ItemInstance* item = player->inventory->getSelected();
 			if (gameMode->useItemOn(player, level, item, x, y, z, face, hitResult.pos)) {
			    mayUse = false;
			    player->swing();
			#ifdef RPI
				} else if (item && item->id == ((Item*)Item::sword_iron)->id) {
					player->swing();
			#endif
			}
			if (item && item->count <= 0) {
				player->inventory->clearSlot(player->inventory->selected);
			}
			//} else if (item && item->count != oldCount) {
			//	  gameRenderer->itemInHandRenderer->itemPlaced();
			//}
		}
	}
	if (mayUse && action->isInteract()) {
		ItemInstance* item = player->inventory->getSelected();
		if (item && !player->isUsingItem()) {
			if (gameMode->useItem(player, level, item)) {
				gameRenderer->itemInHandRenderer->itemUsed();
			}
			if (item && item->count <= 0) {
				player->inventory->clearSlot(player->inventory->selected);
			}
		}
	}
#endif
}

bool Minecraft::isOnlineClient()
{
    return (level != NULL && level->isClientSide);
}

bool Minecraft::isOnline()
{
	return netCallback != NULL;
}

void Minecraft::pauseGame(bool isBackPaused) {
#ifndef STANDALONE_SERVER
	if (screen != NULL) return;
	screenChooser.setScreen(isBackPaused? SCREEN_PAUSEPREV : SCREEN_PAUSE);
#endif
}
void Minecraft::gameLostFocus() {
#ifndef STANDALONE_SERVER
	if(screen != NULL) {
		screen->lostFocus();
	}
#endif
}


void Minecraft::setScreen( Screen* screen )
{
#ifndef	STANDALONE_SERVER
	Mouse::reset();
	Multitouch::reset();
	Multitouch::resetThisUpdate();
	// Flush pending keyboard input so events from the old screen (and the
	// key that triggered the screen switch, e.g. Enter) are consumed exactly
	// once. Using rewind() here was a bug: it reset _index to -1, so the
	// updateEvents loop `while (Keyboard::next())` re-read the very same
	// KEY_RETURN event on its next iteration, calling submit() again ->
	// infinite loop / freeze (chat "death" on every message send).
	Keyboard::reset();

	if (screenMutex) {
		hasScheduledScreen = true;
		scheduledScreen = screen;
		return;
	}

	if (screen != NULL && screen->isErrorScreen())
		return;
	if (screen == NULL && level == NULL)
		screen = screenChooser.createScreen(SCREEN_STARTMENU);

	if (this->screen != NULL) {
		this->screen->removed();
		delete this->screen;
	}

	this->screen = screen;
	// Let the platform know whether this screen wants text input. The Win32
	// frontend enables the IME only then — so a CJK input method never
	// swallows WASD/inventory keys while playing (only while typing).
#if defined(_WIN32) || defined(__ANDROID__)
	Keyboard::setTextInputActive(screen != NULL && screen->hasTextInput());
	// Win32: physically attach/detach the IME context — with a chat/server
	// box open the IME can compose Chinese; without one the context is NULL
	// so letter keys (WASD etc.) always reach the game as plain keys even if
	// the OS input method is in Chinese mode.
#if defined(_WIN32)
	extern HWND g_win32Hwnd;
	extern void win32SetImeEnabled(HWND hwnd, bool enable);
	if (g_win32Hwnd)
		win32SetImeEnabled(g_win32Hwnd, screen != NULL && screen->hasTextInput());
#endif
#endif
	if (screen != NULL) {
		releaseMouse();
		//ScreenSizeCalculator ssc = new ScreenSizeCalculator(options, width, height);
		int screenWidth = (int)(width * Gui::InvGuiScale); //ssc.getWidth();
		int screenHeight = (int)(height * Gui::InvGuiScale); //ssc.getHeight();
		screen->init(this, screenWidth, screenHeight);

		if (screen->isInGameScreen() && level) {
			level->saveLevelData();
            level->saveGame();
        }

		//noRender = false;
	} else {
		grabMouse();
	}
#endif
}

void Minecraft::grabMouse()
{
#ifndef STANDALONE_SERVER
	if (mouseGrabbed) return;
	mouseGrabbed = true;
	mouseHandler.grab();
	//setScreen(NULL);
#endif
}

void Minecraft::releaseMouse()
{
#ifndef STANDALONE_SERVER
	if (!mouseGrabbed) {
		return;
	}
	if (player) {
		player->releaseAllKeys();
	}
	mouseGrabbed = false;
	mouseHandler.release();
#endif
}

bool Minecraft::useTouchscreen() {
#ifdef RPI
	return false;
#endif
	return options.useTouchScreen || !_supportsNonTouchscreen;
}
bool Minecraft::supportNonTouchScreen() {
	return _supportsNonTouchscreen;
}
void Minecraft::init()
{
	options.minecraft = this;
	_supportsNonTouchscreen = !platform()->supportsTouchscreen();
	options.initDefaultValues();
#ifndef STANDALONE_SERVER
	checkGlError("Init enter");

	LOGI("IS TOUCHSCREEN? %d\n", options.useTouchScreen);

	textures = new Textures(&options, platform());
	textures->addDynamicTexture(new WaterTexture());
	textures->addDynamicTexture(new WaterSideTexture());
	gui.texturesLoaded(textures);

	levelRenderer = new LevelRenderer(this);
	gameRenderer = new GameRenderer(this);
	particleEngine = new ParticleEngine(level, textures);

	// Platform specific initialization here
	font = new Font(&options, "font/default8.png", textures);

	// Load the language chosen in options (fallback en_US is loaded earlier
	// in NinecraftApp::init so strings are available before this point).
	I18n::loadLanguage(platform(), options.language);

	_perfRenderer = new PerfRenderer(this, font);

	checkGlError("Init complete");
#endif

#if defined(_WIN32) || defined(__ANDROID__)
	// JS mod 引擎（Duktape）。客户端与独立服务器都创建：同一份 mod zip 双端加载，
	// 服务器跑权威逻辑（方块/事件/命令/数值），客户端跑表现（贴图/UI/光影/音效）。
	// mod 启用状态来自 mods/modlist.json。
	modEngine = new ModEngine(this);
	if (modEngine->init()) {
		modEngine->loadEnabledMods();
	} else {
		modEngine->log("ModEngine init failed");
	}
#ifndef STANDALONE_SERVER
	// Player skin: 重开游戏后自动恢复上次选择的皮肤(主菜单纸娃娃 + 游戏内玩家)。
	// 纯客户端表现，服务器不需要。
	if (modEngine) {
		if (modEngine->hasSavedSkin())
			modEngine->applySavedPlayerSkin();
		else
			modEngine->applyDefaultSkinTo64();  // 无皮肤: 原版 char 也按 64x64 归一
	}
#endif
#endif

	user = new User("TestUser", "");
	setIsCreativeMode(false); // false means it's Survival Mode

#ifdef ANDROID
	if (!externalStoragePath.empty()) {
		options.setSettingsPath(externalStoragePath + "/games/com.mojang/options.txt");
	}
#elif defined(__APPLE__) && !defined(MACOS)
	if (!externalStoragePath.empty()) {
		options.setSettingsPath(externalStoragePath + "/games/com.mojang/options.txt");
	}
#elif defined(MACOS) || defined(LINUX)
	if (!externalStoragePath.empty()) {
		options.setSettingsPath(externalStoragePath + "options.txt");
	}
#endif
	reloadOptions();

}

void Minecraft::setSize(int w, int h) {
#ifndef STANDALONE_SERVER
    transformResolution(&w, &h);

	width  = w;
	height = h;

	if (width >= 1000) {
        #ifdef __APPLE__
            Gui::GuiScale = (width > 2000)? 8.0f : 4.0f;
        #else
            Gui::GuiScale = 4.0f;
        #endif
    }
	else if (width >= 800) {
#ifdef __APPLE__
        Gui::GuiScale = 4.0f;
#else
		Gui::GuiScale = 3.0f;
#endif
    }
	else if (width >= 400)
		Gui::GuiScale = 2.0f;
	else
		Gui::GuiScale = 1.0f;

	// Apply user GUI scale override: 0=auto, 1=small (0.5x), 2=normal (1x), 3=large (1.5x)
	switch (options.guiScale) {
		case 1: { float s = Gui::GuiScale * 0.5f; if (s < 1.0f) s = 1.0f; Gui::GuiScale = s; } break;
		case 2: break; // normal = auto
		case 3: Gui::GuiScale = Gui::GuiScale * 1.5f; break;
		default: break; // 0 = auto
	}

	Gui::InvGuiScale = 1.0f / Gui::GuiScale;
	// UI 可用宽 = 窗口宽 − 系统接管（不派发触摸）的右侧区域，见
	// AppPlatform::getUiRightInset()。贴右边缘的 HUD/开关因此不会画到摸不到地方。
	int uiRightInset = platform() ? platform()->getUiRightInset() : 0;
	int screenWidth  = (int)((width - uiRightInset) * Gui::InvGuiScale);
	int screenHeight = (int)(height * Gui::InvGuiScale);

	if (platform()) {
		float pixelsPerMillimeter = options.getProgressValue(&Options::Option::PIXELS_PER_MILLIMETER);
		pixelCalc.setPixelsPerMillimeter(pixelsPerMillimeter);
		pixelCalcUi.setPixelsPerMillimeter(pixelsPerMillimeter * Gui::InvGuiScale);
	}

	Config config = createConfig(this);
	gui.onConfigChanged(config);
    
	if (screen)
		screen->setSize(screenWidth, screenHeight);

	if (inputHolder)
		inputHolder->onConfigChanged(config);
	//LOGI("Setting size: %d, %d: %f\n", width, height, Gui::InvGuiScale);

#ifdef WIN32
	char resbuf[128];
	sprintf(resbuf, "            %d x %d @ scale %.2f", width, height, Gui::GuiScale);
	//gui.addMessage(resbuf);
#endif
#endif /* STANDALONE_SERVER */
}

void Minecraft::reloadOptions() {
	options.update();
	options.save();
	// Bug fix: options.update() reads the saved language (e.g. zh_CN) from
	// options.txt, but it runs AFTER Minecraft::init already called
	// I18n::loadLanguage with the pre-update default ("en_US"). Reload the
	// translation table now so the chosen language takes effect at startup
	// (previously the UI stayed English until the language was toggled).
	I18n::loadLanguage(platform(), options.language);
	bool wasTouchscreen = options.useTouchScreen;
	options.useTouchScreen = useTouchscreen();
	options.save();

	if ((wasTouchscreen != options.useTouchScreen) || (inputHolder == 0))
		_reloadInput();

	user->name = options.username;

	LOGI("Reloading-options\n");

    // @todo @fix Android and iOS behaves a bit differently when leaving
    //            an options screen (Android recreates OpenGL surface)
    setSize(width, height);
}

void Minecraft::_reloadInput() {
#ifndef STANDALONE_SERVER
	delete inputHolder;

#if defined(MACOS) || defined(LINUX) || defined(WIN32)
	// On macOS/Linux/Win32 always use keyboard + mouse regardless of useTouchScreen.
	// useTouchScreen stays true so ScreenChooser shows the touchscreen UI,
	// but input is handled by keyboard/mouse, not the d-pad touch holder.
	inputHolder = new CustomInputHolder(
		new KeyboardInput(&options),
		new MouseTurnInput(MouseTurnInput::MODE_DELTA, width/2, height/2),
		new MouseBuildInput());
#else
	if (useTouchscreen()) {
		inputHolder = new TouchInputHolder(this, &options);
	} else {
		#if defined(ANDROID) || defined(__APPLE__)
			inputHolder = new CustomInputHolder(
				new XperiaPlayInput(&options),
				new ControllerTurnInput(2, ControllerTurnInput::MODE_DELTA),
				new IBuildInput());
		#else
			inputHolder = new CustomInputHolder(
				new KeyboardInput(&options),
				new MouseTurnInput(MouseTurnInput::MODE_DELTA, width/2, height/2),
				new MouseBuildInput());
		#endif
	}
#endif

	mouseHandler.setTurnInput(inputHolder->getTurnInput());
	if (level && player) {
		player->input = inputHolder->getMoveInput();
	}
#endif
}


//
// Multiplayer
//
void Minecraft::locateMultiplayer() {
#ifndef STANDALONE_SERVER
	isLookingForMultiplayer = true;

	raknetInstance->pingForHosts(19132);
	netCallback = new ClientSideNetworkHandler(this, raknetInstance);
#endif
}

void Minecraft::cancelLocateMultiplayer() {
	isLookingForMultiplayer = false;

	raknetInstance->stopPingForHosts();

	delete netCallback;
	netCallback = NULL;
}

bool Minecraft::joinMultiplayer( const PingedCompatibleServer& server, int a2 )
{
	if (isLookingForMultiplayer && netCallback) {
		isLookingForMultiplayer = false;
		return raknetInstance->connect(server.address.ToString(false), server.address.GetPort());
	}
	return false;
}

void Minecraft::hostMultiplayer(int port) {
    // Tear down last instance
    raknetInstance->disconnect();
    delete netCallback;
    netCallback = NULL;

#if !defined(NO_NETWORK)
	netCallback = new ServerSideNetworkHandler(this, raknetInstance);
    #ifdef STANDALONE_SERVER
        raknetInstance->host(user->name, port, 16);
    #else
        raknetInstance->host(user->name, port);
    #endif
#endif
}

//
// Multi-player dimension world pool (host process only)
//
Level* Minecraft::getOrCreateDimensionLevel(int dimensionId)
{
	std::map<int, Level*>::iterator it = _dimensionLevels.find(dimensionId);
	if (it != _dimensionLevels.end())
		return it->second;

	if (!_mainWorld)
		return NULL;
	if (dimensionId == 0 && _mainWorld->dimension && _mainWorld->dimension->id == 0) {
		_dimensionLevels[0] = _mainWorld;
		return _mainWorld;
	}

	LevelData* ld = _mainWorld->getLevelData();
	if (!ld)
		return NULL;

	LevelSettings settings(ld->getSeed(), ld->getGameType(), ld->getWorldType(), dimensionId);

	// 维度 = 主世界文件夹下的一个独立子世界：用 "<主世界id>/dim<ID>" 当 levelId，
	// ExternalFileLevelStorageSource::getFullPath() 会把它解析成
	// <世界根>/<主世界>/dim<ID>/ —— 于是这个维度有自己的 level.dat、区块和玩家
	// 数据，和主世界互不污染；又因为它不在世界根目录的第一层，getLevelList()
	// 扫不到它（世界列表里不显示、也不能从列表进入）。
	//
	// [旧做法] 共用主世界的存档目录，只把区块文件名加个 "dN" 后缀隔离
	// (chunks.dN.r.*.dat)。那样维度蹭的是主世界的 level.dat，时间和出生点都独立
	// 不了，而且 setDimensionTag 每次切换都要丢弃 region 文件句柄。
	std::string dimLevelId = _mainLevelId + "/dim" + std::to_string(dimensionId);
	LevelStorage* storage = getLevelSource()->selectLevel(dimLevelId, false);
	if (!storage) {
		LOGE("getOrCreateDimensionLevel: could not open storage for dim %d\n", dimensionId);
		return NULL;
	}

	Dimension* dim = NULL;
#if defined(_WIN32) || defined(__ANDROID__)
	if (modEngine && modEngine->hasDimensionGenerator(dimensionId))
		dim = modEngine->createScriptedDimension(dimensionId);
#endif
	if (!dim)
		dim = Dimension::getNew(dimensionId);
	if (!dim) {
		LOGE("getOrCreateDimensionLevel: no dimension generator for dim %d\n", dimensionId);
		delete storage;
		return NULL;
	}

	ServerLevel* world = new ServerLevel(storage, _mainLevelName, settings, SharedConstants::GeneratorVersion, dim);

	_dimensionLevels[dimensionId] = world;
	LOGI("Created server dimension world dim=%d (%p)\n", dimensionId, world);

	// If a multiplayer server is active, register the new world with it so
	// level events and player routing for this dimension work.
	ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(netCallback);
	if (ssn)
		ssn->attachWorld(world);
	return world;
}

void Minecraft::switchActiveLevel(Level* newLevel)
{
	if (!newLevel || newLevel == level)
		return;

	Level* oldLevel = level;
	level = newLevel;

	if (gameMode)
		gameMode->initLevel(newLevel);
#ifndef STANDALONE_SERVER
	if (levelRenderer)
		levelRenderer->setLevel(newLevel);
	if (particleEngine)
		particleEngine->setLevel(newLevel);
#endif
	if (player) {
		cameraTargetPlayer = player;
	}

	newLevel->setUpdateLights(true);
	newLevel->updateSkyBrightness();
	LOGI("switchActiveLevel: dim %d (%p)\n",
		newLevel->dimension ? newLevel->dimension->id : -1, newLevel);
	(void)oldLevel;
}

void Minecraft::teleportPlayerToDimension(int dimensionId, float x, float y, float z)
{
	if (!player || !level)
		return;

	// Multiplayer host: the server handles the whole migration.
	ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(netCallback);
	if (ssn) {
		ssn->teleportPlayerToDimension(player, dimensionId, x, y, z);
		return;
	}

	// Single-player: the target dimension is its own world with its own save
	// (see beginDimensionTravel), so entering it runs the same background
	// loading pipeline as entering a world - never the old synchronous
	// "clear every chunk cache + swap generator + pre-generate" path.
	beginDimensionTravel(dimensionId, x, y, z);
}

// 进世界完成后调用一次：上次退出时不在主世界，就自动切回那个维度。
// 落点从“那个维度子世界自己的存档”里读 —— 每个子世界的存档都存着“玩家在该
// 维度最后的位置”，所以 mod 不用再自己记一份坐标。
void Minecraft::maybeReturnToLastDimension() {
	if (_dimAutoReturnDone)
		return;
	_dimAutoReturnDone = true;
	if (!level || !player || level->isClientSide || _hasPendingTravel)
		return;
	int curDim = level->dimension ? level->dimension->id : 0;
	if (curDim != 0)
		return;                              // 只在“从主世界进游戏”时判断
	LevelData* ld = level->getLevelData();
	int lastDim = ld ? ld->getDimension() : 0;
	if (lastDim == 0 || lastDim == curDim)
		return;
	float x = 0, y = 64, z = 0;
	if (!getDimensionSavedPos(lastDim, x, y, z)) {
#ifndef STANDALONE_SERVER
		if (modEngine)
			modEngine->log("auto return: dim " + std::to_string(lastDim) + " 没有存档坐标，跳过");
#endif
		return;
	}
#ifndef STANDALONE_SERVER
	if (modEngine)
		modEngine->log("auto return: 回维度 " + std::to_string(lastDim)
		               + " at " + std::to_string((int)x) + "," + std::to_string((int)y) + "," + std::to_string((int)z));
#endif
	beginDimensionTravel(lastDim, x, y, z);
}

// 从某个维度子世界的存档里读“玩家在该维度的位置”（NBT 里的 Pos）。
bool Minecraft::getDimensionSavedPos(int dimensionId, float& x, float& y, float& z) {
	Level* w = getOrCreateDimensionLevel(dimensionId);
	if (!w)
		return false;
	LevelData* ld = w->getLevelData();
	CompoundTag* tag = ld ? ld->getLoadedPlayerTag() : NULL;
	if (!tag || !tag->contains("Pos", Tag::TAG_List))
		return false;
	ListTag* pos = tag->getList("Pos");
	if (pos->size() < 3)
		return false;
	x = ((FloatTag*)pos->get(0))->data;
	y = ((FloatTag*)pos->get(1))->data;
	z = ((FloatTag*)pos->get(2))->data;
	return true;
}

bool Minecraft::beginDimensionTravel(int dimensionId, float x, float y, float z)
{
	if (!level || !player)
		return false;

	// 上一个世界还在后台加载时不要再切：加载线程读的是成员 level，两个加载
	// 并发跑会把两个世界搅在一起（进度、isGeneratingLevel 会互相打断）。
	if (isGeneratingLevel || !_hasSignaledGeneratingLevelFinished)
		return false;

	// 把整个维度切换过程记到 mod 日志（平时日志是关的；这个流程一次性、出问题
	// 又难复现，值得留痕。_levelGenerated() 末尾会把它关回去）。
	// （ModEngine 在服务器构建里是被排除的，所以日志这几行单独包一层。）
#ifndef STANDALONE_SERVER
	g_modLogStartupPhase = true;
	if (modEngine)
		modEngine->log("beginDimensionTravel: dim=" + std::to_string(dimensionId)
		               + " from=" + std::to_string(level->dimension ? level->dimension->id : -1)
		               + " at " + std::to_string((int)x) + "," + std::to_string((int)y)
		               + "," + std::to_string((int)z));
#endif

	// 已经在目标维度：这就是一次普通传送。
	if (level->dimension && level->dimension->id == dimensionId) {
		player->moveTo(x, y, z, player->yRot, player->xRot);
		return true;
	}

	Level* oldWorld = level;

	// 1) 离开前把当前世界存好（联机客户端的世界不落盘，服务器/单机才存）。
	if (!oldWorld->isClientSide) {
		if (oldWorld->getChunkSource())
			oldWorld->getChunkSource()->saveAll(true);
		oldWorld->saveLevelData();
	}

	// 1b) 主世界的存档记下“玩家现在去哪个维度”：进游戏时靠它把玩家放回原处。
	//     每个维度子世界的存档各存一份“该维度内的玩家位置”，所以只要知道
	//     是哪个维度，落点就能从那份存档里读出来 —— mod 不必再自己记坐标。
	if (_mainWorld && !_mainWorld->isClientSide) {
		_mainWorld->getLevelData()->setDimension(dimensionId);
		_mainWorld->saveLevelData();
	}

	// 2) 打开（第一次则是创建）目标纬度自己的世界存档。
	Level* target = getOrCreateDimensionLevel(dimensionId);
	if (!target) {
#ifndef STANDALONE_SERVER
		if (modEngine)
			modEngine->log("beginDimensionTravel: FAILED, no world for dim " + std::to_string(dimensionId));
#endif
		LOGE("beginDimensionTravel: no world for dim %d\n", dimensionId);
		return false;
	}

	// 3) 把玩家实体搬过去（entityId 不变），并记下落点 —— 后台加载里的
	//    resetPos() 只会把玩家往上抬出方块，不会还原任意落点。
	player->dimension = dimensionId;
	player->moveTo(x, y, z, player->yRot, player->xRot);
	if (oldWorld->getEntity(player->entityId) == player)
		oldWorld->moveEntityTo(player, target);

	_hasPendingTravel   = true;
	_pendingTravelDim   = dimensionId;
	_pendingTravelX     = x;
	_pendingTravelY     = y;
	_pendingTravelZ     = z;

	// 4) 走"进入世界"那条加载流程：当前世界换成目标世界，区块在后台线程里加载
	//    （加载线程会调 mod 的 JS 生成地形，靠 ModEngine 的互斥锁与主线程串行化），
	//    加载界面由 ProgressScreen 负责（加载完自己关掉）。
	// 加载期间挂起非玩家实体的 tick（见 Level::suspendMobTicking）：新维度里的生物会
	// 立刻跑起来并播脚步声，而此时相机玩家还没接上 —— 那就是空指针崩溃现场。
	target->suspendMobTicking = true;
	setLevel(target, "Loading dimension");
#ifndef STANDALONE_SERVER
	setScreen(new ProgressScreen());
	if (modEngine)
		modEngine->log("beginDimensionTravel: setLevel done, load thread started (dim="
		               + std::to_string(dimensionId) + ")");
#endif
	return true;
}

void Minecraft::destroyDimensionWorlds()
{
	// Called from leaveGame() BEFORE the rendered level is deleted.
	// The main (persistent) world is saved when it isn't the rendered one;
	// non-rendered dimension worlds are in-memory only and just dropped.
	// The rendered level stays owned by leaveGame().
	Level* rendered = level;

	if (_mainWorld && _mainWorld != rendered && _mainWorld->getChunkSource()) {
		_mainWorld->getChunkSource()->saveAll(true);
		_mainWorld->saveGame();
	}

	for (std::map<int, Level*>::iterator it = _dimensionLevels.begin(); it != _dimensionLevels.end(); ++it) {
		Level* w = it->second;
		if (!w || w == rendered)
			continue;
		delete w->getLevelStorage();
		delete w;
	}
	_dimensionLevels.clear();
	_mainWorld = NULL;
}

//
// Level generation
//
/*static*/

	void* Minecraft::prepareLevel_tspawn(void *p_param)
{
	Minecraft* mc = (Minecraft*) p_param;
	mc->generateLevel("Currently not used", mc->level);
	return 0;
}

void Minecraft::generateLevel( const std::string& message, Level* level )
{
	Stopwatch s;
	s.start();
	prepareLevel(message);
	s.stop();
	s.print("Level generated: ");

	// "Unlock"
	isGeneratingLevel = false;
}

void Minecraft::_levelGenerated()
{
#ifndef STANDALONE_SERVER
	if (player == NULL) {
		player = (LocalPlayer*) gameMode->createPlayer(level);
		gameMode->initPlayer(player);
	}

	if (player) {
		player->input = inputHolder->getMoveInput();
	}

	if (levelRenderer != NULL) levelRenderer->setLevel(level);
	if (particleEngine != NULL) particleEngine->setLevel(level);

	gameMode->adjustPlayer(player);
	gui.onLevelGenerated();
#endif

	level->validateSpawn();
	// 玩家已经在这个世界里时（维度切换用 moveEntityTo() 把他搬过来的）不要
	// 再 addEntity 一次：addEntity 会先把"同 entityId 的既有实体"removeEntity()，
	// 而那个实体就是玩家自己 —— 它会被标成 removed，之后 tickEntities() 直接
	// 跳过它，玩家就再也动不了。（原版进入世界时不触发，因为 leaveGame() 把
	// player 置了 NULL，这里是新玩家；切维度时玩家一直在场。）
	level->loadPlayer(player, level->getEntity(player->entityId) != player);
	// if we are client side, we trust the server to have given us a correct position
	if (player && !level->isClientSide) {
		player->resetPos(false);
	}

	// 维度切换：目标世界加载完了，把玩家精确落到传送点。上面那些 resetPos()
	// 只保证"不卡在方块里"，不保证落点（它们会保留 x/z，但 y 会随地形走）。
	if (_hasPendingTravel && level &&
	    level->dimension && level->dimension->id == _pendingTravelDim) {
		_hasPendingTravel = false;
		if (player) {
			player->moveTo(_pendingTravelX, _pendingTravelY, _pendingTravelZ,
			               player->yRot, player->xRot);
			player->resetPos(false);   // 抬出方块、清速度，落点 x/z 不变
		}
#ifndef STANDALONE_SERVER
		if (modEngine)
			modEngine->log("dimension travel landed: dim=" + std::to_string(_pendingTravelDim)
			               + " at " + std::to_string((int)_pendingTravelX) + ","
			               + std::to_string((int)_pendingTravelY) + "," + std::to_string((int)_pendingTravelZ));
#endif
	}

	this->cameraTargetPlayer = player;
	// 世界就绪 + 相机玩家已接上 → 恢复生物 tick（setLevel 时挂起的）。
	if (level) level->suspendMobTicking = false;

	if (raknetInstance->isServer())
		raknetInstance->announceServer(user->name);

	if (netCallback) {
		netCallback->levelGenerated(level);
	}

#if defined(WIN32) || defined(RPI)
#ifndef STANDALONE_SERVER
	// 旧版调试用的 TCP 命令端口（CommandServer）：**没有任何鉴权**，任何能连上
	// 端口的人都能改世界。独立服务器不开它 —— 服务器的命令走自己的控制台与
	// 聊天命令（服务器权威）。
	if (_commandServer) {
		delete _commandServer;
	}
	_commandServer = new CommandServer(this);
	_commandServer->init(commandPort);
#endif
#endif

	// Hack to (hopefully) get the players to show (note: in LevelListener
	// instead, since adding yourself always generates a entityAdded)
	//EntityRenderDispatcher::getInstance()->onGraphicsReset();
	_hasSignaledGeneratingLevelFinished = true;

	// Reset the timer so the game loop doesn't try to catch up with ticks
	// that accumulated during level generation (which can take 6+ seconds).
	timer.skipTime();

	// 上次退出时在别的维度 -> 自动切回去。放在 onJoinWorld 之前：这样 mod 那些
	// “自己记维度再切”的老逻辑不会和引擎抢着切。
	maybeReturnToLastDimension();
#if defined(_WIN32) || defined(__ANDROID__)
	if (modEngine) {
		modEngine->fireEvent("onJoinWorld");
		// World mod-guard fallback: selectLevel() already blocks mismatched
		// worlds before generation starts; this catches any other entry path
		// (e.g. multiplayer / dimension switches) that bypasses it.
		if (level) {
			std::string warning = modEngine->checkWorldMods(level->getLevelData()->getModList());
			if (!warning.empty()) {
#ifndef STANDALONE_SERVER
				modEngine->log("[worldguard] showing ModWarningScreen (fallback)");
				setScreen(new ModWarningScreen(warning));
#else
				// 服务器是唯一权威：存档里的 mod 列表与当前不一致时，直接把当前列表
				// 写回存档（客户端那边是弹警告屏让玩家自己选；服务器没屏幕也没玩家
				// 可问，而且它本来就是权威 —— 它说有哪些 mod 就是哪些）。
				modEngine->log("[worldguard] " + warning + " -> 按当前 mod 列表更新存档");
				if (level && level->getLevelData()) {
					level->getLevelData()->setModList(modEngine->buildModNameList());
					level->saveLevelData();
				}
#endif
			}
		}
	}
#endif

	// Startup phase is over (mods loaded, world generated/loaded and joined):
	// stop writing the mod log during normal gameplay so log() costs nothing.
	// Opening F3 re-enables it on demand.
#ifndef STANDALONE_SERVER
	// 客户端：进入正常游玩后关掉 mod 日志（F3 可按需打开）。
	// 服务器：**一直保留** —— 专服没有 F3 面板，日志是排查服务器 mod 的唯一手段。
	//
	// 【临时诊断】先不关：进入世界后马上就闪退，关掉日志就什么都看不到了。
	// 定位完必须还原成 g_modLogStartupPhase = false;
	//g_modLogStartupPhase = false;
#endif
}

Player* Minecraft::respawnPlayer(int playerId) {
	for (unsigned int i = 0; i < level->players.size(); ++i) {
		if (level->players[i]->entityId == playerId) {
			resetPlayer(level->players[i]);
			return level->players[i];
		}
	}
	return NULL;
}

void Minecraft::resetPlayer(Player* player) {
	level->validateSpawn();
	player->reset();

	Pos p;
	if(player->hasRespawnPosition()) {
		p = player->getRespawnPosition();
	}
	else {
		p = level->getSharedSpawnPos();
	}
	player->setPos((float)p.x + 0.5f, (float)p.y + 1.0f, (float)p.z + 0.5f);
	player->resetPos(true);

	if (isCreativeMode())
		player->inventory->clearInventoryWithDefault();
}

void Minecraft::respawnPlayer() {
	// RESET THE FRACKING PLAYER HERE
	//bool slowCheck = false;
	//for (int i = 0; i < level->entities.size(); ++i)
	//	if (level->entities[i] == player) slowCheck = true;
	//
	//LOGI("Has entity? %d, %d\n", level->getEntity(player->entityId), slowCheck);

	resetPlayer(player);

	// tell server (or other client) that we re-spawned
	RespawnPacket packet(player);
	raknetInstance->send(packet);
}

void Minecraft::onGraphicsReset()
{
#ifndef STANDALONE_SERVER
	textures->clear();
	
	font->onGraphicsReset();
	gui.onGraphicsReset();

	if (levelRenderer) levelRenderer->onGraphicsReset();
	if (gameRenderer) gameRenderer->onGraphicsReset();

	EntityRenderDispatcher::getInstance()->onGraphicsReset();
	TileEntityRenderDispatcher::getInstance()->onGraphicsReset();
#endif
}

int Minecraft::getProgressStatusId() {
	return progressStageStatusId;
}

const char* Minecraft::getProgressMessage()
{
	return progressMessages[progressStageStatusId];
}

bool Minecraft::isLevelGenerated()
{
	return level != NULL && !isGeneratingLevel;
}

LevelStorageSource* Minecraft::getLevelSource()
{
	return storageSource;
}

int Minecraft::getLicenseId() {
	if (!LicenseCodes::isReady(_licenseId))
		_licenseId = platform()->checkLicense();
	return _licenseId;
}

void Minecraft::audioEngineOn() {
#ifndef STANDALONE_SERVER
    soundEngine->enable(true);
#endif
}
void Minecraft::audioEngineOff() {
#ifndef STANDALONE_SERVER
    soundEngine->enable(false);
#endif
}

void Minecraft::setIsCreativeMode(bool isCreative)
{
#ifdef CREATORMODE
	delete gameMode;
	gameMode = new CreatorMode(this);
	_isCreativeMode = true;
#else
	if (!gameMode || isCreative != _isCreativeMode)
	{
		delete gameMode;
		if (isCreative) gameMode = new CreativeMode(this);
		else			gameMode = new SurvivalMode(this);
		_isCreativeMode = isCreative;
	}
#endif
	if (player)
		gameMode->initAbilities(player->abilities);
}

bool Minecraft::isCreativeMode() {
	return _isCreativeMode;
}

bool Minecraft::isKindleFire(int kindleVersion) {
	if (kindleVersion != 1)
		return false;

	std::string model = platform()->getPlatformStringVar(PlatformStringVars::DEVICE_BUILD_MODEL);
	std::string modelLower(model);
	std::transform(modelLower.begin(), modelLower.end(), modelLower.begin(), tolower);

	return (modelLower.find("kindle") != std::string::npos) && (modelLower.find("fire") != std::string::npos);
}

bool Minecraft::transformResolution(int* w, int* h)
{
	bool changed = false;

	// Kindle Fire 1: reporting wrong height in
	// certain cases (e.g. after screen lock)
	if (isKindleFire(1) && *h >= 560 && *h <= 620) {
		*h = 580;
		changed = true;
	}

	return changed;
}

ICreator* Minecraft::getCreator()
{
#ifdef CREATORMODE
	return ((CreatorMode*)gameMode)->getCreator();
#else
	return NULL;
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, bool value ) {
	if(netCallback != NULL && option == &Options::Option::SERVER_VISIBLE) {
		ServerSideNetworkHandler* ss = (ServerSideNetworkHandler*) netCallback;
		ss->allowIncomingConnections(value);
	}
}

void Minecraft::optionUpdated( const Options::Option* option, float value ) {
#ifndef STANDALONE_SERVER
	if(option == &Options::Option::PIXELS_PER_MILLIMETER) {
		pixelCalcUi.setPixelsPerMillimeter(value * Gui::InvGuiScale);
		pixelCalc.setPixelsPerMillimeter(value);
	}
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, int value ) {
	if (option == &Options::Option::GUI_SCALE) {
		setSize(width, height);
	} else if (option == &Options::Option::LANGUAGE) {
		// Re-open the options screen so every cached label picks up the
		// newly loaded language immediately.
#ifndef STANDALONE_SERVER
		if (screen != NULL && dynamic_cast<OptionsScreen*>(screen) != NULL) {
			setScreen(new OptionsScreen());
		}
#endif
	}
}
