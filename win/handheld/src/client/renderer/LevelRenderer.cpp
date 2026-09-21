#include "LevelRenderer.h"
#include "../../platform/time.h"
#include "../../world/level/tile/ModTile.h"

#include "DirtyChunkSorter.h"
#include "DistanceChunkSorter.h"
#include "Chunk.h"
#include "TileRenderer.h"
#include "../Minecraft.h"
#include "../../mod/ModEngine.h"
#include "../../util/Mth.h"
#include "../../world/entity/player/Player.h"
#include "../../world/level/tile/LevelEvent.h"
#include "../../world/level/tile/LeafTile.h"
#include "../../client/particle/ParticleEngine.h"
#include "../../client/particle/ParticleInclude.h"
#include "../sound/SoundEngine.h"
#include "culling/Culler.h"
#include "entity/EntityRenderDispatcher.h"
#include "../model/HumanoidModel.h"

#include "GameRenderer.h"
#include "../../AppPlatform.h"
#include "../../util/PerfTimer.h"
#include "../../util/FrameProf.h"

// ── 重建预算用的帧级状态 ────────────────────────────────────────────────
// g_frameStamp: 主循环每帧递增（见 main_win32.h），让“每帧重建预算”能跨
//               updateDirtyChunks 的多次调用累计，而不是每次调用各自计时。
// g_diagUpdateMs: 上一帧 update() 总耗时（ms），用于自适应调整预算。
#if !defined(_WIN32)
double g_diagUpdateMs = 0.0;   // 非 Win32 平台没有主循环诊断变量
#else
extern double g_diagUpdateMs;  // 定义在 main_win32.h
#endif
int    g_frameStamp = 0;
static int    s_budgetStamp = -1;
static double s_budgetStart = 0.0;
static float  s_budgetMs = 6.0f;
#include "Textures.h"
#include "Tesselator.h"
#include "TextureTesselator.h"
#include "TextureData.h"
#include "tileentity/TileEntityRenderDispatcher.h"
#include "../particle/BreakingItemParticle.h"
#include "../../util/Random.h"
#include <cmath>
#include <vector>

#include "../../client/player/LocalPlayer.h"

#ifdef GFX_SMALLER_CHUNKS
/* static */ const int LevelRenderer::CHUNK_SIZE = 8;
#else
/* static */ const int LevelRenderer::CHUNK_SIZE = 16;
#endif


LevelRenderer::LevelRenderer( Minecraft* mc)
:	mc(mc),
	textures(mc->textures),
	level(NULL),
	cullStep(0),

	chunkLists(0),
	xChunks(0), yChunks(0), zChunks(0),

	chunks(NULL),
	sortedChunks(NULL),

	xMinChunk(0), yMinChunk(0), zMinChunk(0),
	xMaxChunk(0), yMaxChunk(0), zMaxChunk(0),

	lastViewDistance(-1),

	noEntityRenderFrames(2),
	totalEntities(0),
	renderedEntities(0),
	culledEntities(0),

	occlusionCheck(false),
	totalChunks(0), offscreenChunks(0), renderedChunks(0), occludedChunks(0), emptyChunks(0),

	chunkFixOffs(0),
	xOld(-9999), yOld(-9999), zOld(-9999),

	_scriptCam(false), _scX(0), _scY(0), _scZ(0),

	ticks(0),
	skyList(0), starList(0), darkList(0),
	tileRenderer(NULL),
	destroyProgress(0)
{
#ifdef USE_VBO
	// Pool must cover the largest possible dist. allChanged() caps dist at 1024 for
	// macOS/Linux and 400 for other platforms. Use the per-platform cap to size exactly.
#if defined(MACOS) || defined(LINUX) || defined(WIN32)
	static const int MAX_RENDER_DIST = 1024;
#else
	static const int MAX_RENDER_DIST = 256;
#endif
	{
		int maxW = MAX_RENDER_DIST / CHUNK_SIZE + 1;
		numListsOrBuffers = maxW * maxW * (128/CHUNK_SIZE) * 4;   // layers 0..3 (3=植被摇摆)
	}
	chunkBuffers = new GLuint[numListsOrBuffers];
	glGenBuffers2(numListsOrBuffers, chunkBuffers);
	LOGI("numBuffers: %d\n", numListsOrBuffers);
#else
	int maxChunksWidth = 1024 / CHUNK_SIZE;
	numListsOrBuffers = maxChunksWidth * maxChunksWidth * maxChunksWidth * 4;
	chunkLists = glGenLists(numListsOrBuffers);
#endif

	// 1.6.4 天空（照抄 Multiplatform-1.6.4_classic / modifiedeight）：
	// 天幕和星空的网格要在**所有平台**上建。原来这两句被包在
	// `#if defined(OPENGL_ES) || defined(MACOS) || defined(LINUX)` 里，Win32 上
	// skyBuffer 是未初始化的垃圾值，一旦拿去画就是驱动层崩溃。
	generateSky();
	buildStarsMesh();

	// 云网格缓存键（照抄 modifiedeight 的 field_15C/field_160/field_164）：
	// 格子坐标或云色一变就重新生成。
	cloudMeshTexX = 0;
	cloudMeshTexY = 0;
	cloudMeshColorR = cloudMeshColorG = cloudMeshColorB = cloudMeshColorA = -1.0f;
}

LevelRenderer::~LevelRenderer()
{
	delete tileRenderer;
	tileRenderer = NULL;

	deleteChunks();

#ifdef USE_VBO
	glDeleteBuffers(numListsOrBuffers, chunkBuffers);
	delete[] chunkBuffers;
#else
	glDeleteLists(numListsOrBuffers, chunkLists);
#endif
}

// 位置-only 顶点格式（MeshBuffer 内部只存指针，所以这个对象必须活到进程结束）。
static const MeshBuffer::VertexFormat s_posOnlyVertexFormat = [] {
	MeshBuffer::VertexFormat vf;
	vf.enableField(MeshBuffer::VertexFormat::FIELD0);
	return vf;
}();

// 用"位置-only"格式画一个网格：MeshBuffer::render() 只负责设顶点指针，
// 客户端的 vertex array 由调用方开关。
static void renderPosOnlyMesh(MeshBuffer& mesh) {
	glDisableClientState2(GL_COLOR_ARRAY);
	glDisableClientState2(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState2(GL_VERTEX_ARRAY);
	mesh.render();
	glDisableClientState2(GL_VERTEX_ARRAY);
}

// 照抄 Multiplatform-1.6.4（modifiedeight）LevelRenderer::generateSky：
// 天幕是 y=128、半径 2000 的三角扇——1 个圆心 + 11 个圆周点（角度 i/10*2π），
// 一共 12 个顶点、10 个三角形。颜色不写进网格，绘制时由 glColor4f(天空色) 决定。
void LevelRenderer::generateSky() {
	const int SEGMENTS = 11;                    // 参考项目里循环变量 v3 = 0..10
	static float verts[(1 + SEGMENTS) * 3];     // 只含位置（stride 12）

	verts[0] = 0.0f;
	verts[1] = 128.0f;
	verts[2] = 0.0f;
	for (int i = 0; i < SEGMENTS; ++i) {
		Vec3 v(2000.0f, 0, 0);
		v.yRot((float) i / 10.0f * 6.2832f);
		verts[(i + 1) * 3 + 0] = v.x;
		verts[(i + 1) * 3 + 1] = 128.0f;
		verts[(i + 1) * 3 + 2] = v.z;
	}

	skyMesh.load(&s_posOnlyVertexFormat, verts, 1 + SEGMENTS, NULL, 0, 0, GL_TRIANGLE_FAN);
}

// 照抄 Multiplatform-1.6.4（modifiedeight）LevelRenderer::_buildStarsMesh：
// 1500 颗星随机撒在半径 100 的球面上，每颗是一个朝向观察者的小四边形
// （4 个顶点展开成 2 个三角形）。颜色不写进网格，绘制时由 glColor4f 决定。
void LevelRenderer::buildStarsMesh() {
	static std::vector<float> verts;
	verts.clear();
	verts.reserve(1500 * 6 * 3);

	Random random(10842);                       // 参考项目：Random v38(10842)
	for (int i = 0; i < 1500; ++i) {
		float x = random.nextFloat() * 2.0f - 1.0f;
		float y = random.nextFloat() * 2.0f - 1.0f;
		float z = random.nextFloat() * 2.0f - 1.0f;
		float lenSq = x * x + y * y + z * z;
		if (!(lenSq < 1.0f && lenSq > 0.01f)) continue;

		float size = random.nextFloat() * 0.1f + 0.15f;

		// 归一化到单位球面，再放大到半径 100
		float inv = 1.0f / sqrtf(lenSq);
		float nx = x * inv, ny = y * inv, nz = z * inv;
		float px = nx * 100.0f, py = ny * 100.0f, pz = nz * 100.0f;

		// 经度（绕 Y）与纬度
		float lon = atan2f(nx, nz);
		float sinLon = sinf(lon), cosLon = cosf(lon);
		float lat = atan2f(sqrtf(nz * nz + nx * nx), ny);
		float sinLat = sinf(lat), cosLat = cosf(lat);

		// 让四边形在球面上随机滚一个角度
		float t = random.nextFloat();
		float roll = t * 3.1416f + t * 3.1416f;
		float cosRoll = cosf(roll), sinRoll = sinf(roll);

		float q[4][3];
		int c = 0;
		do {
			int a = c & 2;
			++c;
			float o1 = (float) ((c & 2) - 1) * size;   // v35
			float o2 = (float) (a - 1) * size;         // v36
			float m = o2 * cosRoll - o1 * sinRoll;     // C
			float mm = m * cosLat;                     // A
			float n = o2 * sinRoll + o1 * cosRoll;     // B

			q[c - 1][0] = px - mm * sinLon - n * cosLon;
			q[c - 1][1] = py + m * sinLat;
			q[c - 1][2] = pz - mm * cosLon + n * sinLon;
		} while (c != 4);

		// 4 个顶点 → 2 个三角形（v0,v1,v2 + v0,v2,v3），等价于参考项目的 quad。
		static const int order[6] = { 0, 1, 2, 0, 2, 3 };
		for (int k = 0; k < 6; ++k) {
			verts.push_back(q[order[k]][0]);
			verts.push_back(q[order[k]][1]);
			verts.push_back(q[order[k]][2]);
		}
	}

	starsMesh.load(&s_posOnlyVertexFormat, verts.data(), (int) (verts.size() / 3), NULL, 0, 0, GL_TRIANGLES);
}

// 照抄 Minecraft 1.6.4 RenderGlobal.renderSky 里的日出/日落扇形
// （calcSunriseSunsetColors 的那一段，本工程参考项目 modifiedeight 没实现它，
//  按 1.6.4 原版补：D:\...\RenderGlobal.java 的 renderSky）。
//
// 它是在天幕之后、日月之前画的一片"贴在地平线上的橙红扇面"：
// 中心点 (0,100,0) 用霞光色（带 alpha），边缘 16 个点用同一个颜色但 alpha=0，
// 于是从中心往外淡出；GL_SMOOTH 让固定管线做这个渐变。
// 旋转顺序照抄 1.6.4：(90, 绕X) → (太阳在地平线下 ? 180 : 0, 绕Z) → (90, 绕Z)。
void LevelRenderer::renderSunriseFan(float alpha) {
	float* sunrise = level->getSunriseColor(alpha);
	if (sunrise == NULL) return;

	float celestialRadians = level->getTimeOfDay(alpha) * Mth::PI * 2.0f;

	// 借用 GL 状态必须原样归还 —— 这里有个只在日出/日落发作的老 bug：
	// 天空是在**地形之前**画的（GameRenderer::renderLevel 先
	// prepareAndRenderClouds → renderSky，再 levelRenderer->render 画地形），
	// 而地形在"环境光遮蔽"开启时必须用 GL_SMOOTH（方块面 4 个顶点各有 AO 亮度）。
	// 旧代码照抄 1.6.4 在函数末尾硬写 glShadeModel(GL_FLAT)：1.6.4 的天空是在
	// 地形**之后**画的，所以那样写无害；搬到本工程（天空在前）就变成把地形的
	// GL_SMOOTH 踩成 GL_FLAT —— 每个方块面按三角形取"最后一个顶点"的颜色，
	// 于是每个方块上出现一块三角形暗斑，且只在日出/日落那几分钟出现
	// （其余时候 getSunriseColor 返回 NULL，本函数直接 return 不碰状态）。
	// 修法：进来先记下当前 shade model，出去时恢复。
	GLint prevShadeModel = GL_SMOOTH;
	glGetIntegerv(GL_SHADE_MODEL, &prevShadeModel);

	glDisable2(GL_TEXTURE_2D);
	glShadeModel2(GL_SMOOTH);          // 扇面靠顶点色渐变，必须 SMOOTH
	glPushMatrix2();
	{
		glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(sinf(celestialRadians) < 0.0f ? 180.0f : 0.0f, 0.0f, 0.0f, 1.0f);
		glRotatef(90.0f, 0.0f, 0.0f, 1.0f);

		float r = sunrise[0];
		float g = sunrise[1];
		float b = sunrise[2];
		float a = sunrise[3];

		Tesselator& t = Tesselator::instance;
		t.begin(GL_TRIANGLE_FAN);          // 1.6.4 的 startDrawing(6)
		t.color(r, g, b, a);               // 中心：霞光色
		t.vertex(0.0f, 100.0f, 0.0f);
		t.color(r, g, b, 0.0f);            // 边缘：同色但 alpha=0
		for (int j = 0; j <= 16; ++j) {
			float ang = (float) j * Mth::PI * 2.0f / 16.0f;
			float sa = sinf(ang);
			float ca = cosf(ang);
			t.vertex(sa * 120.0f, ca * 120.0f, -ca * 40.0f * a);
		}
		t.draw();
	}
	glPopMatrix2();
	glShadeModel2(prevShadeModel);      // 归还进入前的 shade model（不是硬写 FLAT）
}

// 照抄 Minecraft 1.6.4 RenderGlobal.renderSky 的太阳 + 月亮：
//   -90° 绕 Y 摆正东西方向，再按"天体角 × 360°"绕 X 转；
//   太阳在 y = +100（±30），月亮在 y = -100（±20）—— 两者是同一个坐标系里的对头，
//   所以转到地平线下时自然换人。混合是 (SRC_ALPHA, ONE)，雨天整体变淡。
void LevelRenderer::renderSunOrMoon(float alpha) {
	glEnable2(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glPushMatrix2();
	{
		float rainFade = 1.0f - level->getRainStrength(alpha);
		glColor4f2(1.0f, 1.0f, 1.0f, rainFade);
		glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
		glRotatef(level->getTimeOfDay(alpha) * 360.0f, 1.0f, 0.0f, 0.0f);

		Tesselator& t = Tesselator::instance;

		// 太阳
		t.begin();
		t.vertexUV(-30.0f, 100.0f, -30.0f, 0.0f, 0.0f);
		t.vertexUV( 30.0f, 100.0f, -30.0f, 1.0f, 0.0f);
		t.vertexUV( 30.0f, 100.0f,  30.0f, 1.0f, 1.0f);
		t.vertexUV(-30.0f, 100.0f,  30.0f, 0.0f, 1.0f);
		textures->loadAndBindTexture("environment/sun.png");
		t.draw();

		// 月亮（moon_phases.png 是 4x2 网格，取对应相位那一格）
		int moonPhase = level->getMoonPhase();
		int col = moonPhase % 4;
		int row = moonPhase / 4 % 2;
		float uMin = (float) (col + 0) / 4.0f;
		float vMin = (float) (row + 0) / 2.0f;
		float uMax = (float) (col + 1) / 4.0f;
		float vMax = (float) (row + 1) / 2.0f;
		t.begin();
		t.vertexUV(-20.0f, -100.0f,  20.0f, uMax, vMax);
		t.vertexUV( 20.0f, -100.0f,  20.0f, uMin, vMax);
		t.vertexUV( 20.0f, -100.0f, -20.0f, uMin, vMin);
		t.vertexUV(-20.0f, -100.0f, -20.0f, uMax, vMin);
		textures->loadAndBindTexture("environment/moon_phases.png");
		t.draw();
	}
	glPopMatrix2();
}

// 照抄 Minecraft 1.6.4 RenderGlobal.renderSky 的星空：
// 关掉纹理，颜色 = 星星亮度 × (1 - 雨强度)，亮度为 0 就不画。
// 注意 1.6.4 的星空是预先建好的一整片球面，**不跟着太阳角旋转**。
void LevelRenderer::renderStars(float alpha) {
	glDisable2(GL_TEXTURE_2D);
	float rainFade = 1.0f - level->getRainStrength(alpha);
	float brightness = level->getStarBrightness(alpha) * rainFade;
	if (brightness <= 0.0f) return;

	glColor4f2(brightness, brightness, brightness, brightness);
	renderPosOnlyMesh(starsMesh);
}

void LevelRenderer::setLevel( Level* level )
{
	if (this->level != NULL) {
		this->level->removeListener(this);
	}

	xOld = -9999;
	yOld = -9999;
	zOld = -9999;

	EntityRenderDispatcher::getInstance()->setLevel(level);
	EntityRenderDispatcher::getInstance()->setMinecraft(mc);
	this->level = level;

	delete tileRenderer;
	tileRenderer = new TileRenderer(level);

	if (level != NULL) {
		level->addListener(this);
		allChanged();
	}
}

void LevelRenderer::allChanged()
{
	deleteChunks();

#ifdef USE_VBO
	// Flush GPU pipeline so no in-flight draws reference old VBO data,
	// then orphan every buffer so MetalANGLE releases stale Metal backing.
	// This prevents GPU page faults when render distance changes cause a
	// different number of chunks to reuse the same VBO pool.
	glFinish();
	for (int i = 0; i < numListsOrBuffers; ++i) {
		glBindBuffer2(GL_ARRAY_BUFFER, chunkBuffers[i]);
		glBufferData2(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
	}
	glBindBuffer2(GL_ARRAY_BUFFER, 0);
#endif

	Tile::leaves->setFancy(mc->options.fancyGraphics);
	Tile::leaves_carried->setFancy(mc->options.fancyGraphics);
	lastViewDistance = mc->options.viewDistance & 7; // clamp to [0,7] — 0=far, 7=tiny

	// dist table: vd 0..7 → block distances (capped by maxChunksWidth in practice)
	static const int DIST_TABLE[8] = {512, 256, 128, 64, 768, 1024, 1536, 2048};
	int dist = DIST_TABLE[lastViewDistance];
	if (lastViewDistance <= 2 && mc->isPowerVR())
		dist = (int)((float)dist * 0.8f);
	LOGI("last: %d, power: %d\n", lastViewDistance, mc->isPowerVR());

	#if defined(RPI)
		dist *= 0.6f;
	#endif

#if defined(MACOS) || defined(LINUX) || defined(WIN32)
	if (dist > 1024) dist = 1024;
#else
	if (dist > 256) dist = 256;
#endif
	/*
	* if (Minecraft.FLYBY_MODE) { dist = 512 - CHUNK_SIZE * 2; }
	*/
	xChunks = (dist / LevelRenderer::CHUNK_SIZE) + 1;
	yChunks = (128 /  LevelRenderer::CHUNK_SIZE);
	zChunks = (dist / LevelRenderer::CHUNK_SIZE) + 1;
	chunksLength = xChunks * yChunks * zChunks;
	LOGI("chunksLength: %d. Distance: %d\n", chunksLength, dist);

	chunks = new Chunk*[chunksLength];
	sortedChunks = new Chunk*[chunksLength];

	int id = 0;
	int count = 0;

	xMinChunk = 0;
	yMinChunk = 0;
	zMinChunk = 0;
	xMaxChunk = xChunks;
	yMaxChunk = yChunks;
	zMaxChunk = zChunks;
	dirtyChunks.clear();
	//renderableTileEntities.clear();

	for (int x = 0; x < xChunks; x++) {
		for (int y = 0; y < yChunks; y++) {
			for (int z = 0; z < zChunks; z++) {
				const int c = getLinearCoord(x, y, z);
				Chunk* chunk = new Chunk(level, x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE, CHUNK_SIZE, chunkLists + id, &chunkBuffers[id]);

				if (occlusionCheck) {
					chunk->occlusion_id = 0;//occlusionCheckIds.get(count);
				}
				chunk->occlusion_querying = false;
				chunk->occlusion_visible = true;
				chunk->visible = true;
				chunk->id = count++;
				chunk->setDirty();

				chunks[c] = chunk;
				sortedChunks[c] = chunk;
				chunk->queued = true;
				dirtyChunks.push_back(chunk);

				id += 4;   // NumLayers=4 (0..3; 3=植被摇摆层)
			}
		}
	}

	if (level != NULL) {
		Entity* player = mc->cameraTargetPlayer;
		if (player != NULL) {
			this->resortChunks(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));
			DistanceChunkSorter distanceSorter(player);
			std::sort(sortedChunks, sortedChunks + chunksLength, distanceSorter);
		}
	}
	noEntityRenderFrames = 2;
}

void LevelRenderer::deleteChunks()
{
	if (!chunks) return;

	for (int z = 0; z < zChunks; ++z)
	for (int y = 0; y < yChunks; ++y)
	for (int x = 0; x < xChunks; ++x) {
		int c = getLinearCoord(x, y, z);
		delete chunks[c];
	}

	delete[] chunks;
	chunks = NULL;

	delete[] sortedChunks;
	sortedChunks = NULL;
}

void LevelRenderer::resortChunks( int xc, int yc, int zc )
{
	xc -= CHUNK_SIZE / 2;
	//yc -= CHUNK_SIZE / 2;
	zc -= CHUNK_SIZE / 2;
	xMinChunk = INT_MAX;
	yMinChunk = INT_MAX;
	zMinChunk = INT_MAX;
	xMaxChunk = INT_MIN;
	yMaxChunk = INT_MIN;
	zMaxChunk = INT_MIN;

	int dirty = 0;

	int s2 = xChunks * CHUNK_SIZE;
	int s1 = s2 / 2;

	for (int x = 0; x < xChunks; x++) {
		int xx = x * CHUNK_SIZE;

		int xOff = (xx + s1 - xc);
		if (xOff < 0) xOff -= (s2 - 1);
		xOff /= s2;
		xx -= xOff * s2;

		if (xx < xMinChunk) xMinChunk = xx;
		if (xx > xMaxChunk) xMaxChunk = xx;

		for (int z = 0; z < zChunks; z++) {
			int zz = z * CHUNK_SIZE;
			int zOff = (zz + s1 - zc);
			if (zOff < 0) zOff -= (s2 - 1);
			zOff /= s2;
			zz -= zOff * s2;

			if (zz < zMinChunk) zMinChunk = zz;
			if (zz > zMaxChunk) zMaxChunk = zz;

			for (int y = 0; y < yChunks; y++) {
				int yy = y * CHUNK_SIZE;
				if (yy < yMinChunk) yMinChunk = yy;
				if (yy > yMaxChunk) yMaxChunk = yy;

				Chunk* chunk = chunks[(z * yChunks + y) * xChunks + x];
				bool wasDirty = chunk->isDirty();
				chunk->setPos(xx, yy, zz);
				if (!wasDirty && chunk->isDirty()) {
					chunk->queued = true;
					dirtyChunks.push_back(chunk);
					++dirty;
				}
			}
		}
	}
}

int LevelRenderer::render( Mob* player, int layer, float alpha )
{
	if (mc->options.viewDistance != lastViewDistance) {
		allChanged();
	}

	TIMER_PUSH("sortchunks");
	for (int i = 0; i < 10; i++) {
		chunkFixOffs = (chunkFixOffs + 1) % chunksLength;
		Chunk* c = chunks[chunkFixOffs];
		if (c->isDirty() && !c->queued) {
			c->queued = true;
			dirtyChunks.push_back(c);
		}
	}

	if (layer == 0) {
		totalChunks = 0;
		offscreenChunks = 0;
		occludedChunks = 0;
		renderedChunks = 0;
		emptyChunks = 0;
	}

	float xOff = player->xOld + (player->x - player->xOld) * alpha;
	float yOff = player->yOld + (player->y - player->yOld) * alpha;
	float zOff = player->zOld + (player->z - player->zOld) * alpha;

	float xd = player->x - xOld;
	float yd = player->y - yOld;
	float zd = player->z - zOld;
	if (xd * xd + yd * yd + zd * zd > 4 * 4) {
		xOld = player->x;
		yOld = player->y;
		zOld = player->z;

		resortChunks(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));
		DistanceChunkSorter distanceSorter(player);
		std::sort(sortedChunks, sortedChunks + chunksLength, distanceSorter);
	}

	int count = 0;
	if (occlusionCheck && !mc->options.anaglyph3d && layer == 0) {
		int from = 0;
		int to = 16;
		//checkQueryResults(from, to);
		for (int i = from; i < to; i++) {
			sortedChunks[i]->occlusion_visible = true;
		}

		count += renderChunks(from, to, layer, alpha);

		do {
			from = to;
			to = to * 2;
			if (to > chunksLength) to = chunksLength;

			glDisable2(GL_TEXTURE_2D);
			glDisable2(GL_LIGHTING);
			glDisable2(GL_ALPHA_TEST);
			glDisable2(GL_FOG);

			glColorMask(false, false, false, false);
			glDepthMask(false);
			//checkQueryResults(from, to);
			glPushMatrix2();
			float xo = 0;
			float yo = 0;
			float zo = 0;
			for (int i = from; i < to; i++) {
				if (sortedChunks[i]->isEmpty()) {
					sortedChunks[i]->visible = false;
					continue;
				}
				if (!sortedChunks[i]->visible) {
					sortedChunks[i]->occlusion_visible = true;
				}

				if (sortedChunks[i]->visible && !sortedChunks[i]->occlusion_querying) {
					float dist = Mth::sqrt(sortedChunks[i]->distanceToSqr(player));

					int frequency = (int) (1 + dist / 128);

					if (ticks % frequency == i % frequency) {
						Chunk* chunk = sortedChunks[i];
						float xt = (float) (chunk->x - xOff);
						float yt = (float) (chunk->y - yOff);
						float zt = (float) (chunk->z - zOff);
						float xdd = xt - xo;
						float ydd = yt - yo;
						float zdd = zt - zo;

						if (xdd != 0 || ydd != 0 || zdd != 0) {
							glTranslatef2(xdd, ydd, zdd);
							xo += xdd;
							yo += ydd;
							zo += zdd;
						}

						sortedChunks[i]->renderBB();
						sortedChunks[i]->occlusion_querying = true;
					}
				}
			}
			glPopMatrix2();
			glColorMask(true, true, true, true);
			glDepthMask(true);
			glEnable2(GL_TEXTURE_2D);
			glEnable2(GL_ALPHA_TEST);
			glEnable2(GL_FOG);

			count += renderChunks(from, to, layer, alpha);

		} while (to < chunksLength);

	} else {
		TIMER_POP_PUSH("render");
		count += renderChunks(0, chunksLength, layer, alpha);
	}

	TIMER_POP();
	return count;
}

void LevelRenderer::renderDebug(const AABB& b, float a) const {
	float x0 = b.x0;
	float x1 = b.x1;
	float y0 = b.y0;
	float y1 = b.y1;
	float z0 = b.z0;
	float z1 = b.z1;
	float u0 = 0, v0 = 0;
	float u1 = 1, v1 = 1;

	glEnable2(GL_BLEND);
	glBlendFunc2(GL_DST_COLOR, GL_SRC_COLOR);
	glDisable2(GL_TEXTURE_2D);
	glColor4f2(1, 1, 1, 1);

	textures->loadAndBindTexture("terrain.png");

	Tesselator& t = Tesselator::instance;
	t.begin();
	t.color(255, 255, 255, 255);

	t.offset(((Mob*)mc->player)->getPos(a).negated());

	// up
	t.vertexUV(x0, y0, z1, u0, v1);
	t.vertexUV(x0, y0, z0, u0, v0);
	t.vertexUV(x1, y0, z0, u1, v0);
	t.vertexUV(x1, y0, z1, u1, v1);

	// down
	t.vertexUV(x1, y1, z1, u1, v1);
	t.vertexUV(x1, y1, z0, u1, v0);
	t.vertexUV(x0, y1, z0, u0, v0);
	t.vertexUV(x0, y1, z1, u0, v1);

	// north
	t.vertexUV(x0, y1, z0, u1, v0);
	t.vertexUV(x1, y1, z0, u0, v0);
	t.vertexUV(x1, y0, z0, u0, v1);
	t.vertexUV(x0, y0, z0, u1, v1);

	// south
	t.vertexUV(x0, y1, z1, u0, v0);
	t.vertexUV(x0, y0, z1, u0, v1);
	t.vertexUV(x1, y0, z1, u1, v1);
	t.vertexUV(x1, y1, z1, u1, v0);

	// west
	t.vertexUV(x0, y1, z1, u1, v0);
	t.vertexUV(x0, y1, z0, u0, v0);
	t.vertexUV(x0, y0, z0, u0, v1);
	t.vertexUV(x0, y0, z1, u1, v1);

	// east
	t.vertexUV(x1, y0, z1, u0, v1);
	t.vertexUV(x1, y0, z0, u1, v1);
	t.vertexUV(x1, y1, z0, u1, v0);
	t.vertexUV(x1, y1, z1, u0, v0);

	t.offset(0, 0, 0);
	t.draw();

	glEnable2(GL_TEXTURE_2D);
	glDisable2(GL_BLEND);
}

void LevelRenderer::render(const AABB& b) const
{
	Tesselator& t = Tesselator::instance;

	glColor4f2(1, 1, 1, 1);

	textures->loadAndBindTexture("terrain.png");

	//t.begin();
	t.color(255, 255, 255, 255);

	t.offset(((Mob*)mc->player)->getPos(0).negated());

	t.begin(GL_LINE_STRIP);
	t.vertex(b.x0, b.y0, b.z0);
	t.vertex(b.x1, b.y0, b.z0);
	t.vertex(b.x1, b.y0, b.z1);
	t.vertex(b.x0, b.y0, b.z1);
	t.vertex(b.x0, b.y0, b.z0);
	t.draw();

	t.begin(GL_LINE_STRIP);
	t.vertex(b.x0, b.y1, b.z0);
	t.vertex(b.x1, b.y1, b.z0);
	t.vertex(b.x1, b.y1, b.z1);
	t.vertex(b.x0, b.y1, b.z1);
	t.vertex(b.x0, b.y1, b.z0);
	t.draw();

	t.begin(GL_LINES);
	t.vertex(b.x0, b.y0, b.z0);
	t.vertex(b.x0, b.y1, b.z0);
	t.vertex(b.x1, b.y0, b.z0);
	t.vertex(b.x1, b.y1, b.z0);
	t.vertex(b.x1, b.y0, b.z1);
	t.vertex(b.x1, b.y1, b.z1);
	t.vertex(b.x0, b.y0, b.z1);
	t.vertex(b.x0, b.y1, b.z1);

	t.offset(0, 0, 0);
	t.draw();
}

//void LevelRenderer::checkQueryResults( int from, int to )
//{
//	for (int i = from; i < to; i++) {
//		if (sortedChunks[i]->occlusion_querying) {
//			// I wanna do a fast occusion culler here.
//		}
//	}
//}

int LevelRenderer::renderChunks( int from, int to, int layer, float alpha )
{
	_renderChunks.clear();
	int count = 0;
	for (int i = from; i < to; i++) {
		if (layer == 0) {
			totalChunks++;
			if (sortedChunks[i]->empty[layer]) emptyChunks++;
			else if (!sortedChunks[i]->visible) offscreenChunks++;
			else if (occlusionCheck && !sortedChunks[i]->occlusion_visible) occludedChunks++;
			else renderedChunks++;
		}

		if (!sortedChunks[i]->empty[layer] && sortedChunks[i]->visible && sortedChunks[i]->occlusion_visible) {
			int list = sortedChunks[i]->getList(layer);
			if (list >= 0) {
				_renderChunks.push_back(sortedChunks[i]);
				count++;
			}
		}
	}

	float xOff, yOff, zOff;
	if (_scriptCam) {
		xOff = _scX;
		yOff = _scY;
		zOff = _scZ;
	} else {
		Mob* player = mc->cameraTargetPlayer;
		xOff = player->xOld + (player->x - player->xOld) * alpha;
		yOff = player->yOld + (player->y - player->yOld) * alpha;
		zOff = player->zOld + (player->z - player->zOld) * alpha;
	}

	//int lists = 0;
	renderList.clear();
	renderList.init(xOff, yOff, zOff);

#if defined(MACOS) || defined(LINUX) || defined(WIN32)
	// 植被摆动(layer3): 渲染前对可视植被 chunk 做正弦顶点偏移并重传 VBO
	if (layer == 3) {
		float tSec = (float)(getTimeMs() * 0.001);
		for (unsigned int i = 0; i < _renderChunks.size(); ++i)
			_renderChunks[i]->swayUpdate(tSec, xOff, zOff);
	}
#endif

	for (unsigned int i = 0; i < _renderChunks.size(); ++i) {
		Chunk* chunk = _renderChunks[i];
		#ifdef USE_VBO
			renderList.addR(chunk->getRenderChunk(layer));
		#else
			renderList.add(chunk->getList(layer));
		#endif
		renderList.next();
	}

	renderSameAsLast(layer, alpha);

	if (layer == 0)
		FrameProf::visibleChunks() = count;   // diagnostics (F3 overlay only)

	return count;
}

void LevelRenderer::renderSameAsLast( int layer, float alpha )
{
	renderList.render();
}

void LevelRenderer::setScriptedCameraOrigin(float x, float y, float z) {
	_scriptCam = true;
	_scX = x;
	_scY = y;
	_scZ = z;
}

void LevelRenderer::clearScriptedCameraOrigin() {
	_scriptCam = false;
}

void LevelRenderer::forceOcclusionVisibleAll() {
	for (int i = 0; i < chunksLength; i++) {
		if (chunks[i] && !chunks[i]->isEmpty())
			chunks[i]->occlusion_visible = true;
	}
}
void LevelRenderer::tick()
{
	ticks++;
}

bool LevelRenderer::updateDirtyChunks( Mob* player, bool force )
{
	FrameProf::dirtyCalls()++;   // diagnostics (F3 only): calls per frame
	bool slow = false;

	if (slow) {
		DirtyChunkSorter dirtySorter(player);
		std::sort(dirtyChunks.begin(), dirtyChunks.end(), dirtySorter);
		int s = dirtyChunks.size() - 1;
		int amount = dirtyChunks.size();
		for (int i = 0; i < amount; i++) {
			Chunk* chunk = dirtyChunks[s-i];
			if (!force) {
				if (chunk->distanceToSqr(player) > 16 * 16) {
					if (chunk->visible) {
						if (i >= MAX_VISIBLE_REBUILDS_PER_FRAME) return false;
					} else {
						if (i >= MAX_INVISIBLE_REBUILDS_PER_FRAME) return false;
					}
				}
			} else {
				if (!chunk->visible) continue;
			}
			chunk->rebuild();

			dirtyChunks.erase( std::find(dirtyChunks.begin(), dirtyChunks.end(), chunk) ); // @q: s-i?
			chunk->setClean();
		}

		return dirtyChunks.size() == 0;
	} else {
		// ── 帧级重建预算（同一帧内的多次调用共享）─────────────────────────
		// updateDirtyChunks 一帧内会被调用多次（离屏场景渲染 / 水面反射 / 主视图
		// 各一次）。如果每次调用各自计时（旧行为：每次 8.3ms），每帧重建总量就会
		// 翻 2-3 倍——实测每帧 9.4 块 ≈ 20.3ms，把帧时间顶到 25.8ms（39fps）。
		//
		// 所以预算按“帧”算（用主循环递增的 g_frameStamp 识别新帧），并按上一帧
		// 的实际 update 耗时自适应：超了就少做，有余量就多做——既稳住 60fps，
		// 也不白浪费帧预算（地形该多快就多快）。
		if (s_budgetStamp != g_frameStamp) {
			s_budgetStamp = g_frameStamp;
			s_budgetStart = getTimeS();
			double lastMs = g_diagUpdateMs;
			if (lastMs > 15.0)      s_budgetMs *= 0.80f;
			else if (lastMs < 11.0) s_budgetMs *= 1.15f;
			if (s_budgetMs < 1.5f)  s_budgetMs = 1.5f;
			if (s_budgetMs > 8.0f)  s_budgetMs = 8.0f;
		}
		static const int   MaxChunksPerFrame = 32;

		// 最近的优先重建（DirtyChunkSorter 是"远的在前"，所以最近的在尾部）。
		//
		// 注意：切换世界 / 改视距 / 切 AO 会触发 allChanged()，把整个视距内的
		// chunk 全部标脏（实测队列可达 5000+）。对这么大的队列每帧做一次
		// std::sort 是 O(n log n)，且比较器每次都要算到玩家的距离 —— 在 /Od
		// 下这步本身就能吃掉十几毫秒，而 MaxFrameTime 预算只约束下面的重建循环
		// （不含排序），于是帧时间远超预算。
		//
		// 每帧最多也只重建 MaxChunksPerFrame 个，因此队列很大时只需用
		// nth_element（O(n)）保证"最近的一批"落在尾部即可，无需整体有序。
		// 距离变化很慢，不必每帧对队列重排：隔几帧重排一次即可。pop_back 只
		// 移除尾部元素，不影响其余元素的相对顺序；新标脏的 chunk 落在尾部，
		// 正好是原有的“最近优先”（LIFO）。
		static int s_dirtySortCountdown = 0;
		if (s_dirtySortCountdown <= 0) {
			s_dirtySortCountdown = 4;
			double _s0 = FrameProf::now();
			DirtyChunkSorter dirtyChunkSorter(player);
			const size_t rebuildCap = (size_t)MaxChunksPerFrame;
			if (dirtyChunks.size() > rebuildCap)
				std::nth_element(dirtyChunks.begin(), dirtyChunks.end() - rebuildCap,
				                 dirtyChunks.end(), dirtyChunkSorter);
			else
				std::sort(dirtyChunks.begin(), dirtyChunks.end(), dirtyChunkSorter);
			FrameProf::add(FrameProf::SEC_DIRTY_SORT, _s0, FrameProf::now());
		} else {
			--s_dirtySortCountdown;
		}

		Stopwatch chunkWatch;
		chunkWatch.start();
		double _r0 = FrameProf::now();

		// ── 邻居预热（受同一帧预算）───────────────────────────────────────
		// Region::reset 会对 3×3 邻居调 Level::getChunk；尚未加载的邻居会当场
		// 生成/读盘（实测约 0.27ms/个），于是每块重建要 2.46ms，而帧预算只够
		// 3 块 —— 7000 块的队列要几十秒才清完，期间帧率一直波动（用户感知就是
		// "依旧卡、波动大"）。
		// 这里先按"离玩家最近优先"把待重建块的邻居批量取进 chunk 缓存，重建
		// 阶段便只命中缓存快路径（每块约 0.1ms），队列几十帧内即可清空。加载
		// 成本并没有消失，只是被摊到同一个帧预算里、且不再重复付账。
		{
			const size_t dn = dirtyChunks.size();
			int warmBudget = 24;                      // 每帧最多预热多少块
			for (size_t i = 0; i < dn && warmBudget > 0; ++i) {
				double warmUsedMs = (getTimeS() - s_budgetStart) * 1000.0;
				if (warmUsedMs >= (double)s_budgetMs) break;
				Chunk* wc = dirtyChunks[dn - 1 - i];  // 队尾 = 离玩家最近
				const int ccx = wc->x >> 4;
				const int ccz = wc->z >> 4;
				for (int dx = -1; dx <= 1; ++dx)
					for (int dz = -1; dz <= 1; ++dz)
						level->getChunk(ccx + dx, ccz + dz);
				--warmBudget;
			}
		}

		int rebuilt = 0;
		while (!dirtyChunks.empty()) {
			Chunk* chunk = dirtyChunks.back();
			if (force && !chunk->visible) {
				dirtyChunks.pop_back();
				chunk->setClean();
				continue;
			}
			chunkWatch.stopContinue();
			double frameUsedMs = (getTimeS() - s_budgetStart) * 1000.0;   // 本帧已用
			if (frameUsedMs >= (double)s_budgetMs || rebuilt >= MaxChunksPerFrame) break;
			chunk->rebuild();
			chunk->setClean();
			dirtyChunks.pop_back();
			rebuilt++;
		}

		FrameProf::add(FrameProf::SEC_DIRTY_REBUILD, _r0, FrameProf::now());
		FrameProf::rebuiltAcc() += rebuilt;             // diagnostics (F3 only)
		FrameProf::dirtyQueue()  = (int)dirtyChunks.size();

		return dirtyChunks.empty();
	}
}

void LevelRenderer::renderHit( Player* player, const HitResult& h, int mode, /*ItemInstance*/void* inventoryItem, float a )
{
	if (mode == 0) {
		if (destroyProgress > 0) {
			Tesselator& t = Tesselator::instance;
			glEnable2(GL_BLEND);
			glBlendFunc2(GL_DST_COLOR, GL_SRC_COLOR);

			textures->loadAndBindTexture("terrain.png");
			glPushMatrix2();

			int tileId = level->getTile(h.x, h.y, h.z);
			Tile* tile = tileId > 0 ? Tile::tiles[tileId] : NULL;
			//glDisable2(GL_ALPHA_TEST);

			glPolygonOffset(-3.0f, -3.0f);
			glEnable2(GL_POLYGON_OFFSET_FILL);
			t.begin();
			t.color(1.0f, 1.0f, 1.0f, 0.5f);
			t.noColor();
			float xo = player->xOld + (player->x - player->xOld) * a;
			float yo = player->yOld + (player->y - player->yOld) * a;
			float zo = player->zOld + (player->z - player->zOld) * a;

			t.offset(-xo, -yo, -zo);
			//t.noColor();

			if (tile == NULL) tile = Tile::rock;
			const int progress = (int) (destroyProgress * 10);
			tileRenderer->tesselateInWorld(tile, h.x, h.y, h.z, 15 * 16 + progress);

			t.draw();
			t.offset(0, 0, 0);
			glPolygonOffset(0.0f, 0.0f);
			glDisable2(GL_POLYGON_OFFSET_FILL);
			//glDisable2(GL_ALPHA_TEST);
			glDisable2(GL_BLEND);

			glDepthMask(true);
			glPopMatrix2();
		}
	}
	//else if (inventoryItem != NULL) {
	//          glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//          float br = ((float) (util.Mth::sin(System.currentTimeMillis() / 100.0f)) * 0.2f + 0.8f);
	//          glColor4f2(br, br, br, ((float) (util.Mth::sin(System.currentTimeMillis() / 200.0f)) * 0.2f + 0.5f));

	//          int id = textures.loadTexture("terrain.png");
	//          glBindTexture2(GL_TEXTURE_2D, id);
	//          int x = h.x;
	//          int y = h.y;
	//          int z = h.z;
	//          if (h.f == 0) y--;
	//          if (h.f == 1) y++;
	//          if (h.f == 2) z--;
	//          if (h.f == 3) z++;
	//          if (h.f == 4) x--;
	//          if (h.f == 5) x++;
	//          /*
	//           * t.begin(); t.noColor(); Tile.tiles[tileType].tesselate(level, x,
	//           * y, z, t); t.end();
	//           */
	//      }
}

void LevelRenderer::renderHitOutline( Player* player, const HitResult& h, int mode, /*ItemInstance*/void* inventoryItem, float a )
{
	if (mode == 0 && h.type == TILE) {
		glEnable2(GL_BLEND);
		glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f2(0, 0, 0, 0.4f);
		glLineWidth(1.0f);
		glDisable2(GL_TEXTURE_2D);
		glDepthMask(false);
		float ss = 0.002f;
		int tileId = level->getTile(h.x, h.y, h.z);

		if (tileId > 0 && Tile::tiles[tileId] != NULL) {
			Tile::tiles[tileId]->updateShape(level, h.x, h.y, h.z);
			float xo = player->xOld + (player->x - player->xOld) * a;
			float yo = player->yOld + (player->y - player->yOld) * a;
			float zo = player->zOld + (player->z - player->zOld) * a;
			render(Tile::tiles[tileId]->getTileAABB(level, h.x, h.y, h.z).grow(ss, ss, ss).cloneMove(-xo, -yo, -zo));
		}
		glDepthMask(true);
		glEnable2(GL_TEXTURE_2D);
		glDisable2(GL_BLEND);
	}
}

void LevelRenderer::setDirty( int x0, int y0, int z0, int x1, int y1, int z1 )
{
	int _x0 = Mth::intFloorDiv(x0, CHUNK_SIZE);
	int _y0 = Mth::intFloorDiv(y0, CHUNK_SIZE);
	int _z0 = Mth::intFloorDiv(z0, CHUNK_SIZE);
	int _x1 = Mth::intFloorDiv(x1, CHUNK_SIZE);
	int _y1 = Mth::intFloorDiv(y1, CHUNK_SIZE);
	int _z1 = Mth::intFloorDiv(z1, CHUNK_SIZE);

	for (int x = _x0; x <= _x1; x++) {
		int xx = x % xChunks;
		if (xx < 0) xx += xChunks;
		for (int y = _y0; y <= _y1; y++) {
			int yy = y % yChunks;
			if (yy < 0) yy += yChunks;
			for (int z = _z0; z <= _z1; z++) {
				int zz = z % zChunks;
				if (zz < 0) zz += zChunks;

				int p = ((zz) * yChunks + (yy)) * xChunks + (xx);
				Chunk* chunk = chunks[p];
				if (!chunk->isDirty()) {
					dirtyChunks.push_back(chunk);
					chunk->setDirty();
					FrameProf::dirtyMarks()++;   // diagnostics (F3 overlay only)
				}
			}
		}
	}
}

void LevelRenderer::tileChanged( int x, int y, int z)
{
	setDirty(x - 1, y - 1, z - 1, x + 1, y + 1, z + 1);
}


void LevelRenderer::setTilesDirty( int x0, int y0, int z0, int x1, int y1, int z1 )
{
	setDirty(x0 - 1, y0 - 1, z0 - 1, x1 + 1, y1 + 1, z1 + 1);
}


void LevelRenderer::cull( Culler* culler, float a )
{
	for (int i = 0; i < chunksLength; i++) {
		if (!chunks[i]->isEmpty()) {
			if (!chunks[i]->visible || ((i + cullStep) & 15) == 0) {
				chunks[i]->cull(culler);
			}
		}
	}
	cullStep++;
}

void LevelRenderer::skyColorChanged()
{
	for (int i = 0; i < chunksLength; i++) {
		if (chunks[i]->skyLit) {
			if (!chunks[i]->isDirty()) {
				chunks[i]->queued = true;
				dirtyChunks.push_back(chunks[i]);
				chunks[i]->setDirty();
			}
		}
	}
}

bool entityRenderPredicate(const Entity* a, const Entity* b) {
	return a->entityRendererId < b->entityRendererId;
}

void LevelRenderer::renderEntities(Vec3 cam, Culler* culler, float a) {
    if (noEntityRenderFrames > 0) {
        noEntityRenderFrames--;
        return;
    }

	if (mc->cameraTargetPlayer == NULL) {
		return;
	}

	// 模组动画方块（放在实体之前画：地形层已经画完了）。
	renderAnimatedBlocks(a);

	TIMER_PUSH("prepare");
    TileEntityRenderDispatcher::getInstance()->prepare(level, textures, mc->font, mc->cameraTargetPlayer, a);
    EntityRenderDispatcher::getInstance()->prepare(level, mc->font, mc->cameraTargetPlayer, &mc->options, a);

    totalEntities = 0;
    renderedEntities = 0;
    culledEntities = 0;

	Entity* player = mc->cameraTargetPlayer;
    EntityRenderDispatcher::xOff = TileEntityRenderDispatcher::xOff = (player->xOld + (player->x - player->xOld) * a);
    EntityRenderDispatcher::yOff = TileEntityRenderDispatcher::yOff = (player->yOld + (player->y - player->yOld) * a);
    EntityRenderDispatcher::zOff = TileEntityRenderDispatcher::zOff = (player->zOld + (player->z - player->zOld) * a);

	glEnableClientState2(GL_VERTEX_ARRAY);
	glEnableClientState2(GL_TEXTURE_COORD_ARRAY);

	TIMER_POP_PUSH("entities");
	const EntityList& entities = level->getAllEntities();
	totalEntities = entities.size();
	if (totalEntities > 0) {
		Entity** toRender = new Entity*[totalEntities];
		for (int i = 0; i < totalEntities; i++) {
			Entity* entity = entities[i];

				// Expand BB slightly so entities at the frustum edge aren't incorrectly culled
			if (entity->shouldRender(cam) && culler->isVisible(entity->bb.grow(0.5f, 0.5f, 0.5f)))
			{
				if (entity == mc->cameraTargetPlayer && mc->options.thirdPersonView == 0 && mc->cameraTargetPlayer->isPlayer() && !((Player*)mc->cameraTargetPlayer)->isSleeping()) continue;
				if (entity == mc->cameraTargetPlayer && !mc->options.thirdPersonView)
					continue;
				if (!level->hasChunkAt(Mth::floor(entity->x), Mth::floor(entity->y), Mth::floor(entity->z)))
					continue;

				toRender[renderedEntities++] = entity;
				//EntityRenderDispatcher::getInstance()->render(entity, a);
			}
		}

		if (renderedEntities > 0) {
			std::sort(&toRender[0], &toRender[renderedEntities], entityRenderPredicate);
			for (int i = 0; i < renderedEntities; ++i) {
				EntityRenderDispatcher* disp = EntityRenderDispatcher::getInstance();
				disp->render(toRender[i], a);
			}
		}

		delete[] toRender;
	}

    TIMER_POP_PUSH("tileentities");
    for (unsigned int i = 0; i < level->tileEntities.size(); i++) {
        TileEntityRenderDispatcher::getInstance()->render(level->tileEntities[i], a);
    }

	glDisableClientState2(GL_VERTEX_ARRAY);
	glDisableClientState2(GL_TEXTURE_COORD_ARRAY);

	TIMER_POP();
}

std::string LevelRenderer::gatherStats1() {
	std::stringstream ss;
	ss << "C: " << renderedChunks << "/" << totalChunks << ". F: " << offscreenChunks << ", O: " << occludedChunks << ", E: " << emptyChunks << "\n";
    return ss.str();
}

//
//    /*public*/ std::string gatherStats2() {
//        return "E: " + renderedEntities + "/" + totalEntities + ". B: " + culledEntities + ", I: " + ((totalEntities - culledEntities) - renderedEntities);
//    }
//
//    int[] toRender = new int[50000];
//    IntBuffer resultBuffer = MemoryTracker.createIntBuffer(64);

// 照抄 Minecraft 1.6.4 RenderGlobal.renderSky 的顺序：
//   ① 天幕（受雾，用天空色铺满）→ 画完立刻关雾
//   ② 开混合，画日出/日落扇形
//   ③ 画太阳 + 月亮
//   ④ 关纹理，画星空
//   收尾把混合/alpha test/雾恢复回去。
void LevelRenderer::renderSky(float alpha) {
	// 1.6.4 用 provider.dimensionId/isSurfaceWorld 判断；本工程对应的是 foggy
	// （地狱等没有天空的维度）。
	if (mc->level->dimension->foggy) return;

	glDepthMask(0);

	// ① 天幕
	glEnable2(GL_FOG);
	Vec3 skyColor = level->getSkyColor(mc->cameraTargetPlayer, alpha);
	glColor4f2(skyColor.x, skyColor.y, skyColor.z, 1.0f);
	renderPosOnlyMesh(skyMesh);
	glDisable2(GL_FOG);            // 1.6.4：天幕之后立刻就关雾

	// ② 之后的日月星辰一律在混合打开的状态下画
	glDisable2(GL_ALPHA_TEST);
	glEnable2(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	renderSunriseFan(alpha);       // 日出/日落扇形
	renderSunOrMoon(alpha);        // 太阳 + 月亮
	renderStars(alpha);            // 星空

	// 收尾（1.6.4 的 renderSky 末尾）
	glColor4f2(1.0f, 1.0f, 1.0f, 1.0f);
	glDisable2(GL_BLEND);
	glEnable2(GL_ALPHA_TEST);
	glEnable2(GL_FOG);
	glDepthMask(1);
}

// 照抄 Multiplatform-1.6.4（modifiedeight）LevelRenderer::renderClouds。
//
// 云不是贴在天上的平面图，而是把 clouds.png 的 alpha 通道当成"哪里有一朵云"
// 的高度图，逐格挤出成立体的云块（TextureTesselator），再用 16x7x16 的缩放、
// 自建透视矩阵和跟着云层高度走的雾画出来。
// 模组动画方块：这些方块不进区块网格（姿态每帧在变，烘焙进 VBO 就动不了），
// 每帧在主线程按当前姿态直接画一遍。放在地形层之后、实体之前。
void LevelRenderer::renderAnimatedBlocks(float a) {
	ModEngine* me = ModEngine::instance;
	if (!me || !level)
		return;
	const std::vector<ModEngine::AnimBlockRef>& list = me->animatedBlocks();
	if (list.empty())
		return;
	Mob* cam = mc->cameraTargetPlayer;
	if (!cam)
		return;

	// 与地形同样的平移原点（相机位置的插值）
	float xOff = cam->xOld + (cam->x - cam->xOld) * a;
	float yOff = cam->yOld + (cam->y - cam->yOld) * a;
	float zOff = cam->zOld + (cam->z - cam->zOld) * a;

	Tesselator& t = Tesselator::instance;
	TileRenderer tr(level);
	bool started = false;
	const float maxDist2 = 96.0f * 96.0f;

	glPushMatrix2();
	glTranslatef2(-xOff, -yOff, -zOff);
	for (size_t i = 0; i < list.size(); ++i) {
		const ModEngine::AnimBlockRef& r = list[i];
		if (level->getTile(r.x, r.y, r.z) != r.tileId)
			continue;   // 已经被拆掉/换成别的方块了
		float dx = (float)r.x + 0.5f - cam->x;
		float dy = (float)r.y + 0.5f - cam->y;
		float dz = (float)r.z + 0.5f - cam->z;
		if (dx * dx + dy * dy + dz * dz > maxDist2)
			continue;
		Tile* tile = Tile::tiles[r.tileId];
		if (!tile)
			continue;
		if (!started) {
			started = true;
			t.begin();
			t.offset(0.0f, 0.0f, 0.0f);
		}
		tr.tesselateModelInWorld(tile, r.x, r.y, r.z);
	}
	if (started)
		t.draw();
	glPopMatrix2();
}

void LevelRenderer::renderClouds( float alpha ) {
	// Dimensions can disable sky clouds (e.g. the Aether - it's above
	// the clouds; the Nether's sky is red/foggy). Scripted dimensions
	// control this via Dimension.define(id, { clouds: false }).
	if (!mc->level->dimension->hasClouds) return;

	// 玩家位置 / 云层高度（照抄：云跟着玩家所在格平移，高度取维度的 getCloudHeight）
	// 本工程的 Entity 位置字段是 x/y/z（当前）与 xOld/yOld/zOld（上一帧）。
	float playerY = mc->player->yOld + (mc->player->y - mc->player->yOld) * alpha;
	float cloudHeight = level->dimension->getCloudHeight();
	float yOffs = (cloudHeight - playerY) + 0.33f;
	float playerZ = mc->player->zOld + (mc->player->z - mc->player->zOld) * alpha;

	float xInt, zInt;
	float xFrac = modff((mc->player->xOld + (mc->player->x - mc->player->xOld) * alpha
	                     + (float) (ticks + alpha) * 0.03f) * 0.0625f, &xInt);
	float zFrac = modff(playerZ * 0.0625f, &zInt);
	int texX = (int) xInt;
	int texZ = (int) zInt;

	// 云色（参考项目是 Color4，这里用 4 个 float——gui08 里有个同名 Color4 类，
	// 实例化它会跟本工程的 Color4 撞符号，见 TextureTesselator.h 的注释）
	Vec3 cloudColorVec = level->getCloudColor(alpha);
	float cloudR = cloudColorVec.x;
	float cloudG = cloudColorVec.y;
	float cloudB = cloudColorVec.z;
	float cloudA = 0.7f;   // 参考项目 v30.a = 0.7

	// 云网格缓存：格子坐标或云色变了才重建（照抄 field_15C / field_160 / field_164）
	if (texX != cloudMeshTexX || texZ != cloudMeshTexY || !cloudsMesh.isValid() ||
	    fabsf(cloudR - cloudMeshColorR) + fabsf(cloudG - cloudMeshColorG) + fabsf(cloudB - cloudMeshColorB) > 0.06f) {
		TextureData* cloudTex = textures->loadAndGetTextureData("environment/clouds.png");
		if (cloudTex == NULL) return;
		TextureTesselator tesselator(cloudTex, texX - 32, texZ - 32, texX + 32, texZ + 32,
		                             Vec3(0.0f, 0.70711f, 0.70711f),
		                             0.6f, 0.6f, 0.6f, 1.0f,     // field_1C
		                             cloudR, cloudG, cloudB, cloudA);
		cloudsMesh = tesselator.tesselate();
		cloudMeshTexX = texX;
		cloudMeshTexY = texZ;
		cloudMeshColorR = cloudR;
		cloudMeshColorG = cloudG;
		cloudMeshColorB = cloudB;
		cloudMeshColorA = cloudA;
	}

	// EnableState(2912=GL_FOG) / DisableState(3553=GL_TEXTURE_2D) /
	// EnableClientState(32886=GL_COLOR_ARRAY) / DisableState(blend)：
	// 进入时改状态，出作用域再还原。
	glEnable2(GL_FOG);
	glDisable2(GL_TEXTURE_2D);
	// 云的半透明靠顶点色的 alpha（0.7）；参考项目里混合也是调用者开着的，
	// 这里自己保证打开。低画质时参考项目会把混合关掉（DisableState(graphics?0:3042)）。
	glEnable2(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (!mc->options.fancyGraphics) glDisable2(GL_BLEND);

	float fogBase = mc->gameRenderer->getRenderDistance();   // 参考项目里的 GameRenderer::field_8
	float fogEnd = sqrtf(yOffs * yOffs + 262140.0f);
	glFogf2(GL_FOG_START, 0.0f);
	glFogf2(GL_FOG_END, fogEnd);
	if (yOffs > 1.0f) glDepthRange(0.0, 1.0);

	glPushMatrix2();
	glTranslatef2(-(xFrac * 16.0f), yOffs, -(zFrac * 16.0f));
	glScalef2(16.0f, 7.0f, 16.0f);
	glTranslatef2(-32.0f, 0.0f, -32.0f);

	// 云用自己的投影矩阵（近平面 2.0、远平面跟着雾）
	glMatrixMode(GL_PROJECTION);
	glPushMatrix2();
	glLoadIdentity2();
	float fov = mc->gameRenderer->getFov(alpha, 1);
	gluPerspective(fov, (float) mc->width / (float) mc->height, 2.0f, fogEnd);

	glEnableClientState2(GL_VERTEX_ARRAY);
	glEnableClientState2(GL_COLOR_ARRAY);
	cloudsMesh.render();
	glDisableClientState2(GL_COLOR_ARRAY);
	glDisableClientState2(GL_VERTEX_ARRAY);

	glPopMatrix2();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix2();

	glFogf2(GL_FOG_START, fogBase * 0.7f);
	glFogf2(GL_FOG_END, fogBase);
	if (yOffs > 1.0f) glDepthRange(0.0, 0.7);

	// 还原：纹理开、混合开（低画质时本来就没开）、雾关
	glEnable2(GL_TEXTURE_2D);
	if (!mc->options.fancyGraphics) glEnable2(GL_BLEND);
	glDisable2(GL_FOG);
}

void LevelRenderer::playSound(const std::string& name, float x, float y, float z, float volume, float pitch) {
	// @todo: deny sounds here if sound is off (rather than waiting 'til SoundEngine)
	// 切维度/进世界期间相机玩家可能还没就绪（beginDimensionTravel 刚 setLevel），
	// 这时新维度里的生物已经在 tick 走路并播脚步声 —— 少了这道判断就是空指针崩溃。
	if (!mc->cameraTargetPlayer) return;
	float dd = 16;

    if (volume > 1) dd *= volume;
    if (mc->cameraTargetPlayer->distanceToSqr(x, y, z) < dd * dd) {
        mc->soundEngine->play(name, x, y, z, volume, pitch);
    }
}

void LevelRenderer::addParticle(const std::string& name, float x, float y, float z, float xa, float ya, float za, int data) {

	if (!mc->cameraTargetPlayer) return;   // 同上：加载期间相机玩家可能还没就绪
	float xd = mc->cameraTargetPlayer->x - x;
    float yd = mc->cameraTargetPlayer->y - y;
    float zd = mc->cameraTargetPlayer->z - z;
	float distanceSquared = xd * xd + yd * yd + zd * zd;

	//Particle* p = NULL;
	//if (name == "hugeexplosion") p = new HugeExplosionSeedParticle(level, x, y, z, xa, ya, za);
	//else if (name == "largeexplode") p = new HugeExplosionParticle(textures, level, x, y, z, xa, ya, za);

	//if (p) {
	//	if (distanceSquared < 32 * 32) {
	//		mc->particleEngine->add(p);
	//	} else { delete p; }
	//	return;
	//}

    const float particleDistance = 16;
    if (distanceSquared > particleDistance * particleDistance) return;

	//static Stopwatch sw;
	//sw.start();

    if (name == "bubble") mc->particleEngine->add(new BubbleParticle(level, x, y, z, xa, ya, za));
	else if (name == "crit") mc->particleEngine->add(new CritParticle2(level, x, y, z, xa, ya, za));
	else if (name == "smoke") mc->particleEngine->add(new SmokeParticle(level, x, y, z, xa, ya, za));
    //else if (name == "note") mc->particleEngine->add(new NoteParticle(level, x, y, z, xa, ya, za));
    else if (name == "explode") mc->particleEngine->add(new ExplodeParticle(level, x, y, z, xa, ya, za));
    else if (name == "flame") mc->particleEngine->add(new FlameParticle(level, x, y, z, xa, ya, za));
    else if (name == "lava") mc->particleEngine->add(new LavaParticle(level, x, y, z));
    //else if (name == "splash") mc->particleEngine->add(new SplashParticle(level, x, y, z, xa, ya, za));
	else if (name == "largesmoke") mc->particleEngine->add(new SmokeParticle(level, x, y, z, xa, ya, za, 2.5f));
    else if (name == "reddust") mc->particleEngine->add(new RedDustParticle(level, x, y, z, xa, ya, za));
	else if (name == "iconcrack") { Item* ip = (data >= 0 && data < Item::MAX_ITEMS) ? Item::items[data] : NULL; if (ip) mc->particleEngine->add(new BreakingItemParticle(level, x, y, z, xa, ya, za, ip)); }
	else if (name == "snowballpoof") mc->particleEngine->add(new BreakingItemParticle(level, x, y, z, Item::snowBall));
    //else if (name == "snowballpoof") mc->particleEngine->add(new BreakingItemParticle(level, x, y, z, Item::snowBall));
    //else if (name == "slime") mc->particleEngine->add(new BreakingItemParticle(level, x, y, z, Item::slimeBall));
    //else if (name == "heart") mc->particleEngine->add(new HeartParticle(level, x, y, z, xa, ya, za));

	//sw.stop();
	//sw.printEvery(50, "add-particle-string");
}

/*
void LevelRenderer::addParticle(ParticleType::Id name, float x, float y, float z, float xa, float ya, float za, int data) {
	float xd = mc->cameraTargetPlayer->x - x;
	float yd = mc->cameraTargetPlayer->y - y;
	float zd = mc->cameraTargetPlayer->z - z;

	const float particleDistance = 16;
	if (xd * xd + yd * yd + zd * zd > particleDistance * particleDistance) return;

	//static Stopwatch sw;
	//sw.start();

	//Particle* p = NULL;

	if (name == ParticleType::bubble)		mc->particleEngine->add( new BubbleParticle(level, x, y, z, xa, ya, za) );
	else if (name == ParticleType::crit)		mc->particleEngine->add(new CritParticle2(level, x, y, z, xa, ya, za) );
	else if (name == ParticleType::smoke)		mc->particleEngine->add(new SmokeParticle(level, x, y, z, xa, ya, za) );
	else if (name == ParticleType::explode)		mc->particleEngine->add( new ExplodeParticle(level, x, y, z, xa, ya, za) );
	else if (name == ParticleType::flame)		mc->particleEngine->add( new FlameParticle(level, x, y, z, xa, ya, za) );
	else if (name == ParticleType::lava)		mc->particleEngine->add( new LavaParticle(level, x, y, z) );
	else if (name == ParticleType::largesmoke)	mc->particleEngine->add( new SmokeParticle(level, x, y, z, xa, ya, za, 2.5f) );
	else if (name == ParticleType::reddust)		mc->particleEngine->add( new RedDustParticle(level, x, y, z, xa, ya, za) );
	else if (name == ParticleType::iconcrack) { Item* ip = (data >= 0 && data < Item::MAX_ITEMS) ? Item::items[data] : NULL; if (ip) mc->particleEngine->add( new BreakingItemParticle(level, x, y, z, xa, ya, za, ip) ); }

	//switch (name) {
	//	case ParticleType::bubble:		p = new BubbleParticle(level, x, y, z, xa, ya, za); break;
	//	case ParticleType::crit:		p = new CritParticle2(level, x, y, z, xa, ya, za); break;
	//	case ParticleType::smoke:		p = new SmokeParticle(level, x, y, z, xa, ya, za); break;
	//	//case ParticleType::note: p = new NoteParticle(level, x, y, z, xa, ya, za); break;
	//	case ParticleType::explode:		p = new ExplodeParticle(level, x, y, z, xa, ya, za); break;
	//	case ParticleType::flame:		p = new FlameParticle(level, x, y, z, xa, ya, za); break;
	//	case ParticleType::lava:		p = new LavaParticle(level, x, y, z); break;
	//	//case ParticleType::splash: p = new SplashParticle(level, x, y, z, xa, ya, za); break;
	//	case ParticleType::largesmoke:	p = new SmokeParticle(level, x, y, z, xa, ya, za, 2.5f); break;
	//	case ParticleType::reddust:		p = new RedDustParticle(level, x, y, z, xa, ya, za); break;
	//	case ParticleType::iconcrack:	p = new BreakingItemParticle(level, x, y, z, xa, ya, za, Item::items[data]); break;
	//	//case ParticleType::snowballpoof: p = new BreakingItemParticle(level, x, y, z, Item::snowBall); break;
	//	//case ParticleType::slime: p = new BreakingItemParticle(level, x, y, z, Item::slimeBall); break;
	//	//case ParticleType::heart: p = new HeartParticle(level, x, y, z, xa, ya, za); break;
	//	default:
	//		LOGW("Couldn't find particle of type: %d\n", name);
	//		break;
	//}
	//if (p) {
	//	mc->particleEngine->add(p);
	//}

	//sw.stop();
	//sw.printEvery(50, "add-particle-enum");
}
*/

void LevelRenderer::renderHitSelect( Player* player, const HitResult& h, int mode, /*ItemInstance*/void* inventoryItem, float a )
{
	//if (h.type == TILE) LOGI("type: %s @ (%d, %d, %d)\n", Tile::tiles[level->getTile(h.x, h.y, h.z)]->getDescriptionId().c_str(), h.x, h.y, h.z);

	if (mode == 0) {

		Tesselator& t = Tesselator::instance;
		glEnable2(GL_BLEND);
		glDisable2(GL_TEXTURE_2D);
		glBlendFunc2(GL_SRC_ALPHA, GL_ONE);
		glBlendFunc2(GL_DST_COLOR, GL_SRC_COLOR);
		glEnable2(GL_DEPTH_TEST);

		textures->loadAndBindTexture("terrain.png");
		
		int tileId = level->getTile(h.x, h.y, h.z);
		Tile* tile = tileId > 0 ? Tile::tiles[tileId] : NULL;
		glDisable2(GL_ALPHA_TEST);

		//LOGI("block: %d - %d (%s)\n", tileId, level->getData(h.x, h.y, h.z), tile==NULL?"null" : tile->getDescriptionId().c_str() );

		const float br = 0.65f;
		glColor4f2(br * 1.0f, br * 1.0f, br * 1.0f, br * 1.0f);
		glPushMatrix2();

		//glPolygonOffset(-.3f, -.3f);
		glPolygonOffset(-1.f, -1.f); //Implementation dependent units
		glEnable2(GL_POLYGON_OFFSET_FILL);
		float xo = player->xOld + (player->x - player->xOld) * a;
		float yo = player->yOld + (player->y - player->yOld) * a;
		float zo = player->zOld + (player->z - player->zOld) * a;

		t.begin();
		t.offset(-xo, -yo, -zo);
		t.noColor();

		if (tile == NULL) tile = Tile::rock;
		tileRenderer->tesselateInWorld(tile, h.x, h.y, h.z);

		t.draw();
		t.offset(0, 0, 0);
		glPolygonOffset(0.0f, 0.0f);

		glDisable2(GL_POLYGON_OFFSET_FILL);
		glEnable2(GL_TEXTURE_2D);

		glDepthMask(true);
		glPopMatrix2();

		glEnable2(GL_ALPHA_TEST);
		glDisable2(GL_BLEND);
	}
}

void LevelRenderer::onGraphicsReset()
{
	// 图形上下文重置后旧的 VBO 全部失效：三个天空网格都丢掉重传
	// （照抄 modifiedeight 的 _initResources：generateSky + _buildStarsMesh +
	// cloudsMesh.reset()）。
	skyMesh.reset();
	starsMesh.reset();
	cloudsMesh.reset();
	cloudMeshColorR = -1.0f;   // 云网格缓存失效，下次绘制重建
	generateSky();
	buildStarsMesh();

	// Get new buffers
#ifdef USE_VBO
	glGenBuffers2(numListsOrBuffers, chunkBuffers);
#else
	chunkLists = glGenLists(numListsOrBuffers);
#endif

	// Rebuild
	allChanged();
}

void LevelRenderer::entityAdded( Entity* entity )
{
	if (!entity->isPlayer())
		return;

	// Hack to (hopefully) get the players to show
	EntityRenderDispatcher::getInstance()->onGraphicsReset();
}

int _t_keepPic = -1;

void LevelRenderer::takePicture( TripodCamera* cam, Entity* entity )
{
	// Push old values
	Mob* oldCameraEntity = mc->cameraTargetPlayer;
	bool hideGui = mc->options.hideGui;
	bool thirdPerson = mc->options.thirdPersonView;

	// @huge @attn: This is highly illegal, super temp!
	mc->cameraTargetPlayer = (Mob*)cam;
	mc->options.hideGui = true;
	mc->options.thirdPersonView = false;

	mc->gameRenderer->renderLevel(0);

	// Pop values back
	mc->cameraTargetPlayer = oldCameraEntity;
	mc->options.hideGui = hideGui;
	mc->options.thirdPersonView = thirdPerson;

	_t_keepPic = -1;

	// Save image
	static char filename[256];
	sprintf(filename, "%s/games/com.mojang/img_%.4d.jpg", mc->externalStoragePath.c_str(), getTimeMs());

	mc->platform()->saveScreenshot(filename, mc->width, mc->height);
}

void LevelRenderer::levelEvent(Player* player, int type, int x, int y, int z, int data) {
	switch (type) {    
	case LevelEvent::SOUND_OPEN_DOOR:
        if (Mth::random() < 0.5f) {
            level->playSound(x + 0.5f, y + 0.5f, z + 0.5f, "random.door_open", 1, level->random.nextFloat() * 0.1f + 0.9f);
        } else {
            level->playSound(x + 0.5f, y + 0.5f, z + 0.5f, "random.door_close", 1, level->random.nextFloat() * 0.1f + 0.9f);
        }
        break;
	}
}
