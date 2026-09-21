#include "Chunk.h"
#include "Tesselator.h"
#include "TileRenderer.h"
#include "culling/Culler.h"
#include "VertecDecl.h"
#include <cmath>
#include "../../world/entity/Entity.h"
#include "../../world/level/tile/Tile.h"
#include "../../world/level/tile/ModTile.h"
#include "../../mod/ModEngine.h"
#include "../../world/level/Region.h"
#include "../../world/level/chunk/LevelChunk.h"
#include "../../util/Mth.h"
#include "../../util/FrameProf.h"
//#include "../../platform/time.h"

// 植被摇摆全局参数: 默认关闭(amp=0 → 原版画面不摇), 由光影模组经 Render.setSway 开启并调幅/调速。
static float s_swayAmp = 0.0f;
static float s_swaySpeed = 1.0f;

/*static*/ int Chunk::updates = 0;
//static Stopwatch swRebuild;
//int* _layerChunks[3] = {0, 0, 0}; //Chunk::NumLayers];
//int _layerChunkCount[3] = {0, 0, 0};

Chunk::Chunk( Level* level_, int x, int y, int z, int size, int lists_, GLuint* ptrBuf/*= NULL*/)
:	level(level_),
	visible(false),
	compiled(false),
    _empty(true),
	xs(size), ys(size), zs(size),
	dirty(false),
	occlusion_visible(true),
	occlusion_querying(false),
	queued(false),
	lists(lists_),
	vboBuffers(ptrBuf),
	bb(0,0,0,1,1,1),
	t(Tesselator::instance)
	, _swayCount(0), _swayDirty(false), _hasAnySway(false)
#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
	, _cpuMeshReady(false)
	, _inFlight(false)
#endif
{
#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
	for (int i = 0; i < NumLayers; ++i) _cpuVertexCount[i] = 0;
#endif
	for (int l = 0; l < NumLayers; l++) {
		empty[l] = false;
	}

	radius = Mth::sqrt((float)(xs * xs + ys * ys + zs * zs)) * 0.5f;

	this->x = -999;
	setPos(x, y, z);
}

void Chunk::setPos( int x, int y, int z )
{
	if (x == this->x && y == this->y && z == this->z) return;

	reset();
	this->x = x;
	this->y = y;
	this->z = z;
	xm = x + xs / 2;
	ym = y + ys / 2;
	zm = z + zs / 2;

	const float xzg = 1.0f;
	const float yp = 2.0f;
	const float yn = 0.0f;
	bb.set(x-xzg, y-yn, z-xzg, x + xs+xzg, y + ys+yp, z + zs+xzg);

	//glNewList(lists + 2, GL_COMPILE);
	//ItemRenderer.renderFlat(AABB.newTemp(xRenderOffs - g, yRenderOffs - g, zRenderOffs - g, xRenderOffs + xs + g, yRenderOffs + ys + g, zRenderOffs + zs + g));
	//glEndList();
	setDirty();
}

void Chunk::translateToPos()
{
	glTranslatef2((float)x, (float)y, (float)z);
}

// Shared tessellation logic.
// cpuOnly=true  → uses tArg (background thread tesselator), stores into _cpuMesh[], NO GL calls.
// cpuOnly=false → uses t (global Tesselator::instance), uploads to GPU immediately (original path).
// M2 normal helper (defined below under the desktop guard) — forward decl so
// the sync rebuild path below can call it before its definition.
#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
static void buildChunkNormalColors(const std::vector<uint8_t>& mesh, int vcount, std::vector<uint8_t>& outPTC);
// Set by GameRenderer once a mod actually queries Render.normalTexture:
// after that, chunk rebuilds also produce the per-layer normal VBOs. Off by
// default so plain gameplay doesn't pay the double rebuild cost.
extern bool g_chunkNormalsWanted;

// ── tile 属性缓存（渲染网格生成用）────────────────────────────────────────
// 网格生成时每个方块要问 3 个虚函数：getRenderShape()、getRenderLayer() 和
// 6 个邻居各一次 isSolidRender() —— /Od 构建下这些调用很贵（无内联）。而这些
// 属性对同一个 tile 实例是常量（查看各 tile 的覆写：StoneSlabTile 读成员
// fullSize、ModTile 读 _renderLayer、Door/Trapdoor/Chest 返回常量，都不依赖
// 位置或 data）。因此按 id 缓存，并用 tile 指针校验：mod 在运行时替换/新增
// 方块后指针变化会自动失效重算。
static const Tile*  s_tileCachePtr[256] = {0};
static unsigned char s_tileSolid[256];
static signed char    s_tileShape[256];
static signed char    s_tileLayer[256];

static inline void tileRefreshCache(int id, Tile* t) {
	s_tileCachePtr[id] = t;
	s_tileSolid[id] = t->isSolidRender() ? 1 : 0;
	s_tileShape[id] = (signed char)t->getRenderShape();
	s_tileLayer[id] = (signed char)t->getRenderLayer();
}
static inline bool tileSolidCached(int id) {
	if (id <= 0 || id > 255) return id > 0 && Tile::tiles[id] ? Tile::tiles[id]->isSolidRender() : false;
	Tile* t = Tile::tiles[id];
	if (t == NULL) return false;
	if (s_tileCachePtr[id] != t) tileRefreshCache(id, t);
	return s_tileSolid[id] != 0;
}
static inline int tileShapeCached(int id) {
	if (id <= 0 || id > 255) return id > 0 && Tile::tiles[id] ? Tile::tiles[id]->getRenderShape() : -1;
	Tile* t = Tile::tiles[id];
	if (t == NULL) return -1;
	if (s_tileCachePtr[id] != t) tileRefreshCache(id, t);
	return s_tileShape[id];
}
static inline int tileLayerCached(int id) {
	if (id <= 0 || id > 255) return id > 0 && Tile::tiles[id] ? Tile::tiles[id]->getRenderLayer() : -1;
	Tile* t = Tile::tiles[id];
	if (t == NULL) return -1;
	if (s_tileCachePtr[id] != t) tileRefreshCache(id, t);
	return s_tileLayer[id];
}
#endif
void Chunk::rebuildImpl(Tesselator& tArg, bool cpuOnly)
{
	// Whole-function timer: lets F3 tell how much of a chunk rebuild happens
	// OUTSIDE the mesh steps below (setup / snapshots / hooks).
	double _implStart = FrameProf::on() ? FrameProf::now() : 0.0;
	updates++;

	int x0 = x;
	int y0 = y;
	int z0 = z;
	int x1 = x + xs;
	int y1 = y + ys;
	int z1 = z + zs;
	for (int l = 0; l < NumLayers; l++) {
		empty[l] = true;
	}
	_empty = true;

	LevelChunk::touchedSky = false;

	int r = 1;
	double _setupStart = FrameProf::on() ? FrameProf::now() : 0.0;
	// 性能：跨重建复用 Region（thread_local，后台网格线程各用各的实例）。
	// Region 的构造每次都要 new/delete 两个指针数组，并且对每个 chunk 列调用
	// Level::getChunk()（内部取 recursive_mutex）。实测占每次重建的 0.23ms（约
	// 37%），而重建区域固定是“本块 + 外扩一圈”，没有理由每块重建一次。
	static thread_local Region* s_regionCache = NULL;
	if (s_regionCache == NULL)
		s_regionCache = new Region(level, x0 - r, y0 - r, z0 - r, x1 + r, y1 + r, z1 + r);
	else
		s_regionCache->reset(level, x0 - r, y0 - r, z0 - r, x1 + r, y1 + r, z1 + r);
	Region& region = *s_regionCache;
	TileRenderer tileRenderer(&region);
	if (_setupStart != 0.0)
		FrameProf::add(FrameProf::SEC_SETUP, _setupStart, FrameProf::now());

	bool doRenderLayer[NumLayers] = {true, false, false};

	// ── 性能：一次取块 + 整块跳过 + 跳过全空高度段 ──────────────────────────
	// 原实现对每个层循环都把整片 (ys × zs × xs) 方块空间完整走一遍，每格都重新
	// region.getTile()（region → chunk 缓存 → level 的多层间接）。这里：
	//   1) 把方块 id 连外扩一圈一起缓存（面剔除/邻接判断只看这张表）；
	//   2) 对"标准满格立方体且 6 个邻居都是不透明实心方块"的格子直接跳过 ——
	//      此时 6 个面都会被 Tile::shouldRenderFace 剔除，不产生任何顶点
	//      （地下实心区域占多数，省掉的正是最贵的那部分）；
	//   3) 记录每层方块所在的 y 区间，层循环只扫该区间（跳过地表之上的空段）。
	// 语义不变：哪些方块、哪些层、顶点顺序全部与原实现一致。
	const int spanX = x1 - x0;
	const int spanZ = z1 - z0;
	const int spanY = y1 - y0;
	const int cx = spanX + 2, cz = spanZ + 2, cy = spanY + 2;
	double _meshPrefetchStart = FrameProf::now();
	std::vector<unsigned short> nb((size_t)cx * cz * cy);
	{
		size_t ni = 0;
		for (int yy = 0; yy < cy; ++yy)
			for (int zz = 0; zz < cz; ++zz)
				for (int xx = 0; xx < cx; ++xx) {
					int id = region.getTile(x0 - 1 + xx, y0 - 1 + yy, z0 - 1 + zz);
					nb[ni++] = (unsigned short)(id > 0 ? id : 0);
				}
	}
	// nb 索引：原点为 (x0-1, y0-1, z0-1)
	#define NB_AT(xx, yy, zz) nb[(((size_t)(yy)) * cz + (zz)) * cx + (xx)]

	// 每层待渲染的格子（线性索引；按 y→z→x 收集，与原有顶点顺序一致）
	std::vector<unsigned int> layerCells[NumLayers];
	bool layerHas[NumLayers];
	for (int l = 0; l < NumLayers; ++l) layerHas[l] = false;

	for (int yy = 0; yy < spanY; ++yy) {
		for (int zz = 0; zz < spanZ; ++zz) {
			for (int xx = 0; xx < spanX; ++xx) {
				const int gx = xx + 1, gy = yy + 1, gz = zz + 1;   // nb 坐标
				int tileId = NB_AT(gx, gy, gz);
				if (tileId <= 0) continue;
				Tile* tile = Tile::tiles[tileId];
				if (tile == NULL) continue;

				// 模组动画方块：几何改成每帧重画（ModEngine 的动画方块通道），
				// 不进区块网格 —— 否则姿态会被烤进 VBO，只能重建时才变一下。
				if (Tile::animatedTile[tileId]) {
					if (ModEngine::instance)
						ModEngine::instance->noteAnimatedBlockSeen(tileId, x0 + xx, y0 + yy, z0 + zz);
					continue;
				}

				if (tileShapeCached(tileId) == Tile::SHAPE_BLOCK
					&& tile->xx0 == 0.0f && tile->xx1 == 1.0f
					&& tile->yy0 == 0.0f && tile->yy1 == 1.0f
					&& tile->zz0 == 0.0f && tile->zz1 == 1.0f) {
					bool enclosed = true;
					const int nn[6] = { NB_AT(gx, gy - 1, gz), NB_AT(gx, gy + 1, gz),
					                    NB_AT(gx, gy, gz - 1), NB_AT(gx, gy, gz + 1),
					                    NB_AT(gx - 1, gy, gz), NB_AT(gx + 1, gy, gz) };
					for (int f = 0; f < 6; ++f) {
						if (!tileSolidCached(nn[f])) { enclosed = false; break; }
					}
					if (enclosed) continue;   // 6 面全被遮挡，没有几何
				}

				int rl = tileLayerCached(tileId);
				if (rl < 0 || rl >= NumLayers) continue;
				layerCells[rl].push_back((unsigned int)(((size_t)yy * spanZ + zz) * spanX + xx));
				layerHas[rl] = true;
			}
		}
	}
	for (int l = 1; l < NumLayers; ++l)
		doRenderLayer[l] = layerHas[l];
	FrameProf::add(FrameProf::SEC_MESH_PREFETCH, _meshPrefetchStart, FrameProf::now());

	// 层3(植被/动画层)收集时: 与 _swayBase 顶点顺序一致地记录每顶点 sway 幅度
	// (属主 tile 的显式幅度, 默认 0)。swayAmpCollect 仅对当前在收集的动画层开启。
	bool swayAmpCollect = false;
	std::vector<float> swayAmpTmp;
	for (int l = 0; l < NumLayers; l++) {
		if (!doRenderLayer[l]) continue;
		bool rendered = false;

		bool started = false;
		swayAmpTmp.clear();
		swayAmpCollect = (l == 3);  // 动画层=3(可摇方块层; 0/1/2 不参与摆动)

		// 性能：本层计时放在“层”级别，而不是逐方块调用 QueryPerformanceCounter。
		// 逐方块计时在地表 chunk（近万格）会自己吃掉 ~10ms/块，F3 一开帧率就从
		// 60 掉到个位数，测量结果完全失真（实测 dirtyMesh=76ms / 7.8 块 ≈ 9.8ms 每块）。
		double _layerStart = FrameProf::on() ? FrameProf::now() : 0.0;
		{
			// 只遍历本层真正要渲染的格子（预取时收集），不再对整片 16×ys×16
			// 空间做三重嵌套扫描：内部/地下的格子在上面已被排除，既省下大量
			// 空转的 drawLayer 判定，也避免了每层重复扫同一片空间。
			const std::vector<unsigned int>& cells = layerCells[l];
			for (size_t _ci = 0; _ci < cells.size(); ++_ci) {
				const unsigned int _idx = cells[_ci];
				const int xx = (int)(_idx % (unsigned)spanX);
				const int zz = (int)((_idx / (unsigned)spanX) % (unsigned)spanZ);
				const int yy = (int)(_idx / (unsigned)(spanX * spanZ));
				int tileId = NB_AT(xx + 1, yy + 1, zz + 1);
				Tile* tile = Tile::tiles[tileId];
				if (tile == NULL) continue;

						if (!started) {
							started = true;

#ifndef USE_VBO
							if (!cpuOnly) {
								glNewList(lists + l, GL_COMPILE);
								glPushMatrix2();
								translateToPos();
								float ss = 1.000001f;
								glTranslatef2(-zs / 2.0f, -ys / 2.0f, -zs / 2.0f);
								glScalef2(ss, ss, ss);
								glTranslatef2(zs / 2.0f, ys / 2.0f, zs / 2.0f);
							}
#endif
							tArg.begin();
							tArg.offset(0.0f, 0.0f, 0.0f);
						}

						// 记录本 tile 顶点的 sway 幅度区间: [v0,v1) 个顶点
						// 属于该 tile;0 = 该方块不参与摇摆(默认)。
						int v0 = tArg.getVertexCount();
						rendered |= tileRenderer.tesselateInWorld(tile, x0 + xx, y0 + yy, z0 + zz);
						int v1 = tArg.getVertexCount();
						if (swayAmpCollect) {
							float a = Tile::getRenderStateAmp(tileId, Tile::RENDER_STATE_SWAY);
							for (int k = v0; k < v1; ++k) swayAmpTmp.push_back(a);
						}
					}
		}
		if (_layerStart != 0.0)
			FrameProf::add(FrameProf::SEC_MESH_TESS, _layerStart, FrameProf::now());

		if (started) {
#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
			if (cpuOnly) {
				tArg.endToCPU(_cpuMesh[l], _cpuVertexCount[l]);
				// 植被层(layer3): 常驻一份 CPU 基网格,渲染线程据此做随风摇摆
				if (l == 3 && _cpuVertexCount[l] > 0) {
					_swayBase = _cpuMesh[l];
					_swayCount = _cpuVertexCount[l];
					_swayDirty = true;
					_swayWork.clear();
				}
			} else {
#ifdef USE_VBO
				// 植被层(layer3): end 前快照 CPU 顶点作 sway 基网格(主线程 rebuild 路径)
				if (l == 3) {
					tArg.snapshotTo(_swayBase, _swayCount);
					_swayDirty = true;
					_swayWork.clear();
				}
				// M2: layer0/1/3 法线 VBO(法线色编码 PTC) — 同步 rebuild 路径
				// (end 前快照顶点, end 后补 id)。layer3(sway 植被)用基网格静态法线近似。
				// 懒构建: 无光影 mod 请求 normalTexture 时跳过(省一半重建 CPU/上传)。
				GLuint syncNormId = 0;
				if (g_chunkNormalsWanted && (l == 0 || l == 1 || l == 3)) {
					std::vector<uint8_t> snap;
					int scnt = 0;
					tArg.snapshotTo(snap, scnt);
					std::vector<uint8_t> nptc;
					buildChunkNormalColors(snap, scnt, nptc);
					if (!nptc.empty()) {
						if (_nBuf.empty()) _nBuf.assign((size_t)NumLayers, 0);
						if (_nBuf[l] == 0) glGenBuffers(1, &_nBuf[l]);
						glBindBuffer2(GL_ARRAY_BUFFER, _nBuf[l]);
						glBufferData2(GL_ARRAY_BUFFER, (GLsizeiptr)nptc.size(), nptc.data(), GL_STATIC_DRAW);
						syncNormId = _nBuf[l];
					}
				}
				double _upStart = FrameProf::now();
				renderChunk[l] = tArg.end(true, vboBuffers[l]);
				FrameProf::add(FrameProf::SEC_MESH_UPLOAD, _upStart, FrameProf::now());
				renderChunk[l].pos.x = (float)this->x;
				renderChunk[l].pos.y = (float)this->y;
				renderChunk[l].pos.z = (float)this->z;
				renderChunk[l].normalVboId = syncNormId;
#else
				tArg.end(false, -1);
				glPopMatrix2();
				glEndList();
#endif
			}
#else  // !desktop
#ifdef USE_VBO
			double _upStart = FrameProf::now();
			renderChunk[l] = tArg.end(true, vboBuffers[l]);
			FrameProf::add(FrameProf::SEC_MESH_UPLOAD, _upStart, FrameProf::now());
			renderChunk[l].pos.x = (float)this->x;
			renderChunk[l].pos.y = (float)this->y;
			renderChunk[l].pos.z = (float)this->z;
#else
			tArg.end(false, -1);
			glPopMatrix2();
			glEndList();
#endif
#endif  // desktop
			tArg.offset(0, 0, 0);
		} else {
			rendered = false;
#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
			if (cpuOnly) {
				_cpuMesh[l].clear();
				_cpuVertexCount[l] = 0;
			}
#endif
		}
		// 动画层(layer3)基网格与逐顶点 sway 幅度对齐: rebuild 时按 tile 收集
		// 的 swayAmpTmp 应与 _swayCount 顶点数一致(不一致=旧路径/无顶点则清空)。
		if (l == 3) {
			if (!swayAmpTmp.empty() && (int)swayAmpTmp.size() == _swayCount) {
				_swayAmp.swap(swayAmpTmp);
			} else {
				_swayAmp.clear();
			}
			_hasAnySway = false;
			for (size_t i = 0; i < _swayAmp.size(); ++i) {
				if (_swayAmp[i] > 0.001f) { _hasAnySway = true; break; }
			}
		}
		if (rendered) {
			empty[l] = false;
			_empty = false;
		}
		// (原 `if (!renderNextLayer) break;` 提前退出已不需要：doRenderLayer[]
		//  现在由预取结果精确开启，未开启的层在上面直接 continue。)
	}

	skyLit = LevelChunk::touchedSky;
	compiled = true;
	if (_implStart != 0.0)
		FrameProf::add(FrameProf::SEC_CHUNK_ALL, _implStart, FrameProf::now());

	#undef NB_AT
}

void Chunk::rebuild()
{
	if (!dirty) return;
	rebuildImpl(t, false);
}

#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
// M2: 由 CPU 顶点流(interleaved PTC, TRIANGLES)按三角面离线求法线,
// 编码为"法线顶点缓冲"(仍为 VertexDeclPTC 布局: xyz 位置副本 + uv=0 +
// color = (n*0.5+0.5)*255)。渲染法线通道走 FFP(禁纹理), 与亮度通道同路径, 稳定。
static void buildChunkNormalColors(const std::vector<uint8_t>& mesh, int vcount, std::vector<uint8_t>& outPTC) {
	outPTC.clear();
	if (vcount < 3 || mesh.empty() || mesh.size() < (size_t)vcount * VertexSizeBytes) return;
	outPTC.resize((size_t)vcount * VertexSizeBytes);
	const VertexDeclPTC* v = reinterpret_cast<const VertexDeclPTC*>(mesh.data());
	VertexDeclPTC* o = reinterpret_cast<VertexDeclPTC*>(outPTC.data());
	for (int i = 0; i + 2 < vcount; i += 3) {
		float ax = v[i + 1].x - v[i].x, ay = v[i + 1].y - v[i].y, az = v[i + 1].z - v[i].z;
		float bx = v[i + 2].x - v[i].x, by = v[i + 2].y - v[i].y, bz = v[i + 2].z - v[i].z;
		float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
		float len = sqrtf(nx * nx + ny * ny + nz * nz);
		if (len < 1e-6f) { nx = 0.0f; ny = 1.0f; nz = 0.0f; }
		else { float inv = 1.0f / len; nx *= inv; ny *= inv; nz *= inv; }
		unsigned char cr = (unsigned char)((nx * 0.5f + 0.5f) * 255.0f + 0.5f);
		unsigned char cg = (unsigned char)((ny * 0.5f + 0.5f) * 255.0f + 0.5f);
		unsigned char cb = (unsigned char)((nz * 0.5f + 0.5f) * 255.0f + 0.5f);
		for (int k = 0; k < 3; k++) {
			o[i + k].x = v[i + k].x; o[i + k].y = v[i + k].y; o[i + k].z = v[i + k].z;
			o[i + k].u = 0.0f; o[i + k].v = 0.0f;
			o[i + k].color = 0xFF000000u | ((unsigned int)cr) | ((unsigned int)cg << 8) | ((unsigned int)cb << 16);
		}
	}
}

void Chunk::rebuildCPU(Tesselator& tArg)
{
	// Note: dirty flag is NOT checked here — the caller (mesh thread) already
	// decided this chunk needs work. We reset empty[] then tessellate.
	rebuildImpl(tArg, true);
	_cpuMeshReady.store(true, std::memory_order_release);
}

void Chunk::uploadGPU()
{
#ifdef USE_VBO
	for (int l = 0; l < NumLayers; l++) {
		if (_cpuMesh[l].empty()) continue;

		int bytes = (int)_cpuMesh[l].size();
		int access = GL_STATIC_DRAW;
		glBindBuffer2(GL_ARRAY_BUFFER, vboBuffers[l]);
		glBufferData2(GL_ARRAY_BUFFER, bytes, _cpuMesh[l].data(), access);

		// M2: 法线 VBO(layer0/1/3; layer2 水与 layer3 sway 顶点每帧变, sway 用基网格近似)
		// 懒构建: 无光影 mod 请求 normalTexture 时跳过(见 GameRenderer.cpp g_chunkNormalsWanted)。
		if (g_chunkNormalsWanted && (l == 0 || l == 1 || l == 3)) {
			if (_nBuf.empty()) _nBuf.assign((size_t)NumLayers, 0);
			std::vector<uint8_t> nptc;
			buildChunkNormalColors(_cpuMesh[l], _cpuVertexCount[l], nptc);
			if (!nptc.empty()) {
				if (_nBuf[l] == 0) glGenBuffers(1, &_nBuf[l]);
				glBindBuffer2(GL_ARRAY_BUFFER, _nBuf[l]);
				glBufferData2(GL_ARRAY_BUFFER, (GLsizeiptr)nptc.size(), nptc.data(), GL_STATIC_DRAW);
				renderChunk[l].normalVboId = _nBuf[l];
			}
		}

		renderChunk[l].vboId     = vboBuffers[l];
		renderChunk[l].vertexCount = _cpuVertexCount[l];
		renderChunk[l].pos.x = (float)this->x;
		renderChunk[l].pos.y = (float)this->y;
		renderChunk[l].pos.z = (float)this->z;

		// Release CPU memory after upload
		_cpuMesh[l].clear();
		_cpuMesh[l].shrink_to_fit();
	}
#endif
	_cpuMeshReady.store(false, std::memory_order_release);
	_inFlight.store(false, std::memory_order_release);
}
#endif

float Chunk::distanceToSqr( const Entity* player ) const
{
	float xd = (float) (player->x - xm);
	float yd = (float) (player->y - ym);
	float zd = (float) (player->z - zm);
	return xd * xd + yd * yd + zd * zd;
}

float Chunk::squishedDistanceToSqr( const Entity* player ) const
{
	float xd = (float) (player->x - xm);
	float yd = (float) (player->y - ym) * 2;
	float zd = (float) (player->z - zm);
	return xd * xd + yd * yd + zd * zd;
}

void Chunk::reset()
{
	for (int i = 0; i < NumLayers; i++) {
		empty[i] = true;
	}
	visible = false;
	compiled = false;
    _empty = true;
}

int Chunk::getList( int layer )
{
	if (!visible) return -1;
	if (!empty[layer]) return lists + layer;
	return -1;
}

RenderChunk& Chunk::getRenderChunk( int layer )
{
	return renderChunk[layer];
}

int Chunk::getAllLists( int displayLists[], int p, int layer )
{
	if (!visible) return p;
	if (!empty[layer]) displayLists[p++] = (lists + layer);
	return p;
}

void Chunk::cull( Culler* culler )
{
	visible = culler->isVisible(bb);
}

void Chunk::renderBB()
{
	//glCallList(lists + 2);
}

bool Chunk::isEmpty()
{
	return compiled && _empty;//empty[0] && empty[1] && empty[2];
//	if (!compiled) return false;
//	return empty[0] && empty[1];
}

void Chunk::setDirty()
{
	dirty = true;
}

void Chunk::setClean()
{
	dirty = false;
	queued = false;
}

bool Chunk::isDirty()
{
	return dirty;
}

void Chunk::resetUpdates()
{
	updates = 0;
	//swRebuild.reset();
}

#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
// 植被随风摇摆: 把 layer3 基网格顶点按正弦做水平偏移(底部不动,越高摆幅越大),
// 上传到该 chunk 的 layer3 VBO(动态)。只处理 sway 半径内的 chunk。
void Chunk::swayUpdate(float timeSec, float camX, float camZ)
{
	if (!hasSway()) return;
	if (s_swayAmp <= 0.0001f) return;   // 摇摆默认关: 只有模组显式开启才生效
	// 该 chunk 没有幅度>0 的方块(全部静止): 无需重算/重传, 保持原 VBO。
	if (!_hasAnySway) return;

	float dx = (float)this->xm - camX;
	float dz = (float)this->zm - camZ;
	const float SWAY_RADIUS = 26.0f;
	float dist2 = dx * dx + dz * dz;
	if (dist2 > SWAY_RADIUS * SWAY_RADIUS) return;

	float amp;
	{
		float dist = sqrtf(dist2);
		float f = 1.0f - dist / SWAY_RADIUS;
		if (f < 0.0f) f = 0.0f;
		// 幅度 = 全局 amp × 原公式(近处强/随距离平滑衰减)
		amp = s_swayAmp * (0.30f + 0.55f * f) * f * f;
	}

	if (_swayDirty || _swayWork.empty() || _swayWork.size() != _swayBase.size()) {
		_swayWork = _swayBase;
		_swayDirty = false;
	}

	const float t = timeSec * s_swaySpeed;
	// 主摆速度(用户确认速度不是问题,保持原节奏)
	const float swayX = sinf(t * 1.9f);
	const float swayZ = sinf(t * 1.3f + 1.1f);

	VertexDeclPTC* base = (VertexDeclPTC*)_swayBase.data();
	VertexDeclPTC* out  = (VertexDeclPTC*)_swayWork.data();
	const int n = _swayCount;
	const bool hasPerVertexAmp = ((int)_swayAmp.size() == n);
	for (int i = 0; i < n; i++) {
		const VertexDeclPTC& b = base[i];
		// 逐方块 sway 幅度: 0(默认/显式关闭)的顶点保持静止 — 只有
		// Block.setState 开启过的方块随风摆。无幅度数组(旧网格)则全摇兼容。
		float tileAmp = 1.0f;
		if (hasPerVertexAmp) {
			tileAmp = _swayAmp[i];
			if (tileAmp <= 0.001f) continue;  // 保持 base 原值(不摆动)
		}
		// 线性高度权重: y46 起整个树冠一起摆(不再平方压低下半部)
		float w = (b.y - 46.0f) * 0.02f;
		if (w < 0.0f) w = 0.0f;
		else if (w > 1.0f) w = 1.0f;
		if (w < 0.01f) continue;
		// 相邻植被相位错开 + 整体风浪
		float ph = b.x * 1.9f + b.z * 2.7f;
		float ox = swayX * 0.8f + sinf(ph + t * 0.35f) * 0.2f;
		float oz = swayZ * 0.8f + cosf(ph * 0.9f - t * 0.3f) * 0.2f;
		out[i].x = b.x + ox * amp * tileAmp * w;
		out[i].z = b.z + oz * amp * tileAmp * w;
	}

	glBindBuffer2(GL_ARRAY_BUFFER, renderChunk[3].vboId);
	glBufferData2(GL_ARRAY_BUFFER, (GLsizeiptr)_swayWork.size(), _swayWork.data(), GL_DYNAMIC_DRAW);
	glBindBuffer2(GL_ARRAY_BUFFER, 0);
}
#endif

void Chunk::setSwayGlobals(float amp, float speed)
{
	s_swayAmp = amp;
	s_swaySpeed = (speed > 0.01f) ? speed : 1.0f;
}
