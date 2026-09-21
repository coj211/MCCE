#ifndef NET_MINECRAFT_CLIENT_GUI__Gui_H__
#define NET_MINECRAFT_CLIENT_GUI__Gui_H__

//package net.minecraft.client.gui;

#include "GuiComponent.h"
#include "Font.h"
#include "../player/input/touchscreen/TouchAreaModel.h"
#include "../renderer/RenderChunk.h"
#include "../../util/Random.h"
#include "../IConfigListener.h"

class Minecraft;
class ItemInstance;
class Textures;
class Tesselator;
struct IntRectangle;

struct GuiMessage
{
	std::string message;
	int ticks;
};

typedef std::vector<GuiMessage> GuiMessageList;

class Gui: public GuiComponent, IConfigListener
{
public:
    Gui(Minecraft* minecraft);
	~Gui();

	int getSlotIdAt(int x, int y);
	void flashSlot(int slotId);
	// 0.8.1 背包移植：清除物品名悬浮（选中物品后不再显示旧名字）
	void resetItemNameOverlay() { itemNameOverlayTime = 0; }
	bool isInside(int x, int y);
	RectangleArea getRectangleArea(int extendSide);
	void getSlotPos(int slot, int& posX, int& posY);
	int getNumSlots();

	void handleClick(int button, int x, int y);
	void handleKeyPressed( int key );

	void tick();
	void render(float a, bool mouseFree, int xMouse, int yMouse);

	void renderToolBar( float a, int ySlot, const int screenWidth );
	// 0.8.1 背包移植：2 参重载（screenWidth 用 minecraft->width）
	void renderToolBar( float a, int ySlot );

	void renderChatMessages( const int screenHeight, unsigned int max, bool isChatting, Font* font );
	void renderChatButton(int screenWidth, int screenHeight);

	// UI 布局用的可用宽/高（逻辑像素）—— 已经扣掉“系统接管、不派发触摸”的
	// 右侧区域（见 AppPlatform::getUiRightInset()），即把它当成屏幕外。
	// 所有 HUD 布局和点击命中判定都用它，保证“画出来的地方”就是“摸得到的地方”。
	int uiScreenWidth() const;
	int uiScreenHeight() const;

	void renderOnSelectItemNameText( const int screenWidth, Font* font, int ySlot );

	void renderSleepAnimation( const int screenWidth, const int screenHeight );

	void renderBubbles();
	void renderHearts();
	void renderDebugInfo();

	void renderProgressIndicator( const bool isTouchInterface, const int screenWidth, const int screenHeight, float a );

    void addMessage(const std::string& string);
	// 聊天屏（ChatInputScreen）读聊天历史用：只读访问，不改动原版 HUD 的渲染。
	const GuiMessageList& getGuiMessages() const { return guiMessages; }
	// 离开世界时清空聊天记录（换到另一个世界不该还留着上个世界的聊天）。
	void clearMessages() { guiMessages.clear(); }
	void postError(int errCode);

    void onGraphicsReset();
	void inventoryUpdated();

	void setNowPlaying(const std::string& string);
	void displayClientMessage(const std::string& messageId);
	void renderSlotText(const ItemInstance* item, float x, float y, bool hasFinite, bool shadow);
	void texturesLoaded( Textures* textures );

	void onConfigChanged(const Config& config);
	void onLevelGenerated();

	void setScissorRect(const IntRectangle& rect);

	static float floorAlignToScreenPixel(float);
	static int itemCountItoa(char* buf, int count);
private:
	void renderVignette(float br, int w, int h);
	void renderSlot(int slot, int x, int y, float a);
	void tickItemDrop();
	float cubeSmoothStep(float percentage, float min, float max);
public:
	float progress;
	std::string selectedName;
	static float InvGuiScale;
	static float GuiScale;

private:
	int MAX_MESSAGE_WIDTH;
	//ItemRenderer itemRenderer;
	GuiMessageList guiMessages;
	Random random;

	Minecraft* minecraft;
	int tickCount;
	float itemNameOverlayTime;
	std::string overlayMessageString;
	int overlayMessageTime;
	bool animateOverlayMessageColor;

	float tbr;

	RenderChunk _inventoryRc;
	bool _inventoryNeedsUpdate;
	
	int _flashSlotId;
	float _flashSlotStartTime;

	Font* _slotFont;
	int _numSlots;

	RenderChunk rcFeedbackOuter;
	RenderChunk rcFeedbackInner;
	float _feedbackRadius;
	float _feedbackRadiusInner;

	// For dropping
	static const float DropTicks;
	float  _currentDropTicks;
	int    _currentDropSlot;
};

#endif /*NET_MINECRAFT_CLIENT_GUI__Gui_H__*/
