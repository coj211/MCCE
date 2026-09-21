#ifndef NET_MINECRAFT_CLIENT_RENDERER__LevelRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER__LevelRenderer_H__

//package net.minecraft.client.renderer;

#include "../../world/level/LevelListener.h"
#include "../../world/phys/Vec3.h"
#include "RenderList.h"
#include "gles.h"
#include "MeshBuffer.h"
#include <vector>


class Minecraft;
class Textures;
class Culler;
class Chunk;
class TileRenderer;
class Level;
class Mob;
class Player;
class HitResult;
class AABB;
class TripodCamera;

class LevelRenderer: public LevelListener
{
public:
    static const int CHUNK_SIZE;
    static const int MAX_VISIBLE_REBUILDS_PER_FRAME = 3;
    static const int MAX_INVISIBLE_REBUILDS_PER_FRAME = 1;
    
    float xOld;
    float yOld;
    float zOld;
    float destroyProgress;

	LevelRenderer(Minecraft* mc);
	~LevelRenderer();

	void setLevel(Level* level);
	void allChanged();

	int  render(Mob* player, int layer, float alpha);
	void renderDebug(const AABB& b, float a) const;

	// Scripted (arbitrary-camera) world renders: while set, chunk rendering
	// translates relative to this origin instead of mc->cameraTargetPlayer.
	void setScriptedCameraOrigin(float x, float y, float z);
	void clearScriptedCameraOrigin();
	// Scripted-camera renders ignore the player-view occlusion queries.
	void forceOcclusionVisibleAll();

	void renderSky(float alpha);
	void renderClouds(float alpha);
	void renderEntities(Vec3 cam, Culler* culler, float a);
	// 模组动画方块：几何每帧按当前姿态重画（不进区块网格）。
	void renderAnimatedBlocks(float a);
    void renderSameAsLast(int layer, float alpha);
	void renderHit(Player* player, const HitResult& h, int mode, /*ItemInstance*/void* inventoryItem, float a);
    void renderHitOutline(Player* player, const HitResult& h, int mode, /*ItemInstance*/void* inventoryItem, float a);
	void renderHitSelect(Player* player, const HitResult& h, int mode, /*ItemInstance*/void* inventoryItem, float a);
	void entityAdded(Entity* entity);

	void tick();
	bool updateDirtyChunks(Mob* player, bool force);
	void setDirty(int x0, int y0, int z0, int x1, int y1, int z1);
    void tileChanged(int x, int y, int z);
    void setTilesDirty(int x0, int y0, int z0, int x1, int y1, int z1);
	void cull(Culler* culler, float a);
    void skyColorChanged();

	//void addParticle(ParticleType::Id name, float x, float y, float z, float xa, float ya, float za, int data);
	void addParticle(const std::string& name, float x, float y, float z, float xa, float ya, float za, int data);
	void playSound(const std::string& name, float x, float y, float z, float volume, float pitch);
	void takePicture(TripodCamera* cam, Entity* entity);

	void levelEvent(Player* source, int type, int x, int y, int z, int data);

	std::string gatherStats1();

	void render(const AABB& b) const;
	void onGraphicsReset();
private:
	void generateSky();
	// 1.6.4 天空移植（Multiplatform-1.6.4_classic / modifiedeight + 1.6.4 原版
	// RenderGlobal.renderSky）：
	// 星空网格（一次性生成）
	void buildStarsMesh();
	void renderStars(float alpha);
	// 日出/日落扇形（1.6.4 的 calcSunriseSunsetColors 那段）
	void renderSunriseFan(float alpha);
	// 太阳 + 月亮（1.6.4：同一个坐标系里 y=+100 / y=-100）
	void renderSunOrMoon(float alpha);

	int  renderChunks(int from, int to, int layer, float alpha);
	void resortChunks(int xc, int yc, int zc);
	void deleteChunks();
	//void checkQueryResults(int from, int to);
	__inline int getLinearCoord(int x, int y, int z) {
		return (z * yChunks + y) * xChunks + x;
	}
	int noEntityRenderFrames;
    int totalEntities;
    int renderedEntities;
    int culledEntities;

	std::vector<Chunk*> _renderChunks;

	bool _scriptCam;
	float _scX, _scY, _scZ;

    int cullStep;
	//static const int renderListsLength = 4;
    RenderList renderList;//[renderListsLength];

	int totalChunks, offscreenChunks, occludedChunks, renderedChunks, emptyChunks;
    int chunkFixOffs;

    int xMinChunk, yMinChunk, zMinChunk;
    int xMaxChunk, yMaxChunk, zMaxChunk;

	Level* level;
	std::vector<Chunk*> dirtyChunks;

	Chunk** chunks;
    Chunk** sortedChunks;
	int chunksLength;
public:
	TileRenderer* tileRenderer;
private:
	int xChunks, yChunks, zChunks;
    int chunkLists;
    Minecraft* mc;

	bool occlusionCheck;
	int lastViewDistance;

	int ticks;
    int starList, skyList, darkList;

	int numListsOrBuffers;
	unsigned int* chunkBuffers;

	// 1.6.4 天空：天幕/星空/云各自一个静态网格（照抄 modifiedeight 的
	// skyMesh / starsMesh / cloudsMesh）。
	MeshBuffer skyMesh;
	MeshBuffer starsMesh;
	MeshBuffer cloudsMesh;
	// 云网格的缓存键：格子坐标 + 云颜色（变了才重新生成）。
	int cloudMeshTexX;
	int cloudMeshTexY;
	float cloudMeshColorR, cloudMeshColorG, cloudMeshColorB, cloudMeshColorA;

//    /*public*/ std::vector<TileEntity*> renderableTileEntities;
    Textures* textures;
//    /*private*/ TileRenderer tileRenderer;
//    /*private*/ IntBuffer occlusionCheckIds;

};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__LevelRenderer_H__*/
