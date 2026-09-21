#include <gui/buttons/BuyButton.hpp>
#include <util/ImageDef.hpp>
#include <Minecraft.hpp>
#include <string>
#include <util/Util.hpp>
#include <rendering/Textures.hpp>
#include <rendering/Tesselator.hpp>

BuyButton::BuyButton(int32_t n) : ImageButton(n, ""){
	ImageDef img;
	img.name = "";
	img.x = 0;
	img.y = 0;
	img.u = 64;
	img.v = 182;
	img.subW = 190;
	img.subH = 55;
	img.width = 75;
	img.height = 21.711;
	this->setImageDef(img, 1);
}

BuyButton::~BuyButton(){}

void BuyButton::render(Minecraft* mc, int32_t x, int32_t y){
	int32_t color;
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	bool_t sel = 0;
	if(this->active){
		sel = x >= this->x && y >= this->y && x < (this->x+this->width) && y < (this->y+this->height);
	}
	this->renderBg(mc, x, y);
	std::string s;
	if(this->_imageDef.name.size()){
		s = this->_imageDef.name;
	}else{
		s = Util081::EMPTY_STRING;
	}

	if(mc->textures->loadAndBindTexture(s)){
		Tesselator::instance.begin(GL_QUADS);
		if(this->active){
			if(sel || this->text) color = 0xFFCCCCCC;
			else color = 0xFFFFFFFF;
		}else{
			color = 0xFF808080;
		}
		Tesselator::instance.color(color);
		int32_t v13 = this->x;
		int32_t v14 = this->_imageDef.x;
		int32_t v15 = this->y;
		int32_t v16 = this->_imageDef.y;
		float v17 = this->_imageDef.width*0.5;
		float v18 = this->_imageDef.height*0.5;
		float v19, v20;
		if(sel){
			v19 = v17*0.95;
			v20 = v18*0.95;
		}else{
			v20 = this->_imageDef.height*0.5;
			v19 = this->_imageDef.width*0.5;
		}

		TextureData* td = mc->textures->loadAndGetTextureData(s);

		if(this->_imageDef.hasSubImage){
			if(td){
				float width = td->w;
				float v23 = (v13+v14) + v17;
				int32_t v24 = this->_imageDef.v;
				float v25 = (float)this->_imageDef.u/width;
				float v26 = (float)(this->_imageDef.u + this->_imageDef.subW) / width;
				float height = (float) td->h;
				float v28 = (float)(v24 + this->_imageDef.subH) / height;
				float v29 = (float)((float)v15 + (float)v16) + v18;
				float v30 = v29 - v20;

				Tesselator::instance.vertexUV(v23-v19, v29-v20, this->blitOffset, v25, (float)v24/height);
				float v31 = v29+v20;
				Tesselator::instance.vertexUV(v23-v19, v31, this->blitOffset, v25, v28);
				float v32 = v23+v19;
				Tesselator::instance.vertexUV(v32, v31, this->blitOffset, v26, v28);
				Tesselator::instance.vertexUV(v32, v30, this->blitOffset, v26, (float)v24/height);
			}
		}
		Tesselator::instance.draw();
	}
}
