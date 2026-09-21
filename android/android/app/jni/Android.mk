# 由 gen_android_mk.py 自动生成，勿手改。
#
# MCPE 0.8.1 Android 版（GLES 1.1 固定管线）。
# 源文件清单来自 Windows 版 MinecraftWin32_GL.vcxproj，逐项映射过来。
#
# 为什么拆成多个 static library：393 个 .o 一次链接在 Windows 上会因为
# CreateProcess 命令行超长而失败（make (e=87) 参数错误）。

LOCAL_PATH := $(call my-dir)
MPE_ROOT := $(LOCAL_PATH)

# native_app_glue（NativeActivity 入口）提前 import，之后各 module 才能引用。
# 注意：import-module 会把 LOCAL_PATH 切成 native_app_glue 的目录，
# 而 LOCAL_SRC_FILES 的相对路径要以本目录为基准，所以立即恢复回来。
$(call import-module,android/native_app_glue)
LOCAL_PATH := $(MPE_ROOT)

MPE_INC := \
    $(MPE_ROOT)/../../../handheld/src \
    $(MPE_ROOT)/../../../handheld/src/gui08/include \
    $(MPE_ROOT)/../../../handheld/src/gui08 \
    $(MPE_ROOT)/../../../handheld/src/raknet \
    $(MPE_ROOT)/../../../handheld/thirdparty \
    $(MPE_ROOT)/../../../handheld/thirdparty/duktape \
    $(MPE_ROOT)/../../../handheld/thirdparty/stb \
    $(MPE_ROOT)/../../../handheld/thirdparty/libpng-1.6.40

# 注意：不要加 handheld/lib/include —— 那是 Win32 版用的旧 EGL/GLES/SLES 头，
# 会盖掉 NDK 自带的系统头。

# RakNet 裁剪开关：与 Windows 版 vcxproj 完全一致（关掉用不到的 NAT 穿透 /
# Router2 / UDP 代理），否则会缺少 UDPForwarder 等未编译文件的符号。
MPE_RAKNET_DEFS := \
    -D_RAKNET_SUPPORT_NatPunchthroughClient=0 \
    -D_RAKNET_SUPPORT_NatPunchthroughServer=0 \
    -D_RAKNET_SUPPORT_NatTypeDetectionClient=0 \
    -D_RAKNET_SUPPORT_NatTypeDetectionServer=0 \
    -D_RAKNET_SUPPORT_Router2=0 \
    -D_RAKNET_SUPPORT_UDPProxyClient=0 \
    -D_RAKNET_SUPPORT_UDPProxyCoordinator=0 \
    -D_RAKNET_SUPPORT_UDPProxyServer=0

MPE_CFLAGS   := -w -fno-strict-aliasing $(MPE_RAKNET_DEFS)
MPE_CPPFLAGS := -w -fno-strict-aliasing -frtti $(MPE_RAKNET_DEFS)

# ── libpng（ModEngine 运行时解码 png/jpg 用） ─────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_png
LOCAL_SRC_FILES := \
    ../../../handheld/thirdparty/libpng-1.6.40/png.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngerror.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngget.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngmem.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngpread.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngread.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngrio.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngrtran.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngrutil.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngset.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngtrans.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngwio.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngwrite.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngwtran.c \
    ../../../handheld/thirdparty/libpng-1.6.40/pngwutil.c \
    ../../../handheld/thirdparty/libpng-1.6.40/arm/arm_init.c \
    ../../../handheld/thirdparty/libpng-1.6.40/arm/filter_neon_intrinsics.c \
    ../../../handheld/thirdparty/libpng-1.6.40/arm/palette_neon_intrinsics.c

LOCAL_C_INCLUDES := $(MPE_ROOT)/../../../handheld/thirdparty/libpng-1.6.40
# intrinsics 实现（不用 .S 汇编，避开 32/64 位汇编差异）
LOCAL_CFLAGS := -w -DPNG_ARM_NEON_IMPLEMENTATION=1
LOCAL_ARM_NEON := true
include $(BUILD_STATIC_LIBRARY)

# ── mpe_core（核心/平台/模组/网络/服务端）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_core
LOCAL_SRC_FILES := \
    ../../../handheld/src/mod/ModEngine.cpp \
    ../../../handheld/src/mod/GlJsBindings.cpp \
    ../../../handheld/src/mod/ModZip.cpp \
    ../../../handheld/src/mod/stb_vorbis.c \
    ../../../handheld/thirdparty/duktape/duktape.c \
    ../../../handheld/src/locale/I18n.cpp \
    ../../../handheld/src/main.cpp \
    ../../../handheld/src/nbt/Tag.cpp \
    ../../../handheld/src/network/ClientSideNetworkHandler.cpp \
    ../../../handheld/src/network/NATPunchHandler.cpp \
    ../../../handheld/src/network/command/CommandServer.cpp \
    ../../../handheld/src/network/NetEventCallback.cpp \
    ../../../handheld/src/network/Packet.cpp \
    ../../../handheld/src/network/PHPDirectoryServer2.cpp \
    ../../../handheld/src/network/RakNetInstance.cpp \
    ../../../handheld/src/network/ServerSideNetworkHandler.cpp \
    ../../../handheld/src/NinecraftApp.cpp \
    ../../../handheld/src/Performance.cpp \
    ../../../handheld/src/platform/CThread.cpp \
    ../../../handheld/src/platform/input/Controller.cpp \
    ../../../handheld/src/platform/input/Keyboard.cpp \
    ../../../handheld/src/platform/input/Mouse.cpp \
    ../../../handheld/src/platform/input/Multitouch.cpp \
    ../../../handheld/src/platform/time.cpp \
    ../../../handheld/src/server/ServerLevel.cpp \
    ../../../handheld/src/server/ServerPlayer.cpp \
    ../../../handheld/src/util/DataIO.cpp \
    ../../../handheld/src/util/Mth.cpp \
    ../../../handheld/src/util/PerfRenderer.cpp \
    ../../../handheld/src/util/StringUtils.cpp \
    ../../../handheld/src/util/PerfTimer.cpp \
    ../../../handheld/src/AppPlatform_android.cpp \
    ../../../handheld/src/platform/audio/SoundSystemSL.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
LOCAL_STATIC_LIBRARIES := android_native_app_glue
include $(BUILD_STATIC_LIBRARY)

# ── mpe_raknet（RakNet 网络库）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_raknet
LOCAL_SRC_FILES := \
    ../../../handheld/src/raknet/BitStream.cpp \
    ../../../handheld/src/raknet/CCRakNetSlidingWindow.cpp \
    ../../../handheld/src/raknet/CCRakNetUDT.cpp \
    ../../../handheld/src/raknet/CheckSum.cpp \
    ../../../handheld/src/raknet/CloudClient.cpp \
    ../../../handheld/src/raknet/CloudCommon.cpp \
    ../../../handheld/src/raknet/CloudServer.cpp \
    ../../../handheld/src/raknet/CommandParserInterface.cpp \
    ../../../handheld/src/raknet/ConnectionGraph2.cpp \
    ../../../handheld/src/raknet/ConsoleServer.cpp \
    ../../../handheld/src/raknet/DataCompressor.cpp \
    ../../../handheld/src/raknet/DirectoryDeltaTransfer.cpp \
    ../../../handheld/src/raknet/DS_BytePool.cpp \
    ../../../handheld/src/raknet/DS_ByteQueue.cpp \
    ../../../handheld/src/raknet/DS_HuffmanEncodingTree.cpp \
    ../../../handheld/src/raknet/DS_Table.cpp \
    ../../../handheld/src/raknet/DynDNS.cpp \
    ../../../handheld/src/raknet/EmailSender.cpp \
    ../../../handheld/src/raknet/EncodeClassName.cpp \
    ../../../handheld/src/raknet/EpochTimeToString.cpp \
    ../../../handheld/src/raknet/FileList.cpp \
    ../../../handheld/src/raknet/FileListTransfer.cpp \
    ../../../handheld/src/raknet/FileOperations.cpp \
    ../../../handheld/src/raknet/FormatString.cpp \
    ../../../handheld/src/raknet/FullyConnectedMesh2.cpp \
    ../../../handheld/src/raknet/Getche.cpp \
    ../../../handheld/src/raknet/Gets.cpp \
    ../../../handheld/src/raknet/GetTime.cpp \
    ../../../handheld/src/raknet/gettimeofday.cpp \
    ../../../handheld/src/raknet/GridSectorizer.cpp \
    ../../../handheld/src/raknet/HTTPConnection.cpp \
    ../../../handheld/src/raknet/IncrementalReadInterface.cpp \
    ../../../handheld/src/raknet/Itoa.cpp \
    ../../../handheld/src/raknet/LinuxStrings.cpp \
    ../../../handheld/src/raknet/LocklessTypes.cpp \
    ../../../handheld/src/raknet/LogCommandParser.cpp \
    ../../../handheld/src/raknet/MessageFilter.cpp \
    ../../../handheld/src/raknet/NatPunchthroughClient.cpp \
    ../../../handheld/src/raknet/NatPunchthroughServer.cpp \
    ../../../handheld/src/raknet/NatTypeDetectionClient.cpp \
    ../../../handheld/src/raknet/NatTypeDetectionCommon.cpp \
    ../../../handheld/src/raknet/NatTypeDetectionServer.cpp \
    ../../../handheld/src/raknet/NetworkIDManager.cpp \
    ../../../handheld/src/raknet/NetworkIDObject.cpp \
    ../../../handheld/src/raknet/PacketConsoleLogger.cpp \
    ../../../handheld/src/raknet/PacketFileLogger.cpp \
    ../../../handheld/src/raknet/PacketizedTCP.cpp \
    ../../../handheld/src/raknet/PacketLogger.cpp \
    ../../../handheld/src/raknet/PacketOutputWindowLogger.cpp \
    ../../../handheld/src/raknet/PluginInterface2.cpp \
    ../../../handheld/src/raknet/RakMemoryOverride.cpp \
    ../../../handheld/src/raknet/RakNetCommandParser.cpp \
    ../../../handheld/src/raknet/RakNetSocket.cpp \
    ../../../handheld/src/raknet/RakNetStatistics.cpp \
    ../../../handheld/src/raknet/RakNetTransport2.cpp \
    ../../../handheld/src/raknet/RakNetTypes.cpp \
    ../../../handheld/src/raknet/RakPeer.cpp \
    ../../../handheld/src/raknet/RakSleep.cpp \
    ../../../handheld/src/raknet/RakString.cpp \
    ../../../handheld/src/raknet/RakThread.cpp \
    ../../../handheld/src/raknet/RakWString.cpp \
    ../../../handheld/src/raknet/Rand.cpp \
    ../../../handheld/src/raknet/rdlmalloc.cpp \
    ../../../handheld/src/raknet/ReadyEvent.cpp \
    ../../../handheld/src/raknet/ReliabilityLayer.cpp \
    ../../../handheld/src/raknet/ReplicaManager3.cpp \
    ../../../handheld/src/raknet/RPC4Plugin.cpp \
    ../../../handheld/src/raknet/SecureHandshake.cpp \
    ../../../handheld/src/raknet/SendToThread.cpp \
    ../../../handheld/src/raknet/SHA1.cpp \
    ../../../handheld/src/raknet/SignaledEvent.cpp \
    ../../../handheld/src/raknet/SimpleMutex.cpp \
    ../../../handheld/src/raknet/SocketLayer.cpp \
    ../../../handheld/src/raknet/StringCompressor.cpp \
    ../../../handheld/src/raknet/StringTable.cpp \
    ../../../handheld/src/raknet/SuperFastHash.cpp \
    ../../../handheld/src/raknet/TableSerializer.cpp \
    ../../../handheld/src/raknet/TCPInterface.cpp \
    ../../../handheld/src/raknet/TeamBalancer.cpp \
    ../../../handheld/src/raknet/TelnetTransport.cpp \
    ../../../handheld/src/raknet/ThreadsafePacketLogger.cpp \
    ../../../handheld/src/raknet/TwoWayAuthentication.cpp \
    ../../../handheld/src/raknet/UDPProxyClient.cpp \
    ../../../handheld/src/raknet/UDPProxyCoordinator.cpp \
    ../../../handheld/src/raknet/UDPProxyServer.cpp \
    ../../../handheld/src/raknet/VariableDeltaSerializer.cpp \
    ../../../handheld/src/raknet/VariableListDeltaTracker.cpp \
    ../../../handheld/src/raknet/VariadicSQLParser.cpp \
    ../../../handheld/src/raknet/WSAStartupSingleton.cpp \
    ../../../handheld/src/raknet/_FindFirst.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# ── mpe_world（世界/方块/实体/物品）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_world
LOCAL_SRC_FILES := \
    ../../../handheld/src/world/Direction.cpp \
    ../../../handheld/src/world/entity/AgableMob.cpp \
    ../../../handheld/src/world/entity/ai/control/MoveControl.cpp \
    ../../../handheld/src/world/entity/animal/Animal.cpp \
    ../../../handheld/src/world/entity/animal/Chicken.cpp \
    ../../../handheld/src/world/entity/animal/Cow.cpp \
    ../../../handheld/src/world/entity/animal/Pig.cpp \
    ../../../handheld/src/world/entity/animal/Sheep.cpp \
    ../../../handheld/src/world/entity/animal/WaterAnimal.cpp \
    ../../../handheld/src/world/entity/Entity.cpp \
    ../../../handheld/src/world/entity/EntityFactory.cpp \
    ../../../handheld/src/world/entity/FlyingMob.cpp \
    ../../../handheld/src/world/entity/HangingEntity.cpp \
    ../../../handheld/src/world/entity/item/FallingTile.cpp \
    ../../../handheld/src/world/entity/item/ItemEntity.cpp \
    ../../../handheld/src/world/entity/item/PrimedTnt.cpp \
    ../../../handheld/src/world/entity/item/TripodCamera.cpp \
    ../../../handheld/src/world/entity/Mob.cpp \
    ../../../handheld/src/world/entity/MobCategory.cpp \
    ../../../handheld/src/world/entity/monster/Creeper.cpp \
    ../../../handheld/src/world/entity/monster/ScriptedMob.cpp \
    ../../../handheld/src/world/entity/monster/Monster.cpp \
    ../../../handheld/src/world/entity/monster/PigZombie.cpp \
    ../../../handheld/src/world/entity/monster/Skeleton.cpp \
    ../../../handheld/src/world/entity/monster/Spider.cpp \
    ../../../handheld/src/world/entity/monster/Zombie.cpp \
    ../../../handheld/src/world/entity/Motive.cpp \
    ../../../handheld/src/world/entity/Painting.cpp \
    ../../../handheld/src/world/entity/PathFinderMob.cpp \
    ../../../handheld/src/world/entity/player/Inventory.cpp \
    ../../../handheld/src/world/entity/player/Player.cpp \
    ../../../handheld/src/world/entity/projectile/Arrow.cpp \
    ../../../handheld/src/world/entity/projectile/Throwable.cpp \
    ../../../handheld/src/world/entity/SynchedEntityData.cpp \
    ../../../handheld/src/world/food/SimpleFoodData.cpp \
    ../../../handheld/src/world/inventory/BaseContainerMenu.cpp \
    ../../../handheld/src/world/inventory/ContainerMenu.cpp \
    ../../../handheld/src/world/inventory/FillingContainer.cpp \
    ../../../handheld/src/world/inventory/FurnaceMenu.cpp \
    ../../../handheld/src/world/item/ArmorItem.cpp \
    ../../../handheld/src/world/item/BedItem.cpp \
    ../../../handheld/src/world/item/crafting/ArmorRecipes.cpp \
    ../../../handheld/src/world/item/crafting/FurnaceRecipes.cpp \
    ../../../handheld/src/world/item/crafting/OreRecipes.cpp \
    ../../../handheld/src/world/item/crafting/Recipe.cpp \
    ../../../handheld/src/world/item/crafting/Recipes.cpp \
    ../../../handheld/src/world/item/crafting/StructureRecipes.cpp \
    ../../../handheld/src/world/item/crafting/ToolRecipes.cpp \
    ../../../handheld/src/world/item/crafting/WeaponRecipes.cpp \
    ../../../handheld/src/world/item/DyePowderItem.cpp \
    ../../../handheld/src/world/item/HangingEntityItem.cpp \
    ../../../handheld/src/world/item/HatchetItem.cpp \
    ../../../handheld/src/world/item/HoeItem.cpp \
    ../../../handheld/src/world/item/Item.cpp \
    ../../../handheld/src/world/item/ItemInstance.cpp \
    ../../../handheld/src/world/item/PickaxeItem.cpp \
    ../../../handheld/src/world/item/ShovelItem.cpp \
    ../../../handheld/src/world/level/biome/Biome.cpp \
    ../../../handheld/src/world/level/biome/BiomeSource.cpp \
    ../../../handheld/src/world/level/chunk/LevelChunk.cpp \
    ../../../handheld/src/world/level/dimension/Dimension.cpp \
    ../../../handheld/src/world/level/Explosion.cpp \
    ../../../handheld/src/world/level/Level.cpp \
    ../../../handheld/src/world/level/levelgen/CanyonFeature.cpp \
    ../../../handheld/src/world/level/levelgen/DungeonFeature.cpp \
    ../../../handheld/src/world/level/levelgen/feature/Feature.cpp \
    ../../../handheld/src/world/level/levelgen/LargeCaveFeature.cpp \
    ../../../handheld/src/world/level/levelgen/LargeFeature.cpp \
    ../../../handheld/src/world/level/levelgen/RandomLevelSource.cpp \
    ../../../handheld/src/world/level/levelgen/ScriptedChunkSource.cpp \
    ../../../handheld/src/world/level/dimension/ScriptedDimension.cpp \
    ../../../handheld/src/world/level/levelgen/synth/ImprovedNoise.cpp \
    ../../../handheld/src/world/level/levelgen/synth/PerlinNoise.cpp \
    ../../../handheld/src/world/level/levelgen/synth/Synth.cpp \
    ../../../handheld/src/world/level/LightLayer.cpp \
    ../../../handheld/src/world/level/LightUpdate.cpp \
    ../../../handheld/src/world/level/material/Material.cpp \
    ../../../handheld/src/world/level/MobSpawner.cpp \
    ../../../handheld/src/world/level/pathfinder/Path.cpp \
    ../../../handheld/src/world/level/Region.cpp \
    ../../../handheld/src/world/level/storage/ExternalFileLevelStorage.cpp \
    ../../../handheld/src/world/level/storage/ExternalFileLevelStorageSource.cpp \
    ../../../handheld/src/world/level/storage/FolderMethods.cpp \
    ../../../handheld/src/world/level/storage/LevelData.cpp \
    ../../../handheld/src/world/level/storage/LevelStorageSource.cpp \
    ../../../handheld/src/world/level/storage/RegionFile.cpp \
    ../../../handheld/src/world/level/TickNextTickData.cpp \
    ../../../handheld/src/world/level/tile/BedTile.cpp \
    ../../../handheld/src/world/level/tile/ChestTile.cpp \
    ../../../handheld/src/world/level/tile/CropTile.cpp \
    ../../../handheld/src/world/level/tile/DoorTile.cpp \
    ../../../handheld/src/world/level/tile/EntityTile.cpp \
    ../../../handheld/src/world/level/tile/entity/ChestTileEntity.cpp \
    ../../../handheld/src/world/level/tile/entity/FurnaceTileEntity.cpp \
    ../../../handheld/src/world/level/tile/entity/ModTileEntity.cpp \
    ../../../handheld/src/world/level/tile/entity/NetherReactorTileEntity.cpp \
    ../../../handheld/src/world/level/tile/entity/SignTileEntity.cpp \
    ../../../handheld/src/world/level/tile/entity/TileEntity.cpp \
    ../../../handheld/src/world/level/tile/FurnaceTile.cpp \
    ../../../handheld/src/world/level/tile/GrassTile.cpp \
    ../../../handheld/src/world/level/tile/HeavyTile.cpp \
    ../../../handheld/src/world/level/tile/LightGemTile.cpp \
    ../../../handheld/src/world/level/tile/MelonTile.cpp \
    ../../../handheld/src/world/level/tile/ModTile.cpp \
    ../../../handheld/src/world/level/tile/Mushroom.cpp \
    ../../../handheld/src/world/level/tile/NetherReactor.cpp \
    ../../../handheld/src/world/level/tile/NetherReactorPattern.cpp \
    ../../../handheld/src/world/level/tile/StairTile.cpp \
    ../../../handheld/src/world/level/tile/StemTile.cpp \
    ../../../handheld/src/world/level/tile/StoneSlabTile.cpp \
    ../../../handheld/src/world/level/tile/TallGrass.cpp \
    ../../../handheld/src/world/level/tile/Tile.cpp \
    ../../../handheld/src/world/level/tile/TrapDoorTile.cpp \
    ../../../handheld/src/world/phys/HitResult.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# ── mpe_gui（client/gui 界面）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_gui
LOCAL_SRC_FILES := \
    ../../../handheld/src/client/gui/components/Button.cpp \
    ../../../handheld/src/client/gui/components/GuiElement.cpp \
    ../../../handheld/src/client/gui/components/GuiElementContainer.cpp \
    ../../../handheld/src/client/gui/components/InventoryPane.cpp \
    ../../../handheld/src/client/gui/components/ItemPane.cpp \
    ../../../handheld/src/client/gui/components/LargeImageButton.cpp \
    ../../../handheld/src/client/gui/components/NinePatch.cpp \
    ../../../handheld/src/client/gui/components/OptionsGroup.cpp \
    ../../../handheld/src/client/gui/components/OptionsItem.cpp \
    ../../../handheld/src/client/gui/components/OptionsPane.cpp \
    ../../../handheld/src/client/gui/components/RolledSelectionListH.cpp \
    ../../../handheld/src/client/gui/components/RolledSelectionListV.cpp \
    ../../../handheld/src/client/gui/components/ScrolledSelectionList.cpp \
    ../../../handheld/src/client/gui/components/ScrollingPane.cpp \
    ../../../handheld/src/client/gui/components/Slider.cpp \
    ../../../handheld/src/client/gui/components/SmallButton.cpp \
    ../../../handheld/src/client/gui/components/ImageButton.cpp \
    ../../../handheld/src/client/gui/components/TextBox.cpp \
    ../../../handheld/src/client/gui/Font.cpp \
    ../../../handheld/src/client/gui/Gui.cpp \
    ../../../handheld/src/client/gui/GuiComponent.cpp \
    ../../../handheld/src/client/gui/Screen.cpp \
    ../../../handheld/src/client/gui/screens/ArmorScreen.cpp \
    ../../../handheld/src/client/gui/screens/ChatScreen.cpp \
    ../../../handheld/src/client/gui/screens/ChestScreen.cpp \
    ../../../handheld/src/client/gui/screens/ChooseLevelScreen.cpp \
    ../../../handheld/src/client/gui/screens/ConfirmScreen.cpp \
    ../../../handheld/src/client/gui/screens/crafting/CraftingFilters.cpp \
    ../../../handheld/src/client/gui/screens/crafting/PaneCraftingScreen.cpp \
    ../../../handheld/src/client/gui/screens/crafting/StonecutterScreen.cpp \
    ../../../handheld/src/client/gui/screens/crafting/WorkbenchScreen.cpp \
    ../../../handheld/src/client/gui/screens/DeathScreen.cpp \
    ../../../handheld/src/client/gui/screens/SimpleChooseLevelScreen.cpp \
    ../../../handheld/src/client/gui/screens/FurnaceScreen.cpp \
    ../../../handheld/src/client/gui/screens/InBedScreen.cpp \
    ../../../handheld/src/client/gui/screens/IngameBlockSelectionScreen.cpp \
    ../../../handheld/src/client/gui/screens/JoinGameScreen.cpp \
    ../../../handheld/src/client/gui/screens/AddServerScreen.cpp \
    ../../../handheld/src/client/gui/screens/ChatInputScreen.cpp \
    ../../../handheld/src/client/gui/screens/ModsScreen.cpp \
    ../../../handheld/src/client/gui/screens/OptionsScreen.cpp \
    ../../../handheld/src/client/gui/screens/LanguageScreen.cpp \
    ../../../handheld/src/client/gui/screens/ScriptedScreen.cpp \
    ../../../handheld/src/client/gui/screens/PauseScreen.cpp \
    ../../../handheld/src/client/gui/screens/ProgressScreen.cpp \
    ../../../handheld/src/client/gui/screens/ScreenChooser.cpp \
    ../../../handheld/src/client/gui/screens/SelectWorldScreen.cpp \
    ../../../handheld/src/client/gui/screens/StartMenuScreen.cpp \
    ../../../handheld/src/client/gui/screens/TextEditScreen.cpp \
    ../../../handheld/src/client/gui/screens/touch/TouchIngameBlockSelectionScreen.cpp \
    ../../../handheld/src/client/gui/screens/touch/TouchJoinGameScreen.cpp \
    ../../../handheld/src/client/gui/screens/touch/TouchSelectWorldScreen.cpp \
    ../../../handheld/src/client/gui/screens/touch/TouchStartMenuScreen.cpp \
    ../../../handheld/src/client/gui/screens/UploadPhotoScreen.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# ── mpe_render（client/renderer 渲染）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_render
LOCAL_SRC_FILES := \
    ../../../handheld/src/client/renderer/Chunk.cpp \
    ../../../handheld/src/client/renderer/culling/Frustum.cpp \
    ../../../handheld/src/client/renderer/EntityTileRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/ArrowRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/ModProjectileRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/ChickenRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/EntityRenderDispatcher.cpp \
    ../../../handheld/src/client/renderer/entity/EntityRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/FallingTileRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/HumanoidMobRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/ItemRenderer.cpp \
    ../../../handheld/src/client/renderer/GLBufferPool.cpp \
    ../../../handheld/src/client/renderer/MeshBuffer.cpp \
    ../../../handheld/src/client/renderer/entity/ItemSpriteRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/MobRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/PaintingRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/PlayerRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/SheepRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/ScriptedMobRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/TntRenderer.cpp \
    ../../../handheld/src/client/renderer/entity/TripodCameraRenderer.cpp \
    ../../../handheld/src/client/renderer/GameRenderer.cpp \
    ../../../handheld/src/client/renderer/gles.cpp \
    ../../../handheld/src/client/renderer/ItemInHandRenderer.cpp \
    ../../../handheld/src/client/renderer/LevelRenderer.cpp \
    ../../../handheld/src/client/renderer/ptexture/DynamicTexture.cpp \
    ../../../handheld/src/client/renderer/RenderChunk.cpp \
    ../../../handheld/src/client/renderer/RenderList.cpp \
    ../../../handheld/src/client/renderer/Tesselator.cpp \
    ../../../handheld/src/client/renderer/TextureTesselator.cpp \
    ../../../handheld/src/client/renderer/Textures.cpp \
    ../../../handheld/src/client/renderer/tileentity/ChestRenderer.cpp \
    ../../../handheld/src/client/renderer/tileentity/SignRenderer.cpp \
    ../../../handheld/src/client/renderer/tileentity/TileEntityRenderDispatcher.cpp \
    ../../../handheld/src/client/renderer/tileentity/TileEntityRenderer.cpp \
    ../../../handheld/src/client/renderer/TileRenderer.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# ── mpe_client（client 其余）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_client
LOCAL_SRC_FILES := \
    ../../../handheld/src/client/gamemode/CreatorMode.cpp \
    ../../../handheld/src/client/gamemode/GameMode.cpp \
    ../../../handheld/src/client/gamemode/CreativeMode.cpp \
    ../../../handheld/src/client/gamemode/SurvivalMode.cpp \
    ../../../handheld/src/client/IConfigListener.cpp \
    ../../../handheld/src/client/Minecraft.cpp \
    ../../../handheld/src/client/model/ChickenModel.cpp \
    ../../../handheld/src/client/model/ScriptedModel.cpp \
    ../../../handheld/src/client/model/CowModel.cpp \
    ../../../handheld/src/client/model/geom/Cube.cpp \
    ../../../handheld/src/client/model/geom/ModelPart.cpp \
    ../../../handheld/src/client/model/geom/Polygon.cpp \
    ../../../handheld/src/client/model/HumanoidModel.cpp \
    ../../../handheld/src/client/model/PigModel.cpp \
    ../../../handheld/src/client/model/QuadrupedModel.cpp \
    ../../../handheld/src/client/model/SheepFurModel.cpp \
    ../../../handheld/src/client/model/SheepModel.cpp \
    ../../../handheld/src/client/MouseHandler.cpp \
    ../../../handheld/src/client/Options.cpp \
    ../../../handheld/src/client/OptionsFile.cpp \
    ../../../handheld/src/client/OptionStrings.cpp \
    ../../../handheld/src/client/particle/Particle.cpp \
    ../../../handheld/src/client/particle/ScriptedParticle.cpp \
    ../../../handheld/src/client/particle/ParticleEngine.cpp \
    ../../../handheld/src/client/player/input/KeyboardInput.cpp \
    ../../../handheld/src/client/player/input/touchscreen/TouchScreenInput.cpp \
    ../../../handheld/src/client/player/LocalPlayer.cpp \
    ../../../handheld/src/client/player/RemotePlayer.cpp \
    ../../../handheld/src/client/sound/Sound.cpp \
    ../../../handheld/src/client/sound/SoundEngine.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# ── mpe_gui08（0.8.1 GUI 移植层）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_gui08
LOCAL_SRC_FILES := \
    ../../../handheld/src/gui08/impl_gui/PackedScrollContainer.cpp \
    ../../../handheld/src/gui08/impl/Util081.cpp \
    ../../../handheld/src/gui08/impl/DisableState.cpp \
    ../../../handheld/src/gui08/impl/EnableClientState.cpp \
    ../../../handheld/src/gui08/impl/Color4.cpp \
    ../../../handheld/src/gui08/impl_gui/ScreenId.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/CreativeInventoryScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/buttons/ImageWithBackground.cpp \
    ../../../handheld/src/gui08/impl_gui/buttons/Spinner.cpp \
    ../../../handheld/src/gui08/impl_gui/buttons/BuyButton.cpp \
    ../../../handheld/src/gui08/impl_gui/buttons/CategoryButton.cpp \
    ../../../handheld/src/gui08/impl_gui/elements/Label.cpp \
    ../../../handheld/src/gui08/impl_gui/elements/TextBox081.cpp \
    ../../../handheld/src/gui08/impl_gui/elements/ScrollBar.cpp \
    ../../../handheld/src/gui08/impl_gui/elements/WorldSelectionList.cpp \
    ../../../handheld/src/gui08/impl_gui/elements/Touch_TouchWorldSelectionList.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/DeleteWorldScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/Touch_DeleteWorldScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/PlayScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/SelectWorldScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/PauseScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/CreateWorldScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/AddExternalServerScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/RenameMPLevelScreen.cpp \
    ../../../handheld/src/gui08/impl_gui/elements/LocalServerListItemElement.cpp \
    ../../../handheld/src/gui08/impl_gui/elements/ModListItemElement.cpp \
    ../../../handheld/src/gui08/impl/ExternalServerFile.cpp \
    ../../../handheld/src/gui08/impl/ExternalServer.cpp \
    ../../../handheld/src/gui08/impl/network/mco/MCOServerListItem.cpp \
    ../../../handheld/src/gui08/impl/network/mco/MojangConnector.cpp \
    ../../../handheld/src/gui08/impl/network/mco/RestRequestJob.cpp \
    ../../../handheld/src/gui08/impl/network/mco/MCOParser.cpp \
    ../../../handheld/src/gui08/impl/network/mco/LoginInformation.cpp \
    ../../../handheld/src/gui08/impl_gui/screens/PlayScreenStateSetting.cpp \
    ../../../handheld/src/gui08/impl/network/RestService.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# ── 最终 shared library ──────────────────────────────────────────────────
include $(CLEAR_VARS)

LOCAL_MODULE := minecraftpe
LOCAL_SRC_FILES :=     ../../../handheld/src/SharedConstants.cpp

LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)

LOCAL_WHOLE_STATIC_LIBRARIES := mpe_png mpe_core mpe_raknet mpe_world mpe_gui mpe_render mpe_client mpe_gui08

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv1_CM -lOpenSLES -lz -lm

include $(BUILD_SHARED_LIBRARY)
