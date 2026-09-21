#include "GameRenderer.h"
#include "gles.h"

#include "../../util/PerfTimer.h"
#include "../../util/FrameProf.h"

#include "LevelRenderer.h"
#include "ItemInHandRenderer.h"
#include "culling/AllowAllCuller.h"
#include "culling/FrustumCuller.h"
#include "entity/EntityRenderDispatcher.h"
#include "../Minecraft.h"
#include "../gamemode/GameMode.h"
#include "../particle/ParticleEngine.h"
#include "../player/LocalPlayer.h"
#include "../gui/Screen.h"
#include "../gui/Font.h"
#include "Tesselator.h"
#include "RenderTarget.h"
#if defined(_WIN32) || defined(__ANDROID__)
#include "../../mod/ModEngine.h"
#endif
#include "../../world/level/Level.h"
#include "../../world/level/biome/Biome.h"   // 水下色按群系（waterFogColor）
#include "../../world/entity/Mob.h"
#include "../../world/level/chunk/ChunkCache.h"
#include "../../world/level/material/Material.h"
#include "../../world/Facing.h"
#include "../../platform/input/Controller.h"
#include "../../platform/input/Mouse.h"
#include "../../platform/input/Multitouch.h"
#include "../../NinecraftApp.h"
#include "../../world/level/tile/Tile.h"
#include "../player/input/IInputHolder.h"
#include "Textures.h"
#include "../gui/components/ImageButton.h"
#include "Tesselator.h"

static int _shTicks = -1;

// Set while rendering the scene target with layer-2 dual-writes enabled
// (layer 2 goes to color0 AND the mask channel color1 of the scene target).
static bool g_sceneMaskWrite = false;
// Set while rendering the offscreen scene pass: the held item is skipped
// there and drawn afterwards on the window, on top of the composite.
static bool g_offscreenScenePass = false;
// Reflection refresh rate: every 2nd frame (halves the mirrored full-scene
// re-render; 30Hz planar reflections look identical in practice).
static int g_reflEvery = 0;

// M2: while rendering the normal channel (color3) into the scene target,
// RenderList::renderChunks binds the per-chunk normal-color VBO instead of
// the geometry VBO (fixed-pipeline path, like the brightness channel).
bool g_normalChunkWrite = false;

// M2 normal VBO lazy build: when NO mod ever queries Render.normalTexture the
// per-chunk normal VBOs are pure waste (they double the CPU work + VRAM upload
// of every chunk rebuild). We keep them off by default and only start building
// them once a mod actually asks for the normal channel. Once on, stays on for
// the session (a chunk rebuilt later must carry the normal VBO). Referenced
// from Chunk.cpp rebuildImpl()/uploadGPU().
bool g_chunkNormalsWanted = false;

GameRenderer::GameRenderer( Minecraft* mc )
:	mc(mc),
	renderDistance(0),
	_tick(0),
	_lastTickT(0),
	fovOffset(0),
	fovOffsetO(0),
	fov(1), oFov(1),
	_setupCameraFov(0),
	zoom(1), zoom_x(0), zoom_y(0), modZoom(1),
	cameraRoll(0), cameraRollO(0),
	pickDirection(1, 0, 0),

	thirdDistance(4), thirdDistanceO(4),
	thirdRotation(0), thirdRotationO(0),
	thirdTilt(0), thirdTiltO(0),

	fogBr(0), fogBrO(0),

	fr(0), fg(0), fb(0),
	_rotX(0), _rotY(0),
	_rotXlast(0), _rotYlast(0),
	useScreenScissor(false),
	_offscreenEnabled(false), _sceneTarget(NULL), _reflTarget(NULL),
	_reflWaterY(-999.0f),
	_gbufBrightnessRequested(false), _gbufNormalRequested(false),
	_lastCamX(0), _lastCamY(0), _lastCamZ(0),
	_lastCamYaw(0), _lastCamPitch(0),
	_worldLastCX(0), _worldLastCY(0), _worldLastCZ(0),
	_worldLastProjValid(false), _worldLastChunks(0)
{
	saveMatrices();

	itemInHandRenderer = new ItemInHandRenderer(mc);

	EntityRenderDispatcher* e = EntityRenderDispatcher::getInstance();
	e->itemInHandRenderer = itemInHandRenderer;
	e->textures = mc->textures;
}

GameRenderer::~GameRenderer() {
	delete itemInHandRenderer;
}

void renderCursor(float x, float y, Minecraft* minecraft) {
	Tesselator& t = Tesselator::instance;

	minecraft->textures->loadAndBindTexture("gui/cursor.png");
	glEnable(GL_BLEND);

	const float s = 32;
	const float width = 16;
	const float height = 16;
	t.begin();
	t.color(0xffffffff);
	t.vertexUV(x, y + (float)height, 0, 0, 1);
	t.vertexUV(x + (float)width, y + (float)height, 0, 1, 1);
	t.vertexUV(x + (float)width, y, 0, 1, 0);
	t.vertexUV(x, y, 0, 0, 0);
	t.draw();

	glDisable(GL_BLEND);
}

/*private*/
void GameRenderer::setupCamera(float a, int eye) {
    // Must match LevelRenderer::allChanged() DIST_TABLE exactly.
    static const int DIST_TABLE[8] = {512, 256, 128, 64, 768, 1024, 1536, 2048};
    int vd = mc->options.viewDistance & 7;
    renderDistance = (float)DIST_TABLE[vd];
#if defined(MACOS) || defined(LINUX) || defined(WIN32)
    if (renderDistance > 1024.0f) renderDistance = 1024.0f;
#else
    // Android 与其他非桌面平台一致：上限 400（桌面 1024 会让 LevelRenderer 的
    // VBO 预分配数量膨胀到驱动分配不了，见 LevelRenderer 构造函数里的说明）。
    if (renderDistance > 400.0f)  renderDistance = 400.0f;
#endif
#if defined(ANDROID)
    if (mc->isPowerVR() && vd <= 2)
		renderDistance *= 0.8f;
#endif

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity2();

    float stereoScale = 0.07f;
    if (mc->options.anaglyph3d) glTranslatef2(-(eye * 2 - 1) * stereoScale, 0, 0);
    if (zoom != 1) {
        glTranslatef2((float) zoom_x, (float) -zoom_y, 0);
		glScalef2(zoom, zoom, 1);
        gluPerspective(_setupCameraFov = getFov(a, true), mc->width / (float) mc->height, 0.05f, renderDistance);
    } else {
        gluPerspective(_setupCameraFov = getFov(a, true), mc->width / (float) mc->height, 0.05f, renderDistance);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity2();
    if (mc->options.anaglyph3d) glTranslatef2((eye * 2 - 1) * 0.10f, 0, 0);

    bobHurt(a);
    if (mc->options.bobView) bobView(a);
	 
	moveCameraToPlayer(a);
}

extern int _t_keepPic;

/*public*/
void GameRenderer::render(float a) {
	// Diagnostics: section timings are only collected while F3 is open.
	FrameProf::on() = mc->options.renderDebug;
	TIMER_PUSH("mouse");
	if (mc->player && mc->mouseGrabbed) {
        mc->mouseHandler.poll();
        //printf("Controller.x,y : %f,%f\n", Controller::getX(0), Controller::getY(0));

        float ss = mc->options.sensitivity * 0.6f + 0.2f;
        float sens = (ss * ss * ss) * 8;
        float xo = mc->mouseHandler.xd * sens * 4.f;
        float yo = mc->mouseHandler.yd * sens * 4.f;

		const float now = _tick + a;
		float deltaT = now - _lastTickT;
		if (deltaT > 3.0f) deltaT = 3.0f;
		_lastTickT = now;

		_rotX += xo;
		_rotY += yo;

        int yAxis = -1;
        if (mc->options.invertYMouse) yAxis = 1;

		bool screenCovering = mc->screen && !mc->screen->passEvents;
		if (!screenCovering)
		{
			mc->player->turn(deltaT * _rotXlast, deltaT * _rotYlast * yAxis);
		}
    }

	int xMouse = (int)(Mouse::getX() * Gui::InvGuiScale);
	int yMouse = (int)(Mouse::getY() * Gui::InvGuiScale);
	if (mc->useTouchscreen()) {
		const int pid = Multitouch::getFirstActivePointerIdExThisUpdate();
		if (pid >= 0) {
			xMouse = (int)(Multitouch::getX(pid) * Gui::InvGuiScale);
			yMouse = (int)(Multitouch::getY(pid) * Gui::InvGuiScale);
		} else {
			xMouse = -9999;
			yMouse = -9999;
		}
	}
	TIMER_POP();

	bool hasClearedColorBuffer = false;
	bool hasSetupGuiScreen = false;
	useScreenScissor = false;
	if (mc->isLevelGenerated()) {

		// M2 normal channel: first time a mod really asks for normalTexture,
		// switch on per-chunk normal-VBO building and rebuild existing chunks
		// once so the whole visible world carries normal data (after this,
		// rebuilds include normals automatically via g_chunkNormalsWanted).
		if (!g_chunkNormalsWanted && _gbufNormalRequested && mc->levelRenderer) {
			g_chunkNormalsWanted = true;
			mc->levelRenderer->allChanged();
		}

		TIMER_PUSH("level");
		if (_t_keepPic < 0) {
		if (!(mc->screen && !mc->screen->renderGameBehind())) {

			if (mc->screen && mc->screen->hasClippingArea(screenScissorArea))
				useScreenScissor = true;

#if defined(_WIN32)
			// ── Mod render API: world into an offscreen scene texture, then
			//    let JS mods composite with their own shaders. When no mod
			//    handles onRenderComposite the scene is blitted unchanged.
			bool didOffscreen = false;
			if (_offscreenEnabled && ensureOffscreenTargets(mc->width, mc->height)) {
				// 1) main scene into _sceneTarget; layer 2 also writes the
				//    mask channel (shares the depth => correct occlusion).
				_sceneTarget->bind();
				if (_sceneTarget->hasSecond()) {
					// black the mask channel first
					_sceneTarget->drawToSecond();
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);
					_sceneTarget->drawToFirst();
				}
				g_sceneMaskWrite = true;
				g_offscreenScenePass = true;
				renderLevel(a);
				g_offscreenScenePass = false;
				g_sceneMaskWrite = false;
				// gbuffer 请求位按帧消费: renderLevel 已按上帧状态渲染, 这里清除,
				// 由随后的 onRenderComposite 回调中的查询(模组真采样才查)重新置位。
				clearGbufferRequests();
				_sceneTarget->unbind();
				// 2) planar reflection: mirrored world into _reflTarget
				if (_reflTarget && _reflTarget->ok() && _reflWaterY > -100.0f && ((g_reflEvery++) & 1) == 0)
					renderReflectionTo(a, _reflWaterY);
				didOffscreen = true;
				bool handled = false;
				ModEngine* me = ModEngine::instance;
				if (me) {
					handled = me->fireEvent("onRenderComposite",
						(int)_sceneTarget->colorTexture(0),
						_sceneTarget->width, _sceneTarget->height,
						(int)(_sceneTarget->hasSecond() ? _sceneTarget->colorTexture(1) : 0),
						(int)(_reflTarget ? _reflTarget->colorTexture(0) : 0));
				}
				if (!handled)
					blitSceneToWindow();
				restoreCompositeState();
				// Held item is drawn on the window, above the composite, so
				// the water reflection never paints over the player's hand.
				renderHandOverlayToWindow(a);
			}
			if (!didOffscreen)
#endif
				renderLevel(a);
			hasClearedColorBuffer = true;

			if (!mc->options.hideGui) {
				TIMER_POP_PUSH("gui");
				setupGuiScreen(false);
				hasSetupGuiScreen = true;
				double _ga = FrameProf::now();
				mc->gui.render(a, mc->screen != NULL, xMouse, yMouse);
				FrameProf::add(FrameProf::SEC_GUI, _ga, FrameProf::now());
			}
		}}
		TIMER_POP();

	} else {
        glViewport(0, 0, mc->width, mc->height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity2();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity2();
        setupGuiScreen(true);
		hasSetupGuiScreen = true;
		hasClearedColorBuffer = true;
    }
	//@todo
	if (!hasSetupGuiScreen)
		setupGuiScreen(!hasClearedColorBuffer);

	if (mc->player && mc->screen == NULL) {
		if (mc->inputHolder) mc->inputHolder->render(a);
		if (mc->player->input) mc->player->input->render(a);
	}

    if (mc->screen != NULL) {
		if (useScreenScissor)
			glDisable2(GL_SCISSOR_TEST);

		mc->screen->render(xMouse, yMouse, a);
#ifdef RPI
		renderCursor(xMouse, yMouse, mc);
#endif
		// (Removed the per-frame sleepMs(15) here: the main loop now caps to
		// 60fps globally, and this extra 15ms per frame dragged GUI screens
		// down to ~31fps instead of a smooth 60.)
    }

#if defined(_WIN32) || defined(__ANDROID__)
	// Loading overlay (dimension travel): vanilla dirt-texture background +
	// centered progress text (matches ProgressScreen so it feels like the
	// game's own world-loading screen instead of a plain black veil).
	// Only draw while playing (no pause/menu screen on top): a lingering
	// overlay would otherwise flash over the pause menu / world screens.
	if (ModEngine::instance && (!mc->screen || mc->screen->isInGameScreen())) {
		const std::string& text = ModEngine::instance->loadingOverlay();
		if (!text.empty()) {
			// Dirt tile background (repeated 32px tile, same as
			// Screen::renderDirtBackground / ProgressScreen).
			glDisable2(GL_FOG);
			glColor4f2(1, 1, 1, 1);
			mc->textures->loadAndBindTexture("gui/background.png");
			Tesselator& t = Tesselator::instance;
			const float s = 32;
			t.begin();
			t.color(0x404040);
			t.vertexUV(0, (float)mc->height, 0, 0, (float)mc->height / s);
			t.vertexUV((float)mc->width, (float)mc->height, 0, (float)mc->width / s, (float)mc->height / s);
			t.vertexUV((float)mc->width, 0, 0, (float)mc->width / s, 0);
			t.vertexUV(0, 0, 0, 0, 0);
			t.draw();
			glDisable2(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
			glEnable2(GL_ALPHA_TEST);
			if (mc->font) {
				int tw = mc->font->width(text);
				mc->font->draw(text, (mc->width - tw) / 2, mc->height / 2 - 4, 0xffffffff);
			}
			glEnable2(GL_TEXTURE_2D);
		}
	}
#endif

}

/*public*/
void GameRenderer::setOffscreenSceneEnabled(bool on) {
	_offscreenEnabled = on;
}

// Creates or reuses a RenderTarget sized w×h (destroy+recreate on change).
static bool ensureRenderTargetSize(RenderTarget*& t, int w, int h, bool second = false, bool third = false, bool forth = false) {
	if (w <= 0 || h <= 0)
		return false;
	if (!t)
		t = new RenderTarget();
	if (t->ok() && t->width == w && t->height == h) {
		if ((!second || t->hasSecond()) && (!third || t->hasThird()) && (!forth || t->hasForth()))
			return true;
		t->destroy(); // same size but missing an attachment: rebuild
	}
	return t->init(w, h, second, third, forth);
}

bool GameRenderer::ensureSceneTarget(int w, int h) {
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	return ensureRenderTargetSize(_sceneTarget, w, h);
#else
	return false;
#endif
}

int GameRenderer::normalProgramState() const {
	// M2 normal channel now runs through the fixed pipeline (vertex-color
	// encoded normals) — always usable on desktop.
	return 1;
}

// Creates/reuses scene (with mask channel) + reflection targets at w×h.
// Returns true when the scene target is usable (reflection is best-effort).
bool GameRenderer::ensureOffscreenTargets(int w, int h) {
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	bool scene = ensureRenderTargetSize(_sceneTarget, w, h, /*second*/ true, /*third*/ true, /*forth*/ true);
	ensureRenderTargetSize(_reflTarget, w, h);
	return scene;
#else
	return false;
#endif
}

bool GameRenderer::areCompositeTargetsReady() const {
	return _sceneTarget && _sceneTarget->ok();
}

// Layer-2 (water / translucent) mask now comes from the scene target's
// second color channel (see g_sceneMaskWrite in renderLevel): the water is
// rendered with the exact scene depth, so occluded water stays occluded.

// Planar water reflection: render the world mirrored about the horizontal
// plane y=wy into _reflTarget, from the main camera. The mirror transform is
// pushed onto the modelview before RenderList applies its own -camera
// translate, so vertices flow translate(-cam) -> mirror -> main view.
// Layer 2 is skipped (the water surface itself must not appear in the
// reflection); clouds and entities are included so they show up mirrored.
void GameRenderer::renderReflectionTo(float a, float wy) {
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	if (!mc || !mc->levelRenderer || !mc->textures)
		return;
	Mob* cam = mc->cameraTargetPlayer;
	if (!cam)
		return;
	LevelRenderer* levelRenderer = mc->levelRenderer;
	float xOff = cam->xOld + (cam->x - cam->xOld) * a;
	float yOff = cam->yOld + (cam->y - cam->yOld) * a;
	float zOff = cam->zOld + (cam->z - cam->zOld) * a;
	(void)xOff; (void)zOff;

	_reflTarget->bind();
	// Sky-coloured background: the mirrored world occupies the lower part of
	// the reflection texture; the upper part is the sky direction.
	setupClearColor(a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	setupCamera(a, 0);
	glEnable2(GL_CULL_FACE);
	glPushMatrix2();
	glTranslatef2(0, 2.0f * (wy - yOff), 0);
	glScalef2(1, -1, 1);
	glFrontFace(GL_CW); // the mirror flips triangle winding

	// Clouds (renderClouds draws world-space geometry, so mirroring works;
	// renderSky is a no-op on the WIN32 desktop build).
	if (mc->options.fancyGraphics)
		prepareAndRenderClouds(levelRenderer, a);

	setupFog(0);
	glEnable2(GL_FOG);
	// Offscreen reflection: same no-engine-fog policy as the main scene so
	// the water mirror keeps near colours too.
	if (_offscreenEnabled)
		glDisable2(GL_FOG);
	// Clip away real geometry BELOW the water plane (world y < wy): after
	// mirroring it would land above the water and paint dark fake-block
	// faces into the near-shore reflection. RenderList shifts vertices by
	// -camera before the mirror, so the plane constant is expressed in
	// camera space: keep x.y >= (wy - camY).
	glEnable(GL_CLIP_PLANE0);
	{
		GLdouble plane[4] = { 0, 1, 0, (GLdouble)(yOff - wy) };
		glClipPlane(GL_CLIP_PLANE0, plane);
	}
	glEnable2(GL_TEXTURE_2D);
	mc->textures->loadAndBindTexture("terrain.png");
	glDisable2(GL_ALPHA_TEST);
	glDisable2(GL_BLEND);
	levelRenderer->render(cam, 0, a);
	glEnable2(GL_ALPHA_TEST);
	levelRenderer->render(cam, 1, a);
	// 植被层(layer3): 树叶/草/花也进入水面倒影(保持 alpha test)
	levelRenderer->render(cam, 3, a);
	glDisable2(GL_ALPHA_TEST);

	// Entities (players in third person, animals, ...) into the reflection.
	// RenderList/terrain already painted into the depth buffer; occlusion is
	// naturally correct. Culling is left to the entity's own distance check.
	{
		AllowAllCuller allowAll;
		levelRenderer->renderEntities(cam->getPos(a), &allowAll, a);
	}

	glDisable(GL_CLIP_PLANE0);
	glFrontFace(GL_CCW);
	glPopMatrix2();
	_reflTarget->unbind();
	glDisable2(GL_TEXTURE_2D);
#endif
}

// No-mod fallback for the composite hook: draw the scene texture to the
// window unchanged (fullscreen textured quad in GUI-style coordinates,
// y axis down; the offscreen texture is y-flipped vs window rows, so the
// quad's top edge samples v=1).
void GameRenderer::blitSceneToWindow() {
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	if (!_sceneTarget || !_sceneTarget->ok())
		return;
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glColor4f2(1, 1, 1, 1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity2();
	glOrtho(0, (GLfloat)mc->width, (GLfloat)mc->height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity2();
	glBindTexture(GL_TEXTURE_2D, _sceneTarget->colorTexture());
	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex2f(0, 0);
	glTexCoord2f(1, 1); glVertex2f((GLfloat)mc->width, 0);
	glTexCoord2f(1, 0); glVertex2f((GLfloat)mc->width, (GLfloat)mc->height);
	glTexCoord2f(0, 0); glVertex2f(0, (GLfloat)mc->height);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

// Reset GL state that a JS composite mod may have left behind so the
// following HUD/GUI (fixed-function path) renders cleanly. Mirrors the
// fixed-function state the vanilla path has when renderLevel returns
// (texturing on, alpha test on, fog/cull/blend off, identity colour).
void GameRenderer::restoreCompositeState() {
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_FOG);
	// match the vanilla path: renderLevel returns with cull-face ON
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);
	glColor4f2(1, 1, 1, 1);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity2();
#endif
}

// Render the world from an arbitrary camera into the currently bound
// framebuffer (mod pre-binds its own FBO and sets the viewport). Opaque
// terrain layers 0/1 with frustum culling from that camera; the view
// rotation follows the game's convention (yaw/pitch, degrees). When
// orthoHalf > 0 the camera is orthographic (±orthoHalf square around the
// view axis, near/far span the depth) instead of perspective — used by
// mods for shadow maps. The matrices pushed (projection + modelview) and
// the render origin are kept for lastWorldProjection()/View()/Center().
bool GameRenderer::renderWorldScripted(float x, float y, float z,
	float yawDeg, float pitchDeg, float fovDeg,
	float nearP, float farP, float orthoHalf) {
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	if (!mc || !mc->levelRenderer || !mc->textures || !mc->cameraTargetPlayer)
		return false;
	LevelRenderer* levelRenderer = mc->levelRenderer;
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	float aspect = (vp[2] > 0 && vp[3] > 0)
		? (float)vp[2] / (float)vp[3]
		: (mc->height > 0 ? mc->width / (float)mc->height : 1.0f);
	bool ortho = orthoHalf > 0.0f;
	float fov = fovDeg > 5.0f ? fovDeg : 70.0f;
	float n, f;
	if (ortho) {
		// Orthographic shadow camera: near/far may be negative (the scene
		// sits centred at the camera), clamp only the far>near ordering.
		n = nearP;
		f = farP;
		if (f <= n) f = n + 10.0f;
	} else {
		n = nearP > 0 ? nearP : 0.05f;
		f = (farP > n) ? farP : renderDistance;
		if (f <= n) f = n + 10.0f;
	}

	// RenderList translates chunks by -origin; point it at this camera.
	levelRenderer->setScriptedCameraOrigin(x, y, z);
	// This camera is independent from the player-view occlusion queries:
	// force everything visible so chunks aren't dropped.
	levelRenderer->forceOcclusionVisibleAll();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix2();
	glLoadIdentity2();
	if (ortho)
		glOrtho(-orthoHalf, orthoHalf, -orthoHalf, orthoHalf, n, f);
	else
		gluPerspective(fov, aspect, n, f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix2();
	glLoadIdentity2();
	glRotatef2(pitchDeg, 1, 0, 0);
	glRotatef2(yawDeg + 180, 0, -1, 0);

	setupFog(0);
	glEnable2(GL_FOG);
	glEnable2(GL_TEXTURE_2D);
	mc->textures->loadAndBindTexture("terrain.png");
	glDisable2(GL_ALPHA_TEST);
	glDisable2(GL_BLEND);
	glEnable2(GL_CULL_FACE);
	glShadeModel2(mc->options.ambientOcclusion ? GL_SMOOTH : GL_FLAT);

	FrustumCuller frustum;
	frustum.prepare(x, y, z);
	levelRenderer->cull(&frustum, 0.0f);

	int drawn = 0;
	drawn += levelRenderer->render(mc->cameraTargetPlayer, 0, 0.0f);
	glEnable2(GL_ALPHA_TEST);
	drawn += levelRenderer->render(mc->cameraTargetPlayer, 1, 0.0f);
	glDisable2(GL_ALPHA_TEST);
	_worldLastChunks = drawn;

	glShadeModel2(GL_FLAT);

	// Capture the exact matrices this camera used (still on the stacks)
	// so the JS mod can map world positions into this light/view space.
	glMatrixMode(GL_PROJECTION);
	glGetFloatv(GL_PROJECTION_MATRIX, _worldLastProj);
	glMatrixMode(GL_MODELVIEW);
	glGetFloatv(GL_MODELVIEW_MATRIX, _worldLastView);
	_worldLastCX = x;
	_worldLastCY = y;
	_worldLastCZ = z;
	_worldLastProjValid = true;

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix2();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix2();
	glMatrixMode(GL_MODELVIEW);
	levelRenderer->clearScriptedCameraOrigin();
	return true;
#else
	return false;
#endif
}

// Draw the held item (first-person hand/block) on the window, after the JS
// composite has painted the scene, so the reflection never paints over the
// player's own hand. Needs only the camera projection; renderItemInHand
// builds its own modelview.
void GameRenderer::renderHandOverlayToWindow(float a) {
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	if (!mc || !mc->cameraTargetPlayer || zoom != 1)
		return;
	if (mc->options.thirdPersonView)
		return; // no hand in third person
	glViewport(0, 0, mc->width, mc->height);
	setupCamera(a, 0); // perspective + camera state (renderItemInHand overrides MV)
	glEnable2(GL_CULL_FACE);
	glDisable2(GL_BLEND);
	glEnable2(GL_ALPHA_TEST);
	setupFog(1);
	glEnable2(GL_FOG);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT); // window colour from the composite stays
	renderItemInHand(a, 0);
	glDisable2(GL_FOG);
#endif
}

/*public*/
void GameRenderer::renderLevel(float a) {

    if (mc->cameraTargetPlayer == NULL) {
		if (mc->player)
		{
			mc->cameraTargetPlayer = mc->player;
		}
		else
		{
			return;
		}
    }

	TIMER_PUSH("pick");
    pick(a);

    Mob* cameraEntity = mc->cameraTargetPlayer;
    LevelRenderer* levelRenderer = mc->levelRenderer;
    ParticleEngine* particleEngine = mc->particleEngine;
    float xOff = cameraEntity->xOld + (cameraEntity->x - cameraEntity->xOld) * a;
    float yOff = cameraEntity->yOld + (cameraEntity->y - cameraEntity->yOld) * a;
    float zOff = cameraEntity->zOld + (cameraEntity->z - cameraEntity->zOld) * a;

    // Camera snapshot for the mod camera API (matches the view below).
    _lastCamX = xOff;
    _lastCamY = yOff;
    _lastCamZ = zOff;
    _lastCamYaw = cameraEntity->yRotO + (cameraEntity->yRot - cameraEntity->yRotO) * a;
    _lastCamPitch = cameraEntity->xRotO + (cameraEntity->xRot - cameraEntity->xRotO) * a;

    for (int i = 0; i < 2; i++) {
        if (mc->options.anaglyph3d) {
            if (i == 0) glColorMask(false, true, true, false);
            else glColorMask(true, false, false, false);
        }

		TIMER_POP_PUSH("clear");
		glViewport(0, 0, mc->width, mc->height);
		setupClearColor(a);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable2(GL_CULL_FACE);

		TIMER_POP_PUSH("camera");
        setupCamera(a, i);
		saveMatrices();

		if (useScreenScissor) {
			glEnable2(GL_SCISSOR_TEST);
			glScissor(	screenScissorArea.x, screenScissorArea.y,
						screenScissorArea.w, screenScissorArea.h);
		}
		
//         if(mc->options.fancyGraphics) {
// 			setupFog(-1);
// 			TIMER_POP_PUSH("sky");
// 			glFogf(GL_FOG_START, renderDistance  * 0.2f);
// 			glFogf(GL_FOG_END, renderDistance *0.75);
//             levelRenderer->renderSky(a);
// 			glFogf(GL_FOG_START, renderDistance  * 0.6f);
// 			glFogf(GL_FOG_END, renderDistance);
//         }
        glEnable2(GL_FOG);
        setupFog(1);

        if (mc->options.ambientOcclusion) {
            glShadeModel2(GL_SMOOTH);
		}
        
		TIMER_POP_PUSH("frustrum");
		FrustumCuller frustum;
        frustum.prepare(xOff, yOff, zOff);

		TIMER_POP_PUSH("culling");
		{
			double _pf0 = FrameProf::now();
			mc->levelRenderer->cull(&frustum, a);
			double _pf1 = FrameProf::now();
			mc->levelRenderer->updateDirtyChunks(cameraEntity, false);
			double _pf2 = FrameProf::now();
			FrameProf::add(FrameProf::SEC_CULL, _pf0, _pf1);
			FrameProf::add(FrameProf::SEC_DIRTY, _pf1, _pf2);
		}

		if(mc->options.fancyGraphics) {
			prepareAndRenderClouds(levelRenderer, a);
		}

		// 天空里画日出/日落霞光扇面（renderSunriseFan）会改 GL 状态：它把
		// shade model 临时设成 SMOOTH，而本工程天空在地形**之前**画。那里已经
		// 改成"用完归还进入前的状态"了，这里再声明一次地形要用的 shade model，
		// 作为第二道保险（环境光遮蔽开着时地形必须 SMOOTH，否则每个方块面会按
		// 三角形取最后一个顶点的单色 → 日出/日落时方块上出现三角形暗斑）。
		if (mc->options.ambientOcclusion)
			glShadeModel2(GL_SMOOTH);

        setupFog(0);
        glEnable2(GL_FOG);
		// Mod render API (offscreen): engine GL_FOG is disabled so the far
		// terrain keeps near colour (distance fading is done by the JS
		// composite instead, where it can be tuned and matches the sky).
		if (g_offscreenScenePass)
			glDisable2(GL_FOG);

		glEnable2(GL_TEXTURE_2D);
		mc->textures->loadAndBindTexture("terrain.png");
        glDisable2(GL_ALPHA_TEST);
        glDisable2(GL_BLEND);
        glEnable2(GL_CULL_FACE);

		TIMER_POP_PUSH("terrain-0");
		{ double _a = FrameProf::now(); levelRenderer->render(cameraEntity, 0, a); FrameProf::add(FrameProf::SEC_LAYER0, _a, FrameProf::now()); }

		TIMER_POP_PUSH("terrain-1");
        glEnable2(GL_ALPHA_TEST);
		{ double _a = FrameProf::now(); levelRenderer->render(cameraEntity, 1, a); FrameProf::add(FrameProf::SEC_LAYER1, _a, FrameProf::now()); }

		// 植被层(layer3): 草/花/苗/蔗随风摇摆。原 grass 在层1,这里紧随其后(alpha test 开,不透明写深度)。
		{ double _a = FrameProf::now(); levelRenderer->render(cameraEntity, 3, a); FrameProf::add(FrameProf::SEC_LAYER3, _a, FrameProf::now()); }

        glShadeModel2(GL_FLAT);
		TIMER_POP_PUSH("entities");
		{ double _a = FrameProf::now(); mc->levelRenderer->renderEntities(cameraEntity->getPos(a), &frustum, a); FrameProf::add(FrameProf::SEC_ENTITIES, _a, FrameProf::now()); }
//        setupFog(0);
		TIMER_POP_PUSH("particles");
		{ double _a = FrameProf::now(); particleEngine->render(cameraEntity, a); FrameProf::add(FrameProf::SEC_PARTICLES, _a, FrameProf::now()); }

		glDisable2(GL_BLEND);
        glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        setupFog(0);
        glEnable2(GL_BLEND);
        // Cull back faces for blended terrain (water, semi-transparent
        // mod blocks): without culling you can see the inner faces of a
        // transparent cube, showing the block's "skeleton" edges.
        glEnable2(GL_CULL_FACE);
		glDepthMask(GL_FALSE);
        glDisable2(GL_ALPHA_TEST);
		mc->textures->loadAndBindTexture("terrain.png");
        //if (mc->options.fancyGraphics) {
        //    glColorMask(false, false, false, false);
        //    int visibleWaterChunks = levelRenderer->render(cameraEntity, 1, a);
        //    glColorMask(true, true, true, true);
        //    if (mc->options.anaglyph3d) {
        //        if (i == 0) glColorMask(false, true, true, false);
        //        else glColorMask(true, false, false, false);
        //    }
        //    if (visibleWaterChunks > 0) {
        //        levelRenderer->renderSameAsLast(1, a);
        //    }
        //} else
		{
			//glDepthRangef(0.1f, 1.0f);
			//glDepthMask(GL_FALSE);
			TIMER_POP_PUSH("terrain-water");
			glEnable2(GL_DEPTH_TEST);
			// Mod render API: when rendering into the offscreen scene target,
			// layer 2 (water) is also written to the mask channel (color1)
			// so the composite mod gets a depth-occlusion-correct water mask.
			if (g_sceneMaskWrite && _sceneTarget && _sceneTarget->hasSecond())
				_sceneTarget->drawToBoth();
			{ double _a = FrameProf::now(); levelRenderer->render(cameraEntity, 2, a); FrameProf::add(FrameProf::SEC_LAYER2, _a, FrameProf::now()); }
			if (g_sceneMaskWrite && _sceneTarget && _sceneTarget->hasSecond())
				_sceneTarget->drawToFirst();
			// M1: brightness/AO channel (gbuffer color2). Re-render terrain
			// layers 0/1/3 with texturing disabled so color2 receives only the
			// baked vertex color (= block light × face AO from the fixed
			// pipeline). Depth is NOT touched (depthMask false + LEQUAL): the
			// JS depth texture must stay the authoritative scene depth.
			// 性能: 仅当 JS 模组查询过 brightnessTexture (即真会采样 color2)
			// 才做这次整场景重渲; rc 系滤镜/水反/阴影模组从不查询 → 整段跳过。
			if (g_sceneMaskWrite && _sceneTarget && _sceneTarget->hasThird() && _gbufBrightnessRequested) {
				GLint prevDepthFunc = GL_LEQUAL;
				glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
				_sceneTarget->drawToThird();
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				glDisable2(GL_BLEND);
				glDepthMask(GL_FALSE);
				glDepthFunc(GL_LEQUAL);
				glDisable2(GL_TEXTURE_2D);
				glShadeModel2(mc->options.ambientOcclusion ? GL_SMOOTH : GL_FLAT);
				levelRenderer->render(cameraEntity, 0, a);
				glEnable2(GL_ALPHA_TEST);
				levelRenderer->render(cameraEntity, 1, a);
				levelRenderer->render(cameraEntity, 3, a);
				glDisable2(GL_ALPHA_TEST);
				glEnable2(GL_TEXTURE_2D);
				glDepthFunc(prevDepthFunc);
				glDepthMask(GL_TRUE);
				_sceneTarget->drawToFirst();
			}
			// M2: normal channel (gbuffer color3, Java gnormal equivalent).
			// Renders the per-chunk normal-color VBO through the FIXED PIPELINE
			// (g_normalChunkWrite swaps the geometry VBO for the normal VBO,
			// whose vertex color carries the encoded normal) — the same stable
			// path the brightness channel uses. Depth untouched (mask false +
			// LEQUAL).
			// 性能: 仅当 JS 查询过 normalTexture 才重渲(见 M1 注释)。
			if (g_sceneMaskWrite && _sceneTarget && _sceneTarget->hasForth() && _gbufNormalRequested) {
				GLint pd2 = GL_LEQUAL;
				glGetIntegerv(GL_DEPTH_FUNC, &pd2);
				_sceneTarget->drawToFourth();
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);
				glDisable2(GL_BLEND);
				glDepthMask(GL_FALSE);
				glDepthFunc(GL_LEQUAL);
				glDisable2(GL_TEXTURE_2D);
				glShadeModel2(GL_FLAT);
				g_normalChunkWrite = true;
				levelRenderer->render(cameraEntity, 0, a);
				glEnable2(GL_ALPHA_TEST);
				levelRenderer->render(cameraEntity, 1, a);
				levelRenderer->render(cameraEntity, 3, a);
				glDisable2(GL_ALPHA_TEST);
				g_normalChunkWrite = false;
				glEnable2(GL_TEXTURE_2D);
				glDepthFunc(pd2);
				glDepthMask(GL_TRUE);
				_sceneTarget->drawToFirst();
			}
			//glDepthRangef(0, 1);

        }
        
		glDepthMask(GL_TRUE);
        glEnable2(GL_CULL_FACE);
        glDisable2(GL_BLEND);
        glEnable2(GL_ALPHA_TEST);

		if (/*!Minecraft::FLYBY_MODE &&*/ zoom == 1 && cameraEntity->isPlayer()) {
			if (mc->hitResult.isHit() && !cameraEntity->isUnderLiquid(Material::water)) {
				TIMER_POP_PUSH("select");
				Player* player = (Player*) cameraEntity;
				if (mc->useTouchscreen()) {
					levelRenderer->renderHitSelect(player, mc->hitResult, 0, NULL, a); //player.inventory->getSelected(), a);
				}
				levelRenderer->renderHit(player, mc->hitResult, 0, NULL, a);//player->inventory.getSelected(), a);
			}
		}

		glDisable2(GL_FOG);
//
//        setupFog(0);
//        glEnable2(GL_FOG);
////        levelRenderer->renderClouds(a);
//        glDisable2(GL_FOG);
        setupFog(1);

        if (zoom == 1 && !g_offscreenScenePass) {
			TIMER_POP_PUSH("hand");
            glClear(GL_DEPTH_BUFFER_BIT);
            renderItemInHand(a, i);
        }

        if (!mc->options.anaglyph3d) {
			TIMER_POP();
            return;
        }
    }
    glColorMask(true, true, true, false);
	TIMER_POP();
}

void GameRenderer::tickFov() {
	if (mc->cameraTargetPlayer != mc->player)
		return;

	oFov = fov;
	fov += (mc->player->getFieldOfViewModifier() - fov) * 0.5f;
}

/*private*/
float GameRenderer::getFov(float a, bool applyEffects) {
    Mob* player = mc->cameraTargetPlayer;
    float fov = mc->options.fieldOfView;

	if (applyEffects)
		fov *= this->oFov + (this->fov - this->oFov) * a;

    if (player->isUnderLiquid(Material::water)) fov = 60;
    if (player->health <= 0) {
        float duration = player->deathTime + a;

        fov /= ((1 - 500 / (duration + 500)) * 2.0f + 1);
    }
    float out = fov + fovOffsetO + (fovOffset - fovOffsetO) * a;
    if (applyEffects && modZoom > 1.0f) {
        // 模组变焦（player.setZoom）：把【世界】的 FOV 收窄 modZoom 倍 —— 和 TaCZ 的
        // magnificationToFov(mag, fov) = 2*atan(tan(fov/2)/mag) 是同一个公式。
        // 手部渲染走的是 getFov(a, false)（见 renderItemInHand），所以枪的大小不变，
        // 也不会触发 zoom != 1 那套"跳过手部渲染"的老行为。
        float half = out * Mth::DEGRAD * 0.5f;
        out = (float)(2.0 * atan(tan(half) / modZoom) / Mth::DEGRAD);
    }
    return out;
}

/*private*/
void GameRenderer::moveCameraToPlayer(float a) {
    Entity* player = mc->cameraTargetPlayer;

    float heightOffset = player->heightOffset - 1.62f;

    float x = player->xo + (player->x - player->xo) * a;
    float y = player->yo + (player->y - player->yo) * a - heightOffset;
	//printf("camera y: %f\n", y);
    float z = player->zo + (player->z - player->zo) * a;

	//printf("rot: %f %f\n", cameraRollO, cameraRoll);
    glRotatef2(cameraRollO + (cameraRoll - cameraRollO) * a, 0, 0, 1);

	//LOGI("player. alive, removed: %d, %d\n", player->isAlive(), player->removed);
	if(player->isPlayer() && ((Player*)player)->isSleeping()) {
		heightOffset += 1.0;
		glTranslatef(0.0f, 0.3f, 0);
		if (!mc->options.fixedCamera) {
			int t = mc->level->getTile(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));
			if (t == Tile::bed->id) {
				int data = mc->level->getData(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));

				int direction = data & 3;
				glRotatef(float(direction * 90), 0, 1, 0);
			}
			glRotatef(player->yRotO + (player->yRot - player->yRotO) * a + 180, 0, -1, 0);
			glRotatef(player->xRotO + (player->xRot - player->xRotO) * a, -1, 0, 0);
		}
	} else if (mc->options.thirdPersonView/* || (player->isPlayer() && !player->isAlive())*/) {
        float cameraDist = thirdDistanceO + (thirdDistance - thirdDistanceO) * a;

        if (mc->options.fixedCamera) {

            float rotationY = thirdRotationO + (thirdRotation - thirdRotationO) * a;
            float xRot = thirdTiltO + (thirdTilt - thirdTiltO) * a;

            glTranslatef2(0, 0, (float) -cameraDist);
            glRotatef2(xRot, 1, 0, 0);
            glRotatef2(rotationY, 0, 1, 0);
        } else {
            float yRot = player->yRot;
            float xRot = player->xRot/* + 180.0f*/;
            float xd = -Mth::sin(yRot / 180 * Mth::PI) * Mth::cos(xRot / 180 * Mth::PI) * cameraDist;
            float zd = Mth::cos(yRot / 180 * Mth::PI) * Mth::cos(xRot / 180 * Mth::PI) * cameraDist;
            float yd = -Mth::sin(xRot / 180 * Mth::PI) * cameraDist;

            for (int i = 0; i < 8; i++) {
                float xo = (float)((i & 1) * 2 - 1);
                float yo = (float)(((i >> 1) & 1) * 2 - 1);
                float zo = (float)(((i >> 2) & 1) * 2 - 1);

                xo *= 0.1f;
                yo *= 0.1f;
                zo *= 0.1f;

                HitResult hr = mc->level->clip(Vec3(x + xo, y + yo, z + zo), Vec3(x - xd + xo + zo, y - yd + yo, z - zd + zo)); // newTemp
				if (hr.type != NO_HIT) {
                    float dist = hr.pos.distanceTo(Vec3(x, y, z)); // newTemp
                    if (dist < cameraDist) cameraDist = dist;
                }
            }

			//glRotatef2(180, 0, 1, 0);

			glRotatef2(player->xRot - xRot, 1, 0, 0);
            glRotatef2(player->yRot - yRot, 0, 1, 0);
            glTranslatef2(0, 0, (float) -cameraDist);
            glRotatef2(yRot - player->yRot, 0, 1, 0);
            glRotatef2(xRot - player->xRot, 1, 0, 0);
        }
    } else {
        glTranslatef2(0, 0, -0.1f);
    }

    if (!mc->options.fixedCamera) {
        glRotatef2(player->xRotO + (player->xRot - player->xRotO) * a, 1.0f, 0.0f, 0.0f);
        glRotatef2(player->yRotO + (player->yRot - player->yRotO) * a + 180, 0, 1, 0);
		//if (_t_keepPic > 0)
	}
    glTranslatef2(0, heightOffset, 0);
}

/*private*/
void GameRenderer::bobHurt(float a) {
    Mob* player = mc->cameraTargetPlayer;

    float hurt = player->hurtTime - a;

    if (player->health <= 0) {
        float duration = player->deathTime + a;
        glRotatef2(40 - (40 * 200) / (duration + 200), 0, 0, 1);
    }

    if (player->hurtTime <= 0) return;

	hurt /= player->hurtDuration;
    hurt = (float) Mth::sin(hurt * hurt * hurt * hurt * Mth::PI);

    float rr = player->hurtDir;

    glRotatef2(-rr, 0, 1, 0);
    glRotatef2(-hurt * 14, 0, 0, 1);
    glRotatef2(+rr, 0, 1, 0);
}

/*private*/
void GameRenderer::bobView(float a) {
    //if (mc->options.thirdPersonView) return;
	if (!(mc->cameraTargetPlayer->isPlayer())) {
        return;
    }
    Player* player = (Player*) mc->cameraTargetPlayer;

    float wda = player->walkDist - player->walkDistO;
    float b = -(player->walkDist + wda * a);
    float bob = player->oBob + (player->bob - player->oBob) * a;
    float tilt = player->oTilt + (player->tilt - player->oTilt) * a;
    glTranslatef2((float) Mth::sin(b * Mth::PI) * bob * 0.5f, -(float) std::abs(Mth::cos(b * Mth::PI) * bob), 0);
    glRotatef2((float) Mth::sin(b * Mth::PI) * bob * 3, 0, 0, 1);
    glRotatef2((float) std::abs(Mth::cos(b * Mth::PI - 0.2f) * bob) * 5, 1, 0, 0);
    glRotatef2((float) tilt, 1, 0, 0);
}

/*private*/
void GameRenderer::setupFog(int i) {
    Mob* player = mc->cameraTargetPlayer;
	float fogBuffer[4] = {fr, fg, fb, 1};

    glFogfv2(GL_FOG_COLOR, (GLfloat*)fogBuffer);
    glColor4f2(1, 1, 1, 1);

    if (player->isUnderLiquid(Material::water)) {
        glFogx2(GL_FOG_MODE, GL_EXP);
        glFogf2(GL_FOG_DENSITY, 0.1f); // was 0.06

//        float rr = 0.4f;
//        float gg = 0.4f;
//        float bb = 0.9f;
//
//        if (mc->options.anaglyph3d) {
//            float rrr = (rr * 30 + gg * 59 + bb * 11) / 100;
//            float ggg = (rr * 30 + gg * 70) / (100);
//            float bbb = (rr * 30 + bb * 70) / (100);
//
//            rr = rrr;
//            gg = ggg;
//            bb = bbb;
//        }
    } else if (player->isUnderLiquid(Material::lava)) {
        glFogx2(GL_FOG_MODE, GL_EXP);
        glFogf2(GL_FOG_DENSITY, 2.f); // was 0.06
//        float rr = 0.4f;
//        float gg = 0.3f;
//        float bb = 0.3f;
//
//        if (mc->options.anaglyph3d) {
//            float rrr = (rr * 30 + gg * 59 + bb * 11) / 100;
//            float ggg = (rr * 30 + gg * 70) / (100);
//            float bbb = (rr * 30 + bb * 70) / (100);
//
//            rr = rrr;
//            gg = ggg;
//            bb = bbb;
//        }
    } else {
        glFogx2(GL_FOG_MODE, GL_LINEAR);
        glFogf2(GL_FOG_START, renderDistance * 0.6f);
        glFogf2(GL_FOG_END, renderDistance);
        if (i < 0) {
            glFogf2(GL_FOG_START, 0);
            glFogf2(GL_FOG_END, renderDistance * 1.0f);
        }

        if (mc->level->dimension->foggy) {
            glFogf2(GL_FOG_START, 0);
        }
    }

    glEnable2(GL_COLOR_MATERIAL);
    //glColorMaterial(GL_FRONT, GL_AMBIENT);
}

void GameRenderer::updateAllChunks() {
    mc->levelRenderer->updateDirtyChunks(mc->cameraTargetPlayer, true);
}

bool GameRenderer::updateFreeformPickDirection(float a, Vec3& outDir) {

    if (!mc->inputHolder->allowPicking()) {
        _shTicks = 1;
        return false;
    }

    Vec3 c = mc->cameraTargetPlayer->getPos(a);

    bool firstPerson = !mc->options.thirdPersonView;
    const float PickingDistance = firstPerson? 6.0f : 12.0f;

    _shTicks = -1;

    int vp[4] = {0, 0, mc->width, mc->height};
    float pt[3];
    float x = mc->inputHolder->mousex;
    float y = mc->height - mc->inputHolder->mousey;

    //sw.start();

    if (!glhUnProjectf(x, y, 1, lastModelMatrix, lastProjMatrix, vp, pt)) {
        return false;
    }
    Vec3 p1(pt[0] + c.x, pt[1] + c.y, pt[2] + c.z);

    glhUnProjectf(x, y, 0, lastModelMatrix, lastProjMatrix, vp, pt);
    Vec3 p0(pt[0] + c.x, pt[1] + c.y, pt[2] + c.z);

    outDir = (p1 - p0).normalized();
    p1 = p0 + outDir * PickingDistance;

    //sw.stop();
    //sw.printEvery(30, "unproject ");

    const HitResult& hit = mc->hitResult = mc->level->clip(p0, p1, false);

    // If in 3rd person view - verify that the hit target is within range
    if (!firstPerson && hit.isHit()) {
        const float MaxSqrDist = PickingDistance*PickingDistance;
        if (mc->cameraTargetPlayer->distanceToSqr((float)hit.x, (float)hit.y, (float)hit.z) > MaxSqrDist)
            mc->hitResult.type = NO_HIT;
    }
    return true;
}

/*public*/
void GameRenderer::pick(float a) {
	if (mc->level == NULL) return;
    if (mc->cameraTargetPlayer == NULL) return;
	if (!mc->cameraTargetPlayer->isAlive()) return;

    float range = mc->gameMode->getPickRange();
    bool isPicking = true;

    bool freeform = mc->useTouchscreen()  && !mc->options.isJoyTouchArea;
    if (freeform) {
        isPicking = updateFreeformPickDirection(a, pickDirection);
    } else {
        mc->hitResult = mc->cameraTargetPlayer->pick(range, a);
        pickDirection = mc->cameraTargetPlayer->getViewVector(a);
    }

    Vec3  from = mc->cameraTargetPlayer->getPos(a);
    float dist = range;
	if (mc->hitResult.isHit()) {
        dist = mc->hitResult.pos.distanceTo(from);
    }

	if (mc->gameMode->isCreativeType()) {
        /*dist =*/ range = 12;
    } else {
        if (dist > 3) dist = 3;
        range = dist;
    }

    Vec3 pv = (pickDirection * range);
    Vec3 to  = from + pv;
    mc->cameraTargetPlayer->aimDirection = pickDirection;

    Entity* hovered = NULL;
    const float g = 1;
    AABB aabb = mc->cameraTargetPlayer->bb.expand(pv.x, pv.y, pv.z).grow(g, g, g);
	EntityList& objects = mc->level->getEntities(mc->cameraTargetPlayer, aabb);
    float nearest = 0;
    for (unsigned int i = 0; i < objects.size(); i++) {
        Entity* e = objects[i];
        if (!e->isPickable()) continue;

        float rr = e->getPickRadius();
        AABB bb = e->bb.grow(rr, rr, rr);
        HitResult p = bb.clip(from, to);
		//printf("Clip Hitresult %d (%d)\n", p.type, p.isHit());

        if (bb.contains(from)) {
            //@todo: hovered = e; break; ?
            if (nearest >= 0) {
                hovered = e;
                nearest = 0;
            }
		} else if (p.isHit()) {
            float dd = from.distanceTo(p.pos);
            if (dd < nearest || nearest == 0) {
                hovered = e;
                nearest = dd;
            }
        }
    }

    if (hovered != NULL) {
		if(nearest < dist) {
			mc->hitResult = HitResult(hovered);
		}
    }
	else if (isPicking && !mc->hitResult.isHit()) {
		// if we don't have a hit result, attempt to hit the edge of the block we are standing on
		// (this is an pocket edition simplification to help building floors)
        //LOGI("hovered : %d (%f)\n", mc->hitResult.type, viewVec.y);
		if (pickDirection.y < -.7f) {
			// looking down by more than roughly 45 degrees, fetch a hit to the block standing on
			Vec3 to = from.add(0, -2.0f, 0);

			HitResult downHitResult = mc->level->clip(from, to);
			if (downHitResult.isHit()) {
				mc->hitResult = downHitResult;
				mc->hitResult.indirectHit = true;
				// change face (not up)
				if (std::abs(pickDirection.x) > std::abs(pickDirection.z)) {
                    mc->hitResult.f = (pickDirection.x < 0)? 4 : 5;
				} else {
                    mc->hitResult.f = (pickDirection.z < 0)? 2 : 3;
				}
			}
		}
	}
}
/*public*/
void GameRenderer::tick(int nTick, int maxTick) {
	--_t_keepPic;

	if (!mc->player)
	{
		return;
	}

	if (--_shTicks == 0)
		mc->hitResult.type = NO_HIT;

	//_rotXlast = _rotX;
	//_rotYlast = _rotY;

	//LOGI("x: %f\n", _rotX);

    if (nTick == maxTick) {
        const float tickMult = 1.0f / (float)(1 + maxTick);
        _rotXlast = 0.4f * std::pow(std::abs(_rotX), 1.2f) * tickMult;
        if (_rotX < 0) _rotXlast = -_rotXlast;

        _rotYlast = 0.4f * std::pow(std::abs(_rotY), 1.2f) * tickMult;
        if (_rotY < 0) _rotYlast = -_rotYlast;

        _rotX = 0;
        _rotY = 0;
    }

    fogBrO = fogBr;
    thirdDistanceO = thirdDistance;
    thirdRotationO = thirdRotation;
    thirdTiltO = thirdTilt;
    fovOffsetO = fovOffset;
    cameraRollO = cameraRoll;

    if (mc->cameraTargetPlayer == NULL) {
        mc->cameraTargetPlayer = mc->player;
    }

	tickFov();

    float brr = mc->level->getBrightness(	Mth::floor(mc->cameraTargetPlayer->x),
											Mth::floor(mc->cameraTargetPlayer->y),
											Mth::floor(mc->cameraTargetPlayer->z));

	int _vdClamped = mc->options.viewDistance & 7; if (_vdClamped > 3) _vdClamped = 3;
	float whiteness = (3 - _vdClamped) / 3.0f;
    float fogBrT = brr * (1 - whiteness) + whiteness;
    fogBr += (fogBrT - fogBr) * 0.1f;

    _tick++;

    itemInHandRenderer->tick();
//    if (mc->isRaining) tickRain();
}

/*private*/
void GameRenderer::setupClearColor(float a) {
    Level* level = mc->level;
    Mob* player = mc->cameraTargetPlayer;

    int _vd2 = mc->options.viewDistance & 7; if (_vd2 > 3) _vd2 = 3;
    float whiteness = 1.0f / (4 - _vd2);
    whiteness = 1 - (float) pow(whiteness, 0.25f);

    Vec3 skyColor = level->getSkyColor(mc->cameraTargetPlayer, a);
    float sr = (float) skyColor.x;
    float sg = (float) skyColor.y;
    float sb = (float) skyColor.z;

#if defined(_WIN32) || defined(__ANDROID__)
    // Stage 6: a mod sky tint (level.setSkyColor) must clear the screen to
    // the sky color directly — the fog blend below (fr += (sr-fr)*whiteness)
    // barely moves the color when whiteness≈0 at larger view distances, so
    // the custom tint was invisible.
    if (level->skyR >= 0.0f) {
        fr = sr; fg = sg; fb = sb;
    } else
#endif
    {
        // 雾色按群系走（Biome.define 的 fogColor）：把相机位置传进去，
        // 没定义 fogColor 的群系（含原版全部）还是原来的维度雾色。
        Vec3 fogColor = player
            ? level->getFogColor(a, player->x, player->z)
            : level->getFogColor(a);
        fr = (float) fogColor.x;
        fg = (float) fogColor.y;
        fb = (float) fogColor.z;

        // 1.6.4 EntityRenderer::updateFogColor：日出/日落时把霞光混进雾色
        // （天幕是被雾染色的，所以雾色一变，整片天空跟着变橙红）。
        // 细节照 1.6.4：强度 = 玩家朝向 · (太阳所在的那一侧)，太阳在天上取 -X、
        // 在地下取 +X；而且只在前两档渲染距离才做（1.6.4 的 renderDistance < 2）。
        int vd = mc->options.viewDistance & 7; if (vd > 3) vd = 3;
        if (vd < 2) {
            Vec3 look = player->getLookAngle();
            float dirX = (Mth::sin(level->getTimeOfDay(a) * Mth::PI * 2.0f) > 0.0f) ? -1.0f : 1.0f;
            float facing = look.x * dirX;
            if (facing < 0.0f) facing = 0.0f;
            if (facing > 0.0f) {
                float* sunrise = level->getSunriseColor(a);
                if (sunrise != NULL) {
                    facing *= sunrise[3];
                    fr = fr * (1.0f - facing) + sunrise[0] * facing;
                    fg = fg * (1.0f - facing) + sunrise[1] * facing;
                    fb = fb * (1.0f - facing) + sunrise[2] * facing;
                }
            }
        }

        fr += (sr - fr) * whiteness;
        fg += (sg - fg) * whiteness;
        fb += (sb - fb) * whiteness;
    }

    if (player->isUnderLiquid(Material::water)) {
        // 水下清晰色/雾色：群系给了 waterFogColor 就用它，否则是原来的硬编码暗蓝。
        Biome* wb = level->getBiome((int)player->x, (int)player->z);
        int wc = (wb && wb->waterFogColor >= 0) ? wb->waterFogColor : 0x050533;
        fr = ((wc >> 16) & 0xff) / 255.0f;
        fg = ((wc >> 8) & 0xff) / 255.0f;
        fb = ((wc) & 0xff) / 255.0f;
    } else if (player->isUnderLiquid(Material::lava)) {
        fr = 0.6f;
        fg = 0.1f;
        fb = 0.00f;
    }

    float brr = fogBrO + (fogBr - fogBrO) * a;
    fr *= brr;
    fg *= brr;
    fb *= brr;

    if (mc->options.anaglyph3d) {
        float frr = (fr * 30 + fg * 59 + fb * 11) / 100;
        float fgg = (fr * 30 + fg * 70) / (100);
        float fbb = (fr * 30 + fb * 70) / (100);

        fr = frr;
        fg = fgg;
        fb = fbb;
    }

    glClearColor(fr, fg, fb, 1.0f);
}

void GameRenderer::zoomRegion( float zoom, float xa, float ya )
{
	this->zoom = zoom;
	this->zoom_x = xa;
	this->zoom_y = ya;
}

void GameRenderer::unZoomRegion()
{
	zoom = 1;
}

void GameRenderer::setupGuiScreen( bool clearColorBuffer )
{
	// UI 可用宽 = 窗口宽 − 系统接管（不派发触摸）的右侧区域，见
	// AppPlatform::getUiRightInset()，与 Gui::uiScreenWidth() 保持一致。
	int uiRightInset = mc->platform() ? mc->platform()->getUiRightInset() : 0;
	int screenWidth = (int)((mc->width - uiRightInset) * Gui::InvGuiScale);
	int screenHeight = (int)(mc->height * Gui::InvGuiScale);

	// Setup GUI render mode
	GLbitfield clearBits = clearColorBuffer?
			GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT
		:	GL_DEPTH_BUFFER_BIT;

	glClear(clearBits);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity2();
#if defined(__APPLE__) && !defined(MACOS)
	// iOS: Use a smaller near/far ortho range that covers all blitOffset
	// z-values used by GUI elements (0 and -90).
	glOrthof(0, (GLfloat)screenWidth, (GLfloat)screenHeight, 0, -500.0f, 500.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity2();
	// No translate needed — vertices at z=0 and blitOffset are within range
#else
	glOrthof(0, (GLfloat)screenWidth, (GLfloat)screenHeight, 0, 2000, 3000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity2();
	glTranslatef2(0, 0, -2000);
#endif
}

/*private*/
void GameRenderer::renderItemInHand(float a, int eye) {
    glLoadIdentity2();
    if (mc->options.anaglyph3d) glTranslatef2((eye * 2 - 1) * 0.10f, 0, 0);

    glPushMatrix2();
    bobHurt(a);
    if (mc->options.bobView) bobView(a);

    if (!mc->options.thirdPersonView && (mc->cameraTargetPlayer->isPlayer() && !((Player*)mc->cameraTargetPlayer)->isSleeping())) {
        if (!mc->options.hideGui) {
			float fov = getFov(a, false);
			if (fov != _setupCameraFov) {
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				gluPerspective(fov, mc->width / (float) mc->height, 0.05f, renderDistance);
				glMatrixMode(GL_MODELVIEW);
			}
            double _ha = FrameProf::now();
            itemInHandRenderer->render(a);
            FrameProf::add(FrameProf::SEC_HAND, _ha, FrameProf::now());
        }
    }

    glPopMatrix2();
    if (!mc->options.thirdPersonView && (mc->cameraTargetPlayer->isPlayer() && !((Player*)mc->cameraTargetPlayer)->isSleeping())) {
        itemInHandRenderer->renderScreenEffect(a);
        bobHurt(a);
    }
    if (mc->options.bobView) bobView(a);
}

void GameRenderer::onGraphicsReset()
{
	if (itemInHandRenderer) itemInHandRenderer->onGraphicsReset();
}

void GameRenderer::saveMatrices()
{
	#if defined(RPI)
		return;
	#endif

	static bool saved = false;
	//if (saved) return;

	saved = true;

	glGetFloatv(GL_PROJECTION_MATRIX, lastProjMatrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, lastModelMatrix);
}

void GameRenderer::prepareAndRenderClouds( LevelRenderer* levelRenderer, float a ) {
	//if(mc->options.isCloudsOn()) {
	TIMER_PUSH("clouds");
	glMatrixMode(GL_PROJECTION);
	glPushMatrix2();
	glLoadIdentity2();
	// 照抄 modifiedeight GameRenderer::renderSky：远平面是 renderDistance*5120
	// （天幕会被下面的 glScalef 放大到数十万格外，用原来的 *512 会被远平面裁掉）。
	gluPerspective(_setupCameraFov = getFov(a, true), mc->width / (float) mc->height, 2, renderDistance * 5120);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix2();
	setupFog(0);
	glDepthMask(false);
	glEnable2(GL_FOG);
	// 照抄 modifiedeight GameRenderer::renderSky：
	//   1) 天幕/日月/星星整体放大 renderDistance/100*128；
	//   2) 雾拉到 start=0 / end=68129。
	// 这两件事是配套的：放大后半径 2000 的天幕落在 68129 的雾里被染成雾色，
	// 天空色才能和清屏色一致——否则平视时"没加载区块的地方"会露出清屏色，
	// 抬头是天空、前面一条蓝，中间就是那道接缝。
	glPushMatrix2();
	{
		float skyScale = (renderDistance / 100.0f) * 128.0f;
		glScalef2(skyScale, skyScale, skyScale);
		glFogf2(GL_FOG_START, 0.0f);
		glFogf2(GL_FOG_END, 68129.0f);
		levelRenderer->renderSky(a);
	}
	glPopMatrix2();
	glFogf2(GL_FOG_START, renderDistance * 4.2f * 0.6f);
	glFogf2(GL_FOG_END, renderDistance * 4.2f);
	levelRenderer->renderClouds(a);
	glFogf2(GL_FOG_START, renderDistance  * 0.6f);
	glFogf2(GL_FOG_END, renderDistance);
	glDisable2(GL_FOG);
	glDepthMask(true);
	setupFog(1);
	glPopMatrix2();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix2();
	glMatrixMode(GL_MODELVIEW);
	TIMER_POP();
	//}
}
