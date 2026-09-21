#ifndef NET_MINECRAFT_CLIENT_GUI_COMPONENTS__LargeImageButton_H__
#define NET_MINECRAFT_CLIENT_GUI_COMPONENTS__LargeImageButton_H__

#include "ImageButton.h"

class LargeImageButton: public ImageButton
{
	typedef ImageButton super;
public:
	LargeImageButton(int id, const std::string& msg);
	LargeImageButton(int id, const std::string& msg, ImageDef& imageDef);

	void render(Minecraft* minecraft, int xm, int ym);
	// 0.8.1 GUI 移植：按钮缩放动画值（0.8.1 名 field_6C）
	float field_6C;

private:
	void setupDefault();
};

#endif /*NET_MINECRAFT_CLIENT_GUI_COMPONENTS__LargeImageButton_H__*/
