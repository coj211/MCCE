#pragma once
#include <gui/GuiElement.hpp>
#include <Options.hpp>
// 注意：必须 include 完整 Screen 定义，不能只前向声明！
// 前向声明下 MSVC 把 void (Screen::*)(int32_t) 成员函数指针按 16 字节处理
// （不完整类型可能多继承），而完整定义是 4 字节 → 9 参构造参数栈错位，
// 导致 key/validChars 等字段读到垃圾 → 外部页面/编辑世界闪退、NAME 无法输入。
#include <gui/Screen.hpp>
struct Minecraft;
struct Button;
namespace Touch {
	struct TButton;
}

struct TextBox081: GuiElement
{
	static char* numberChars; //"0123456789"
	static char* extendedAcsii; //"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890?^" "~'-_.,;<>!",0x22,"#&()=` []{}",0
	// 当前聚焦的输入框（互斥聚焦用：同一时刻只允许一个 TextBox081 接收输入）
	static TextBox081* s_focused;


	int32_t key;
	Touch::TButton* doneButton;
	Button* field_2C;
	int8_t field_30, field_31, field_32, field_33;
	const Options::Option* option;
	std::string text, field_3C;
	bool field_40;
	int8_t field_41, field_42, field_43;
	int32_t field_44;
	const char* validChars;
	uint32_t field_4C;
	void (Screen::*field_50)(int32_t);
	int32_t field_54;
	Screen* field_58;
	bool field_5C;
	int8_t field_5D, field_5E, field_5F;
	int32_t field_60;

	TextBox081(Minecraft*, const Options::Option*, const std::string&);
	TextBox081(Minecraft*, const std::string&, int32_t, const char*, int32_t, Screen*, void (Screen::*)(int32_t), int32_t, int32_t); //TODO seems to have one less argument according to demangled function name
	int32_t getKey();
	std::string* getText();
	void setText(const std::string&);
	void setValidChars(const char*, uint32_t);
	void updateText(Minecraft*);

	virtual ~TextBox081();
	virtual void tick(Minecraft*);
	virtual void render(Minecraft*, int32_t, int32_t);
	virtual void topRender(Minecraft*, int32_t, int32_t);
	virtual void mouseClicked(Minecraft*, int32_t, int32_t, int32_t);
	virtual void mouseReleased(Minecraft*, int32_t, int32_t, int32_t);
	virtual void focusuedMouseClicked(Minecraft*, int32_t, int32_t, int32_t);
	virtual void focusuedMouseReleased(Minecraft*, int32_t, int32_t, int32_t);
	virtual void keyPressed(Minecraft*, int32_t);
	virtual void keyboardNewChar(Minecraft*, const std::string&, bool);
	virtual bool backPressed(Minecraft*, bool);
	virtual bool suppressOtherGUI();
	virtual void setTextboxText(const std::string&);
	virtual void setFocus(Minecraft*);
	virtual bool loseFocus(Minecraft*);
};
