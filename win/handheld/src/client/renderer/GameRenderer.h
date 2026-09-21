#ifndef NET_MINECRAFT_CLIENT_RENDERER__GameRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER__GameRenderer_H__

//package net.minecraft.client.renderer;

#include "gles.h"
#include "RenderTarget.h"
#include <cstddef>
#include "../../util/SmoothFloat.h"
#include "../../world/phys/Vec3.h"
#include "../gui/components/ImageButton.h"

class Minecraft;
class Entity;
class ItemInHandRenderer;
class LevelRenderer;
class GameRenderer
{
public:
	GameRenderer(Minecraft* mc_);
	~GameRenderer();

	void pick(float a);

	void updateAllChunks();

	void zoomRegion(float zoom, float xa, float ya);
	void unZoomRegion();

	// 模组变焦（JS player.setZoom）—— 和上面 zoomRegion 的区别很关键：
	//   zoomRegion 走的是老的 zoom 状态，zoom != 1 时引擎会**跳过第一人称手部渲染**
	//   （枪直接消失）；这个只收窄【世界】投影的 FOV（getFov 里 applyEffects=true 那条路），
	//   手部走 applyEffects=false 不受影响 —— 于是开镜时枪还在屏幕上、大小也不变，
	//   可以再把它推到"镜片对准屏幕中央"（TaCZ 的 zoom_model_fov 就是这个意思）。
	void setModZoom(float z) { modZoom = (z > 1.0f && z == z) ? z : 1.0f; }
	float getModZoom() const { return modZoom; }
	void setupGuiScreen(bool clearColorBuffer);

	void tick(int nTick, int maxTick);
	void render(float a);
	void renderLevel(float a);
	void renderItemInHand(float a, int eye);

	void onGraphicsReset();

	void setupCamera(float a, int eye);
	void moveCameraToPlayer(float a);

	void setupClearColor(float a);
	float getFov(float a, bool applyEffects);
	// 渲染距离（世界单位）。天空/云的自建投影矩阵和雾范围要用它
	// （照抄 modifiedeight 里 LevelRenderer 读 GameRenderer::field_8 的用法）。
	float getRenderDistance() const { return renderDistance; }

	// ── Offscreen scene rendering for the mod render API (桌面 GL) ──
	// When enabled, the world (renderLevel) is drawn into an RGBA8+D24 FBO
	// instead of the window; after that the engine fires the JS event
	// onRenderComposite(sceneTex, w, h, maskTex, reflTex) so mods can draw
	// the final frame with their own GLSL. With no mod handling the event
	// the scene texture is blitted back to the window unchanged (visual
	// no-op, minus cost).
	// maskTex = second color attachment of the scene target (layer-2 water /
	// translucent, shares the scene depth so occluded water stays occluded);
	// reflTex = planar water reflection rendered from the camera mirrored at
	// the water plane (see setReflectionWaterY).
	void setOffscreenSceneEnabled(bool on);
	bool isOffscreenSceneEnabled() const { return _offscreenEnabled; }
	unsigned int sceneColorTexture() const { return _sceneTarget ? _sceneTarget->colorTexture(0) : 0; }
	unsigned int sceneDepthTexture() const { return _sceneTarget ? _sceneTarget->depthTexture() : 0; }
	// gbuffer color2 (M0: reserved attachment, not yet written by a pass)
	unsigned int sceneBrightnessTexture() const {
		return (_sceneTarget && _sceneTarget->hasThird()) ? _sceneTarget->colorTexture(2) : 0;
	}
	// gbuffer color3 (world/face normals encoded n*0.5+0.5)
	unsigned int sceneNormalTexture() const {
		return (_sceneTarget && _sceneTarget->hasForth()) ? _sceneTarget->colorTexture(3) : 0;
	}
	// M2 normal-pass GLSL program state: 0=not built 1=ok 2=build failed
	int normalProgramState() const;
	unsigned int maskColorTexture() const {
		return (_sceneTarget && _sceneTarget->hasSecond()) ? _sceneTarget->colorTexture(1) : 0;
	}
	unsigned int reflectionColorTexture() const { return _reflTarget ? _reflTarget->colorTexture(0) : 0; }
	bool ensureSceneTarget(int w, int h);
	bool isSceneTargetReady() const { return _sceneTarget && _sceneTarget->ok(); }
	bool areCompositeTargetsReady() const;
	// Water plane height used by the planar reflection pass (world Y of the
	// water surface). <= -100 disables the reflection pass.
	void setReflectionWaterY(float y) { _reflWaterY = y; }
	float reflectionWaterY() const { return _reflWaterY; }
	// Snapshot of the camera used for the last scene render (camera API).
	float lastCameraX() const { return _lastCamX; }
	float lastCameraY() const { return _lastCamY; }
	float lastCameraZ() const { return _lastCamZ; }
	float lastCameraYaw() const { return _lastCamYaw; }
	float lastCameraPitch() const { return _lastCamPitch; }
	// GL matrices (column-major float[16]) of the last scene render —
	// capture right after the camera setup (see saveMatrices()).
	const float* lastProjectionMatrix() const { return lastProjMatrix; }
	const float* lastModelViewMatrix() const { return lastModelMatrix; }
	// Scripted camera: render the world from an arbitrary camera into the
	// CURRENTLY bound framebuffer (the mod binds its own FBO + viewport
	// first). Draws opaque terrain (layers 0/1) with frustum culling from
	// that camera. Angles in degrees. Returns false when not usable.
	// orthoHalf > 0 switches to an orthographic camera (±orthoHalf square
	// view, near/far as given) — used for shadow maps. Whatever was rendered
	// is captured so the JS mod can read back the exact matrices used:
	// lastWorldProjection/lastWorldView (column-major) + lastWorldCenter.
	bool renderWorldScripted(float x, float y, float z,
		float yawDeg, float pitchDeg, float fovDeg,
		float nearP, float farP, float orthoHalf = 0.0f);
	// The projection/modelview matrices the most recent scripted world
	// render pushed (valid after a renderWorldScripted call; perspective or
	// ortho depending on the mode chosen) and the camera origin that the
	// chunk renderer translated relative to (the "world center").
	const float* lastWorldProjection() const { return _worldLastProjValid ? _worldLastProj : NULL; }
	const float* lastWorldView() const { return _worldLastProjValid ? _worldLastView : NULL; }
	float lastWorldCenterX() const { return _worldLastCX; }
	float lastWorldCenterY() const { return _worldLastCY; }
	float lastWorldCenterZ() const { return _worldLastCZ; }
	// Visible non-empty chunks actually drawn by the most recent scripted
	// world render (diagnostics: 0 means the light camera culled everything).
	int lastWorldChunks() const { return _worldLastChunks; }
private:
	void setupFog(int i);

	void tickFov();
	

	void bobHurt(float a);
	void bobView(float a);

	bool updateFreeformPickDirection(float a, Vec3& outDir);
	void prepareAndRenderClouds(LevelRenderer* levelRenderer, float a);

public:
	ItemInHandRenderer* itemInHandRenderer;

	// ── gbuffer color2/3 按需渲染(性能) ──
	// color2(brightness)/color3(normal) 只在 JS 模组真正查询过对应纹理时
	// 才每帧重渲;不查询(=模组不用)则整段跳过,省两遍全场景重渲。
	// 查询发生在 onRenderComposite(渲染完成后),故语义为 sticky:
	// 置位后保持,直到离屏被禁用或尺寸重建(此时内容由 init 中性值兜底)。
	void requestGbufferBrightness() { _gbufBrightnessRequested = true; }
	void requestGbufferNormal()     { _gbufNormalRequested = true; }
	void clearGbufferRequests()     { _gbufBrightnessRequested = false; _gbufNormalRequested = false; }

private:
	Minecraft* mc;

	float renderDistance;
	int _tick;
	Vec3 pickDirection;

	// smooth camera movement
	SmoothFloat smoothTurnX;
	SmoothFloat smoothTurnY;

	//    // third-person distance etc
	//    SmoothFloat smoothDistance = /*new*/ SmoothFloat();
	//    SmoothFloat smoothRotation = /*new*/ SmoothFloat();
	//    SmoothFloat smoothTilt = /*new*/ SmoothFloat();
	//    SmoothFloat smoothRoll = /*new*/ SmoothFloat();

	float thirdDistance;
	float thirdDistanceO;
	float thirdRotation;
	float thirdRotationO;
	float thirdTilt;
	float thirdTiltO;

	// zoom
	float zoom;
	float zoom_x;
	float zoom_y;
	float modZoom;   // 模组变焦（player.setZoom）：只作用于世界 FOV，不影响手部

	// fov modification
	float fov, oFov;
	float fovOffset;
	float fovOffsetO;
	float _setupCameraFov;

	// roll modification
	float cameraRoll;
	float cameraRollO;

	float fr;
	float fg;
	float fb;

	float fogBrO, fogBr;

	float _rotX;
	float _rotY;
	float _rotXlast;
	float _rotYlast;
	float _lastTickT;

	void saveMatrices();
	float lastProjMatrix[16];
	float lastModelMatrix[16];

	// Scissor area that Minecraft::screen defines
	bool useScreenScissor;
	IntRectangle screenScissorArea;

	// Offscreen scene state (mod render API)
	bool _offscreenEnabled;
	RenderTarget* _sceneTarget;
	RenderTarget* _reflTarget;
	float _reflWaterY;
	float _lastCamX, _lastCamY, _lastCamZ;
	float _lastCamYaw, _lastCamPitch;
	bool ensureOffscreenTargets(int w, int h);
	void renderReflectionTo(float a, float wy); // mirrored world into _reflTarget
	void renderHandOverlayToWindow(float a);    // held item drawn over the composite
	void blitSceneToWindow();      // no-mod fallback: draw scene tex fullscreen
	void restoreCompositeState();  // GL state reset after the JS composite hook

	// ── gbuffer color2/3 请求位(见 public requestGbuffer*) ──
	bool _gbufBrightnessRequested;
	bool _gbufNormalRequested;

	// Matrices captured by the most recent scripted world render.
	float _worldLastProj[16];
	float _worldLastView[16];
	float _worldLastCX, _worldLastCY, _worldLastCZ;
	bool _worldLastProjValid;
	int _worldLastChunks;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__GameRenderer_H__*/
