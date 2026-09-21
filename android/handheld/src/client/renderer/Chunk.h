#ifndef NET_MINECRAFT_CLIENT_RENDERER__Chunk_H__
#define NET_MINECRAFT_CLIENT_RENDERER__Chunk_H__

//package net.minecraft.client.renderer;

#include "RenderChunk.h"
#include "../../world/phys/AABB.h"

#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
#include <vector>
#include <cstdint>
#include <atomic>
#endif

class Level;
class Entity;
class Culler;
class Tesselator;

// @note: TileEntity stuff is stripped away
class Chunk
{
    // 0=不透明 1=alpha测试 2=混合(水) 3=植被(草/花/苗/蔗,渲染线程做随风摇摆)
    static const int NumLayers = 4;
public:
    Chunk(Level* level_, int x, int y, int z, int size, int lists_, GLuint* ptrBuf = NULL);

    void setPos(int x, int y, int z);

	void rebuild();
#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
	// Background-thread tessellation step: fills _cpuMesh[]/cpuVertexCount[]
	// using the provided Tesselator (not the global singleton).
	void rebuildCPU(Tesselator& t);
	// Main-thread GPU upload step: uploads _cpuMesh[] to OpenGL VBOs.
	void uploadGPU();
#endif
	void setDirty();
	void setClean();
	bool isDirty();
	void reset();

    float distanceToSqr(const Entity* player) const;
    float squishedDistanceToSqr(const Entity* player) const;

	//@todo @fix
    int getAllLists(int displayLists[], int p, int layer);
	int getList(int layer);

	RenderChunk& getRenderChunk(int layer);

	bool isEmpty();
    void cull(Culler* culler);

#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
	// 植被摆动(layer3): 渲染线程把基网格顶点按正弦偏移后重传 VBO。
	// 仅距相机 sway 半径内的 chunk 动画(每 chunk 一次 glBufferData)。
	bool hasSway() const { return _swayCount > 0 && !_swayBase.empty(); }
	static void setSwayGlobals(float amp, float speed);   // 全局摇摆: amp=0 关(默认), 模组 Render.setSway 开/调速
	void swayUpdate(float timeSec, float camX, float camZ);
	// 逐方块 sway 幅度: 与该 chunk 层3 CPU 基网格顶点一一对应(rebuild 时
	// 按 tile 记录),swayUpdate 只偏移幅度>0 的顶点 — 实现"指定某方块摇"。
	bool hasSwayAmp() const { return !_swayAmp.empty(); }
	const std::vector<float>& swayAmp() const { return _swayAmp; }
#endif

    void renderBB();
	static void resetUpdates();

private:
	void translateToPos();
	void rebuildImpl(Tesselator& t, bool cpuOnly);
public:
	Level* level;

	static int updates;// = 0;

	int x, y, z, xs, ys, zs;
	bool empty[NumLayers];
	int xm, ym, zm;
	float radius;
	AABB bb;

	int id;
	bool visible;
	bool occlusion_visible;
	bool occlusion_querying;
	int occlusion_id;
	bool queued; // true while this chunk is in LevelRenderer::dirtyChunks
	bool skyLit;

	RenderChunk renderChunk[NumLayers];

#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
	// CPU-side mesh buffers written by rebuildCPU(), consumed by uploadGPU().
	std::vector<uint8_t> _cpuMesh[NumLayers];
	int _cpuVertexCount[NumLayers];
	// 植被(layer3)CPU 基网格: rebuildCPU 后常驻,供 swayUpdate 每帧做正弦摆动。
	std::vector<uint8_t> _swayBase;
	std::vector<uint8_t> _swayWork;
	int _swayCount;
	bool _swayDirty;
	// 逐顶点 sway 幅度(与 _swayBase 顶点一一对应,rebuild 时按 tile 记录)。
	// 值为该顶点所属方块的显式幅度(默认 0 = 不摇);空 = 该层无摇摆参与。
	std::vector<float> _swayAmp;
	// 该 chunk 动画层是否存在幅度>0 的顶点(swayUpdate 快速跳过全静止 chunk)。
	bool _hasAnySway;
	// M2: per-layer normal VBOs (layer0/1), built offline in uploadGPU().
	std::vector<GLuint> _nBuf;
	// Set to true by rebuildCPU() when data is ready, cleared by uploadGPU().
	std::atomic<bool> _cpuMeshReady;
	// True while rebuildCPU() or uploadGPU() are executing (prevents double-queue).
	std::atomic<bool> _inFlight;
#endif

private:
	Tesselator& t;
	int lists;
	GLuint* vboBuffers;
	bool compiled;
	bool dirty;
    bool _empty;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__Chunk_H__*/
