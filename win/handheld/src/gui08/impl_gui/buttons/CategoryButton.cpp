#include <gui/buttons/CategoryButton.hpp>

#include <Minecraft.hpp>
#include <Options.hpp>
#include <rendering/Tesselator.hpp>
CategoryButton::CategoryButton(int32_t id, NinePatchLayer* f74, NinePatchLayer* f78, Button** f70) : CategoryButton("", id, f74, f78, f70){

}

CategoryButton::CategoryButton(std::string ss, int32_t id, NinePatchLayer* f74, NinePatchLayer* f78, Button** f70) : ImageButton(id, ss){
	this->field_74 = f74;
	this->field_78 = f78;
	this->field_70 = f70;
}
CategoryButton::~CategoryButton() {
}
void CategoryButton::renderBg(struct Minecraft* mc, int32_t a3, int32_t a4) {
	if (!mc->options.classicGUI) {
		int32_t colorA, colorB;
		mc->options.getNeonColors(colorA, colorB);
		int32_t borderCol = colorA;
		if (this->active && this->pressed && (a3 >= this->x) && (a4 >= this->y) && a3 < this->x + this->width && a4 < this->y + this->height || *this->field_70 == this) {
			borderCol = colorB;
		} else if (!this->active) {
			borderCol = 0xFF555555;
		}
		int32_t glowCol = (borderCol & 0x00FFFFFF) | 0x33000000;
		this->fill(this->x - 1, this->y - 1, this->x + this->width + 1, this->y + this->height + 1, glowCol);
		this->fillGradient(this->x + 1, this->y + 1, this->x + this->width - 1, this->y + this->height - 1, 0xDD0D0214, 0xDD1E042D);
		this->fill(this->x, this->y, this->x + this->width, this->y + 1, borderCol);
		this->fill(this->x, this->y + this->height - 1, this->x + this->width, this->y + this->height, borderCol);
		this->fill(this->x, this->y, this->x + 1, this->y + this->height, borderCol);
		this->fill(this->x + this->width - 1, this->y, this->x + this->width, this->y + this->height, borderCol);
		return;
	}
	int32_t v4, v5;
	int32_t posX, posY;
	NinePatchLayer* v8;
	if(this->active && this->pressed && (v4 = this->x, a3 >= v4) && (v5 = this->y, a4 >= v5) && a3 < v4 + this->width && a4 < v5 + this->height || *this->field_70 == this) {
		posX = this->x;
		posY = this->y;
		v8 = this->field_78;
	} else {
		posX = this->x;
		posY = this->y;
		v8 = this->field_74;
	}
	if (v8) {
		v8->setSize((float)this->width, (float)this->height);
		v8->draw(Tesselator::instance, posX, posY);
	}
}
void CategoryButton::render(struct Minecraft* mc, int32_t x, int32_t y) {
	ImageButton::render(mc, x, y);
	if (this->id == 6 && this->_imageDef.name == "gui/touchgui2.png" && this->_imageDef.u == 134 && this->_imageDef.v == 56) {
		int32_t cx = this->x + this->width / 2;
		int32_t cy = this->y + this->height / 2;
		this->fill(cx - 3, cy - 6, cx + 4, cy - 5, 0xFF1B1B1B);
		this->fill(cx - 3, cy + 6, cx + 4, cy + 7, 0xFF1B1B1B);
		this->fill(cx - 3, cy - 5, cx - 2, cy - 2, 0xFF1B1B1B);
		this->fill(cx + 3, cy - 5, cx + 4, cy - 2, 0xFF1B1B1B);
		this->fill(cx - 3, cy + 3, cx - 2, cy + 6, 0xFF1B1B1B);
		this->fill(cx + 3, cy + 3, cx + 4, cy + 6, 0xFF1B1B1B);
		this->fill(cx - 6, cy - 3, cx - 5, cy + 4, 0xFF1B1B1B);
		this->fill(cx + 6, cy - 3, cx + 7, cy + 4, 0xFF1B1B1B);
		this->fill(cx - 5, cy - 3, cx - 2, cy - 2, 0xFF1B1B1B);
		this->fill(cx - 5, cy + 3, cx - 2, cy + 4, 0xFF1B1B1B);
		this->fill(cx + 3, cy - 3, cx + 6, cy - 2, 0xFF1B1B1B);
		this->fill(cx + 3, cy + 3, cx + 6, cy + 4, 0xFF1B1B1B);
		this->fill(cx - 2, cy - 5, cx + 3, cy + 6, 0xFF6B6B6B);
		this->fill(cx - 5, cy - 2, cx + 6, cy + 3, 0xFF6B6B6B);
		this->fill(cx - 2, cy - 5, cx + 3, cy - 4, 0xFFDBDBDB);
		this->fill(cx - 2, cy - 4, cx - 1, cy - 2, 0xFFDBDBDB);
		this->fill(cx - 2, cy + 3, cx - 1, cy + 5, 0xFFDBDBDB);
		this->fill(cx - 5, cy - 2, cx - 2, cy - 1, 0xFFDBDBDB);
		this->fill(cx + 3, cy - 2, cx + 6, cy - 1, 0xFFDBDBDB);
		this->fill(cx - 5, cy - 1, cx - 4, cy + 3, 0xFFDBDBDB);
		this->fill(cx - 2, cy + 5, cx + 3, cy + 6, 0xFF3C3C3C);
		this->fill(cx + 2, cy - 4, cx + 3, cy - 2, 0xFF3C3C3C);
		this->fill(cx + 2, cy + 3, cx + 3, cy + 5, 0xFF3C3C3C);
		this->fill(cx - 5, cy + 2, cx - 2, cy + 3, 0xFF3C3C3C);
		this->fill(cx + 3, cy + 2, cx + 6, cy + 3, 0xFF3C3C3C);
		this->fill(cx + 5, cy - 1, cx + 6, cy + 2, 0xFF3C3C3C);
	}
}
bool CategoryButton::isSecondImage(bool) {
	return 0;
}
