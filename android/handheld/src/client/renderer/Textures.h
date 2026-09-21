#ifndef NET_MINECRAFT_CLIENT_RENDERER__Textures_H__
#define NET_MINECRAFT_CLIENT_RENDERER__Textures_H__

//package net.minecraft.client.renderer;

#include <string>
#include <map>
#include <utility>
#include "gles.h"
#include "TextureData.h"

class DynamicTexture;
class Options;
class AppPlatform;

typedef GLuint TextureId;
typedef std::map<std::string, TextureId> TextureMap;
typedef std::map<TextureId, TextureData> TextureImageMap;

//@todo: Should probably delete the data buffers with image data
//       after we've created an OpenGL-texture, and rewrite the
//       getTemporaryTextureData() to actually load from file IF
//       it's only read ~once anyway.
class Textures
{
public:
    Textures(Options* options_,  AppPlatform* platform_);
	~Textures();

	void addDynamicTexture(DynamicTexture* dynamicTexture);

	__inline void bind(TextureId id) {
		if (id != Textures::InvalidId) {
			glBindTexture2(GL_TEXTURE_2D, id);
			lastBoundTexture = id;
			++textureChanges;
		} else {
            LOGI("invalidId!\n");
        }
	}
	TextureId loadTexture(const std::string& resourceName, bool inTextureFolder = true);
	TextureId loadAndBindTexture(const std::string& resourceName);
	// Drop a cached texture so the next loadTexture re-reads the file
	// (used to revert resource-pack style overrides).
	void unloadTexture(const std::string& resourceName);
	// 09 · UI 覆盖：把某张已加载贴图的 CPU 侧像素缓存替换成 mod 提供的
	// RGBA（尺寸可不同）。这样 loadAndGetTextureData 返回的 w/h 与内容都
	// 是替换后的——主菜单标题等按像素尺寸布局的绘制点不会被旧缓存带歪。
	// GL 显存内容由调用方用 glTexSubImage2D 同步；本函数只改缓存。
	// 返回 false = 该资源名未加载（无法替换缓存）。
	bool replaceTextureData(const std::string& resourceName, int w, int h, const unsigned char* rgba);

    TextureId assignTexture(const std::string& resourceName, const TextureData& img);
	const TextureData* getTemporaryTextureData(TextureId id);
	// 0.8.1 GUI 移植：加载贴图并返回其数据指针（无则返回 NULL）
	TextureData* loadAndGetTextureData(const std::string& resourceName);

	void tick(bool uploadToGraphicsCard);

	void clear();
	void reloadAll();

	__inline static bool isTextureIdValid(TextureId t) { return t != Textures::InvalidId; }

private:
	int smoothBlend(int c0, int c1);
	int crispBlend(int c0, int c1);

public:
	static bool MIPMAP;
	static int textureChanges;
	static const TextureId InvalidId = -1;

private:
	TextureMap idMap;
	TextureImageMap loadedImages;

	Options* options;
	AppPlatform* platform;

	bool clamp;
	bool blur;

	int lastBoundTexture;
	std::vector<DynamicTexture*> dynamicTextures;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__Textures_H__*/
