#ifndef NET_MINECRAFT_CLIENT_GUI_COMPONENTS__RolledSelectionListH_H__
#define NET_MINECRAFT_CLIENT_GUI_COMPONENTS__RolledSelectionListH_H__
#include "../GuiComponent.h"
#include <cstdint>

struct Tesselator;
struct Minecraft;
struct RolledSelectionListH: GuiComponent
{
	Minecraft* minecraft;
	float field_8, field_C;
	int32_t field_10, field_14, field_18;
	float field_1C, field_20;
	int32_t field_24;
	float field_28, field_2C, field_30, field_34, field_38;
	int32_t field_3C;
	bool renderSelection, componentSelected;
	int8_t field_42, field_43;
	int32_t field_44;
	bool field_48;
	int8_t field_49, field_4A, field_4B;
	int32_t field_4C, field_50;
	float field_54;

	RolledSelectionListH(Minecraft*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
	int32_t getItemAtXPositionRaw(int32_t);
	void setRenderSelection(bool);


	virtual ~RolledSelectionListH();
	virtual int32_t getItemAtPosition(int32_t, int32_t);
	virtual bool capXPosition(void);
	virtual void tick();
	virtual void render(int32_t, int32_t, float);
	virtual void renderHoleBackground(float, float, int32_t, int32_t);
	virtual void setRenderHeader(bool, int32_t);
	virtual void setComponentSelected(bool);
	virtual int32_t getNumberOfItems() = 0;
	virtual void selectStart(int32_t, int32_t, int32_t);
	virtual void selectCancel(void);
	virtual void selectItem(int32_t, bool) = 0;
	virtual bool isSelectedItem(int32_t) = 0;
	virtual int32_t getMaxPosition();
	virtual float getPos(float);
	virtual void touched();
	virtual void renderItem(int32_t, int32_t, int32_t, int32_t, Tesselator&) = 0;
	virtual void renderHeader(int32_t, int32_t, Tesselator&);
	virtual void renderBackground() = 0;
	virtual void renderDecorations(int32_t, int32_t);
	virtual void clickedHeader(int32_t, int32_t);
};

#endif /*NET_MINECRAFT_CLIENT_GUI_COMPONENTS__RolledSelectionListH_H__*/
