#include "TextBox.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
TextBox::TextBox( int id, const std::string& msg )
 : id(0), w(0), h(0), x(0), y(0), text(msg), focused(false) {

}

TextBox::TextBox( int id, int x, int y, const std::string& msg ) 
 : id(id), w(0), h(0), x(x), y(y), text(msg), focused(false) {

}

TextBox::TextBox( int id, int x, int y, int w, int h, const std::string& msg )
 : id(id), w(w), h(h), x(x), y(y), text(msg) {

}

void TextBox::setFocus(Minecraft* minecraft) {
	if(!focused) {
		// 必须是三参版：安卓平台只覆写了这个版本，它会把自己的文本交给 IME 并
		// 弹键盘；无参版是基类的空实现（只置个标志位），输入法不会出来。
		// 第二个参数 0 = 不限长度（Java 侧会当 256）。
		minecraft->platform()->showKeyboard(&this->text, 0, false);
		focused = true;
	}
}

bool TextBox::loseFocus(Minecraft* minecraft) {
	if(focused) {
		// 这里原本写的是 showKeyboard()，显然写反了：失焦应该收键盘，
		// 而 hideKeyboard() 会把 IME 里最终的文本写回 text。
		minecraft->platform()->hideKeyboard();
		focused = false;
		return true;
	}
	return false;
}

void TextBox::render( Minecraft* minecraft, int xm, int ym ) {
	
}
