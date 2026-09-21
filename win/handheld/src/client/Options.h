#ifndef NET_MINECRAFT_CLIENT__Options_H__
#define NET_MINECRAFT_CLIENT__Options_H__

//package net.minecraft.client;

//#include "locale/Language.h"

#include <string>
#include <cstdio>
#include "KeyMapping.h"
#include "../platform/input/Keyboard.h"
#include "../util/StringUtils.h"
#include "OptionsFile.h"
#include "../locale/I18n.h"

class Minecraft;
typedef std::vector<std::string> StringVector;

class Options
{
public:
	// 0.8.1 背包移植：全局单例（main 里创建后赋值，_singleton = &minecraft->options）
	static Options* instance;

    class Option
	{
        const bool _isProgress;
        const bool _isBoolean;
        const std::string _captionId;
		const int _ordinal;

	public:
		static const Option MUSIC;
		static const Option SOUND;
		static const Option INVERT_MOUSE;
		static const Option SENSITIVITY;
		static const Option RENDER_DISTANCE;
		static const Option VIEW_BOBBING;
		static const Option ANAGLYPH;
		static const Option LIMIT_FRAMERATE;
		static const Option DIFFICULTY;
		static const Option GRAPHICS;
		static const Option AMBIENT_OCCLUSION;
		static const Option GUI_SCALE;
        
		static const Option THIRD_PERSON;
		static const Option HIDE_GUI;
		static const Option SERVER_VISIBLE;
		static const Option LEFT_HANDED;
		static const Option USE_TOUCHSCREEN;
		static const Option USE_TOUCH_JOYPAD;
		static const Option DESTROY_VIBRATION;
		static const Option LANGUAGE;

		static const Option PIXELS_PER_MILLIMETER;
		static const Option FOV;
		// 0.8.1 GUI 移植：设置页新增选项
		static const Option NAME;
		static const Option SHOW_COORDINATES;
		static const Option DEBUG_SCREEN;
		static const Option HUD_CAMERA_BUTTON;
		static const Option SPRINT;
		static const Option AUTO_JUMP;
		static const Option SWAP_JUMP_AND_SNEAK;
		static const Option BRIGHTNESS;
		static const Option FOG_ENABLED;
		static const Option FANCY_SKIES;
		static const Option CLASSIC_TEXTURES;
		static const Option ANIMATE_TEXTURES;
		static const Option ANIMATE_WATER;
		static const Option ANIMATE_LAVA;
		static const Option ANIMATE_FIRE;
		static const Option SMOOTH_CHUNKS;
		static const Option LOD_CHUNKS;
		static const Option MARKETPLACE;
		static const Option SHOW_FPS;
		static const Option DISCORD_RPC;
		static const Option PANORAMA_ANGLE;
		static const Option CHAT_COLOR;
		static const Option CHAT_BG_COLOR;
		static const Option CLASSIC_BACKGROUND;
		static const Option CLASSIC_GUI;
		static const Option NEON_COLOR_THEME;
		static const Option NEW_ADDITIONS;

		/*
        static Option* getItem(int id) {
            for (Option item : Option.values()) {
                if (item.getId() == id) {
                    return item;
                }
            }
            return NULL;
        }
		*/

        Option(int ordinal, const std::string& captionId, bool hasProgress, bool isBoolean)
		:	_captionId(captionId),
			_isProgress(hasProgress),
			_isBoolean(isBoolean),
			_ordinal(ordinal)
		{}

        bool isProgress() const {
            return _isProgress;
        }

        bool isBoolean() const {
            return _isBoolean;
        }

		bool isInt() const {
			return (!_isBoolean && !_isProgress);
		}

        int getId() {
            return _ordinal;
        }

        std::string getCaptionId() const {
            return _captionId;
        }
    };

private:
	static const float SOUND_MIN_VALUE;
	static const float SOUND_MAX_VALUE;
	static const float MUSIC_MIN_VALUE;
	static const float MUSIC_MAX_VALUE;
	static const float SENSITIVITY_MIN_VALUE;
	static const float SENSITIVITY_MAX_VALUE;
	static const float PIXELS_PER_MILLIMETER_MIN_VALUE;
	static const float PIXELS_PER_MILLIMETER_MAX_VALUE;
	static const float FOV_MIN_VALUE;
	static const float FOV_MAX_VALUE;
	static const int DIFFICULY_LEVELS[];
public:
    static const char* RENDER_DISTANCE_NAMES[];
    static const char* DIFFICULTY_NAMES[];
    static const char* GUI_SCALE[];
	static bool debugGl;

	float music;
    float sound;
    float sensitivity;
    bool invertYMouse;
    int viewDistance;
    bool bobView;
    bool anaglyph3d;
    bool limitFramerate;
    bool fancyGraphics;
    bool ambientOcclusion;
	// 0.8.1 GUI 移植：经典 GUI 开关（false=霓虹风格）与霓虹主题
	bool classicGUI;
	int neonColorTheme;
	// 0.8.1 GUI 移植：经典背景（true=panorama 全景 3D 背景；false=霓虹网格）
	bool classicBackground;
	bool useMouseForDigging;
	bool isLeftHanded;
	bool destroyVibration;
    //std::string skin;

    KeyMapping keyUp;
    KeyMapping keyLeft;
    KeyMapping keyDown;
    KeyMapping keyRight;
    KeyMapping keyJump;
    KeyMapping keyBuild;
    KeyMapping keyDrop;
    KeyMapping keyChat;
    KeyMapping keyFog;
    KeyMapping keySneak;
	KeyMapping keyCraft;
	KeyMapping keyDestroy;
	KeyMapping keyUse;

	KeyMapping keyMenuNext;
	KeyMapping keyMenuPrevious;
	KeyMapping keyMenuOk;
	KeyMapping keyMenuCancel;

	// 0.8.1 GUI 移植：霓虹主题配色（classicGUI=false 时生效）
	void getNeonColors(int& colorA, int& colorB);

    KeyMapping* keyMappings[16];

	/*protected*/ Minecraft* minecraft;
    ///*private*/ File optionsFile;

    int difficulty;
    bool hideGui;
    bool thirdPersonView;
    bool renderDebug;

    bool isFlying;
    bool smoothCamera;
    bool fixedCamera;
    float flySpeed;
    float cameraSpeed;
    int guiScale;
	std::string username;
	std::string language;

	bool serverVisible;
	bool isJoyTouchArea;
	bool useTouchScreen;
	float pixelsPerMillimeter;
	float fieldOfView;
	// 0.8.1 GUI 移植：设置页新增字段
	bool showCoordinates;
	bool debugScreen;
	bool hudCameraButton;
	bool sprintEnabled;
	bool autoJump;
	bool swapJumpAndSneak;
	float brightness;
	bool fogEnabled;
	bool fancySkies;
	bool classicTextures;
	bool animateTextures;
	bool animateWater;
	bool animateLava;
	bool animateFire;
	bool smoothChunks;
	bool lodChunks;
	bool marketplace;
	bool showFps;
	bool discordIntegration;
	int panoramaAngle;
	int chatColor;
	int chatBgColor;
	int newAdditions;
	// 0.8.1 背包移植：左手模式（creative 背包 tab 位置）
	bool leftHanded;
    Options(Minecraft* minecraft, const std::string& workingDirectory)
	:	minecraft(minecraft)
	{
        //optionsFile = /*new*/ File(workingDirectory, "options.txt");
        initDefaultValues();

		load();
    }

	Options()
	:	minecraft(NULL)
	{
		
	}

	void initDefaultValues();

    std::string getKeyDescription(int i) {
        //Language language = Language.getInstance();
        //return language.getElement(keyMappings[i].name);
		return "Options::getKeyDescription not implemented";
    }

    std::string getKeyMessage(int i) {
        //return Keyboard.getKeyName(keyMappings[i].key);
		return "Options::getKeyMessage not implemented";
    }

    void setKey(int i, int key) {
        keyMappings[i]->key = key;
        save();
    }

    void set(const Option* item, float value) {
        if (item == &Option::MUSIC) {
            music = value;
            //minecraft.soundEngine.updateOptions();
        } else if (item == &Option::SOUND) {
            sound = value;
            //minecraft.soundEngine.updateOptions();
        } else if (item == &Option::SENSITIVITY) {
            sensitivity = value;
		} else if (item == &Option::PIXELS_PER_MILLIMETER) {
			 pixelsPerMillimeter = value;
		} else if (item == &Option::FOV) {
			fieldOfView = value;
		} else if (item == &Option::BRIGHTNESS) {
			brightness = value;
		}
		notifyOptionUpdate(item, value);
		save();
    }
	void set(const Option* item, int value) {
		if(item == &Option::DIFFICULTY) {
			difficulty = value;
			if (difficulty != DIFFICULY_LEVELS[0] && difficulty != DIFFICULY_LEVELS[1]) {
				difficulty = DIFFICULY_LEVELS[1];
			}
		} else if (item == &Option::RENDER_DISTANCE) {
			viewDistance = value & 7;
		} else if (item == &Option::GUI_SCALE) {
			guiScale = value & 3;
		} else if (item == &Option::GRAPHICS) {
			fancyGraphics = value != 0;
		} else if (item == &Option::LANGUAGE) {
			setLanguageValue(value & 1);
		} else if (item == &Option::PANORAMA_ANGLE) {
			panoramaAngle = value;
		} else if (item == &Option::CHAT_COLOR) {
			chatColor = value;
		} else if (item == &Option::CHAT_BG_COLOR) {
			chatBgColor = value;
		} else if (item == &Option::NEON_COLOR_THEME) {
			neonColorTheme = value;
		}
		notifyOptionUpdate(item, value);
		save();
	}

	// 0.8.1 GUI 移植：NAME 输入框支持
	void set(const Option* item, std::string value) {
		if (item == &Option::NAME) {
			username = value;
			// 允许删空（用户删除过程中不应被强制弹回默认名），
			// 仅当输入框失焦/保存时由上层兜底非空。
		}
		save();
	}
	std::string getStringValue(const Option* item) {
		if (item == &Option::NAME) return username;
		return "";
	}
	bool canModify(const Option* item) {
		(void)item;
		return true;
	}
    void toggle(const Option* option, int dir) {
        if (option == &Option::INVERT_MOUSE)	invertYMouse = !invertYMouse;
        if (option == &Option::RENDER_DISTANCE) viewDistance = (viewDistance + dir) & 7;
        if (option == &Option::GUI_SCALE)		guiScale = (guiScale + dir) & 3;
        if (option == &Option::VIEW_BOBBING)	bobView = !bobView;
		if (option == &Option::THIRD_PERSON)	thirdPersonView = !thirdPersonView;
		if (option == &Option::HIDE_GUI)		hideGui = !hideGui;
		if (option == &Option::SERVER_VISIBLE)		serverVisible = !serverVisible;
		if (option == &Option::LEFT_HANDED) isLeftHanded = !isLeftHanded;
		if (option == &Option::USE_TOUCHSCREEN) useTouchScreen = !useTouchScreen;
		if (option == &Option::USE_TOUCH_JOYPAD) isJoyTouchArea = !isJoyTouchArea;
		if (option == &Option::DESTROY_VIBRATION) destroyVibration = !destroyVibration;
		// 0.8.1 GUI 移植：新增选项切换
		if (option == &Option::SHOW_COORDINATES) showCoordinates = !showCoordinates;
		if (option == &Option::DEBUG_SCREEN) debugScreen = !debugScreen;
		if (option == &Option::HUD_CAMERA_BUTTON) hudCameraButton = !hudCameraButton;
		if (option == &Option::SPRINT) sprintEnabled = !sprintEnabled;
		if (option == &Option::AUTO_JUMP) autoJump = !autoJump;
		if (option == &Option::SWAP_JUMP_AND_SNEAK) swapJumpAndSneak = !swapJumpAndSneak;
		if (option == &Option::FOG_ENABLED) fogEnabled = !fogEnabled;
		if (option == &Option::FANCY_SKIES) fancySkies = !fancySkies;
		if (option == &Option::CLASSIC_TEXTURES) classicTextures = !classicTextures;
		if (option == &Option::ANIMATE_TEXTURES) animateTextures = !animateTextures;
		if (option == &Option::ANIMATE_WATER) animateWater = !animateWater;
		if (option == &Option::ANIMATE_LAVA) animateLava = !animateLava;
		if (option == &Option::ANIMATE_FIRE) animateFire = !animateFire;
		if (option == &Option::SMOOTH_CHUNKS) smoothChunks = !smoothChunks;
		if (option == &Option::LOD_CHUNKS) lodChunks = !lodChunks;
		if (option == &Option::MARKETPLACE) marketplace = !marketplace;
		if (option == &Option::SHOW_FPS) showFps = !showFps;
		if (option == &Option::DISCORD_RPC) discordIntegration = !discordIntegration;
		if (option == &Option::CLASSIC_BACKGROUND) classicBackground = !classicBackground;
		if (option == &Option::CLASSIC_GUI) classicGUI = !classicGUI;
		if (option == &Option::LANGUAGE) {
			toggleLanguage();
		}
		if (option == &Option::ANAGLYPH) {
            anaglyph3d = !anaglyph3d;
            //minecraft->textures.reloadAll();
        }
        if (option == &Option::LIMIT_FRAMERATE) limitFramerate = !limitFramerate;
        if (option == &Option::DIFFICULTY) {
			const int difficultyLevelsNum = 2;
			int difficultyIndex = 0;
			for (int i = 0; i < difficultyLevelsNum; ++i) {
				if (DIFFICULY_LEVELS[i] == difficulty) {
					difficultyIndex = i;
					break;
				}
			}
			difficultyIndex += dir;
			while (difficultyIndex < 0) difficultyIndex += difficultyLevelsNum;
			difficulty = DIFFICULY_LEVELS[difficultyIndex % difficultyLevelsNum];
		}
        if (option == &Option::GRAPHICS) {
            fancyGraphics = !fancyGraphics;
            //minecraft->levelRenderer.allChanged();
        }
        if (option == &Option::AMBIENT_OCCLUSION) {
            ambientOcclusion = !ambientOcclusion;
            syncAmbientOcclusion();
        }
		if (option->isBoolean()) {
			notifyOptionUpdate(option, getBooleanValue(option));
		} else {
			notifyOptionUpdate(option, getIntValue(option));
		}
        save();
    }

	int getIntValue(const Option* item) {
		if(item == &Option::DIFFICULTY) return difficulty;
		if(item == &Option::RENDER_DISTANCE) return viewDistance;
		if(item == &Option::GUI_SCALE) return guiScale;
		if(item == &Option::GRAPHICS) return fancyGraphics ? 1 : 0;
		if(item == &Option::LANGUAGE) return (language == "zh_CN") ? 1 : 0;
		if(item == &Option::PANORAMA_ANGLE) return panoramaAngle;
		if(item == &Option::CHAT_COLOR) return chatColor;
		if(item == &Option::CHAT_BG_COLOR) return chatBgColor;
		if(item == &Option::NEON_COLOR_THEME) return neonColorTheme;
		if(item == &Option::NEW_ADDITIONS) return newAdditions;
		return 0;
	}

    float getProgressValue(const Option* item) {
        if (item == &Option::MUSIC) return music;
        if (item == &Option::SOUND) return sound;
        if (item == &Option::SENSITIVITY) return sensitivity;
		if (item == &Option::PIXELS_PER_MILLIMETER) return pixelsPerMillimeter;
		if (item == &Option::FOV) return fieldOfView;
		if (item == &Option::BRIGHTNESS) return brightness;
        return 0;
    }

    bool getBooleanValue(const Option* item) {
        if (item == &Option::INVERT_MOUSE)
            return invertYMouse;
        if (item == &Option::VIEW_BOBBING)
            return bobView;
        if (item == &Option::ANAGLYPH)
            return anaglyph3d;
        if (item == &Option::LIMIT_FRAMERATE)
            return limitFramerate;
        if (item == &Option::AMBIENT_OCCLUSION)
            return ambientOcclusion;
        if (item == &Option::THIRD_PERSON)
            return thirdPersonView;
        if (item == &Option::HIDE_GUI)
            return hideGui;
		if (item == &Option::SERVER_VISIBLE)
			return serverVisible;
		if (item == &Option::LEFT_HANDED)
			return isLeftHanded;
		if (item == &Option::USE_TOUCHSCREEN)
			return useTouchScreen;
		if (item == &Option::USE_TOUCH_JOYPAD)
			return isJoyTouchArea;
		if (item == &Option::DESTROY_VIBRATION)
			return destroyVibration;
		if (item == &Option::SHOW_COORDINATES) return showCoordinates;
		if (item == &Option::DEBUG_SCREEN) return debugScreen;
		if (item == &Option::HUD_CAMERA_BUTTON) return hudCameraButton;
		if (item == &Option::SPRINT) return sprintEnabled;
		if (item == &Option::AUTO_JUMP) return autoJump;
		if (item == &Option::SWAP_JUMP_AND_SNEAK) return swapJumpAndSneak;
		if (item == &Option::FOG_ENABLED) return fogEnabled;
		if (item == &Option::FANCY_SKIES) return fancySkies;
		if (item == &Option::CLASSIC_TEXTURES) return classicTextures;
		if (item == &Option::ANIMATE_TEXTURES) return animateTextures;
		if (item == &Option::ANIMATE_WATER) return animateWater;
		if (item == &Option::ANIMATE_LAVA) return animateLava;
		if (item == &Option::ANIMATE_FIRE) return animateFire;
		if (item == &Option::SMOOTH_CHUNKS) return smoothChunks;
		if (item == &Option::LOD_CHUNKS) return lodChunks;
		if (item == &Option::MARKETPLACE) return marketplace;
		if (item == &Option::SHOW_FPS) return showFps;
		if (item == &Option::DISCORD_RPC) return discordIntegration;
		if (item == &Option::CLASSIC_BACKGROUND) return classicBackground;
		if (item == &Option::CLASSIC_GUI) return classicGUI;
		return false;
	}

	float getProgrssMin(const Option* item) {
		if (item == &Option::MUSIC) return MUSIC_MIN_VALUE;
		if (item == &Option::SOUND) return SOUND_MIN_VALUE;
		if (item == &Option::SENSITIVITY) return SENSITIVITY_MIN_VALUE;
		if (item == &Option::PIXELS_PER_MILLIMETER) return PIXELS_PER_MILLIMETER_MIN_VALUE;
		if (item == &Option::FOV) return FOV_MIN_VALUE;
		return 0;
	}

	float getProgrssMax(const Option* item) {
		if (item == &Option::MUSIC) return MUSIC_MAX_VALUE;
		if (item == &Option::SOUND) return SOUND_MAX_VALUE;
		if (item == &Option::SENSITIVITY) return SENSITIVITY_MAX_VALUE;
		if (item == &Option::PIXELS_PER_MILLIMETER) return PIXELS_PER_MILLIMETER_MAX_VALUE;
		if (item == &Option::FOV) return FOV_MAX_VALUE;
		return 1.0f;
	}

	std::string getMessage(const Option* item);

	void setSettingsPath(const std::string& path);
	void update();
    void load();
    void save();
    void syncAmbientOcclusion();
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, bool boolValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, float floatValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, int intValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, const std::string& value);
	void notifyOptionUpdate(const Option* option, bool value);
	void notifyOptionUpdate(const Option* option, float value);
	void notifyOptionUpdate(const Option* option, int value);
	void setLanguageValue(int idx);
	void toggleLanguage();
private:
    static bool readFloat(const std::string& string, float& value);
    static bool readInt(const std::string& string, int& value);
	static bool readBool(const std::string& string, bool& value);
	static bool readString(const std::string& string, std::string& value);

private:
	OptionsFile optionsFile;
	
};

#endif /*NET_MINECRAFT_CLIENT__Options_H__*/
