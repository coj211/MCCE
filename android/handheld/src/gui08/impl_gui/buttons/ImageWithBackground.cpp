#include <gui/buttons/ImageWithBackground.hpp>
#include <gui/NinePatchFactory.hpp>
#include <rendering/Textures.hpp>
#include <Minecraft.hpp>
#include <gui/NinePatchLayer.hpp>
#include <cpputils.hpp>
#include <rendering/Tesselator.hpp>

ImageWithBackground::ImageWithBackground(int32_t a) : LargeImageButton(a, ""){
	this->field_70 = 0;
	this->field_74 = 0;
}

void ImageWithBackground::init(Textures* tex, int32_t a3, int32_t a4, IntRectangle a5, IntRectangle a6, int32_t a7, int32_t a8, const std::string &a9){
	NinePatchFactory npf(tex, a9);
	this->field_70 = npf.createSymmetrical(a5, a7, a8, a3, a4);
	this->field_74 = npf.createSymmetrical(a6, a7, a8, a3, a4);
}

void ImageWithBackground::setSize(float w, float h){
	this->width = w;
	this->height = h;
	if(this->field_70){
		if(this->field_74){
			this->field_70->setSize(w, h);
			this->field_74->setSize(w, h);
		}
	}
}

ImageWithBackground::~ImageWithBackground(){
	safeRemove(this->field_70);
	safeRemove(this->field_74);
}

void ImageWithBackground::render(struct Minecraft* mc, int32_t x, int32_t y){
	if(this->visible){
		glColor4f(1, 1, 1, 1);
		bool_t active;
		if(this->active){
			active = this->pressed;
			if(this->pressed){
				active = x >= this->x && y >= this->y && x < (this->x+this->width) && y < (this->y+this->height);
			}
		}
		this->renderBg(mc, x, y);
		if(this->_imageDef.name.size()){
			if(mc->textures->loadAndBindTexture(this->_imageDef.name)){
				Tesselator::instance.begin(GL_QUADS);
				int32_t col;
				if(this->active) col = 0xffffffff;
				else col = 0xff808080;
				Tesselator::instance.color(col);
				int32_t width = this->_imageDef.width;
				int32_t height = this->_imageDef.height;
				float v15 = (((float)this->x + (float)this->_imageDef.x) + (float)(width*0.5)) + ((float)this->width - width)*0.5;
				float v16 = height*0.5;
				float v17 = (((float)this->y + (float)this->_imageDef.y) + (float)(height*0.5)) + ((float)this->height - height)*0.5;
				float v18 = this->field_6C;
				float v19;
				if(active){
					v19 = v18-0.025;
					if((float)(v18-0.025) <= 0.95) v19 = 0.95;
				}else{
					v19 = v18+0.025;
					if((float)(v18+0.025) >= 1.0) v19 = 1;
				}

				int32_t v20 = this->_imageDef.hasSubImage;
				this->field_6C = v19;
				float v21 = this->field_6C;
				float v22 = (float)(width*0.5)*v21;
				float v23 = v16*v21;
				if(v20){
					TextureData* td = mc->textures->loadAndGetTextureData(this->_imageDef.name);
					if(td){
						int32_t v25 = this->_imageDef.v;
						float v26 = td->w;
						float v27 = (float)this->_imageDef.u/v26;
						float v28 = (float)(this->_imageDef.u+this->_imageDef.subW)/v26;
						float v29 = td->h;
						float v30 = v27+0.001;
						float v31 = v17-v23;
						float v32 = v28-0.001;
						float v33 = ((float)(v25 + this->_imageDef.subH)/v29) - 0.001;
						Tesselator::instance.vertexUV(v15-v22, v17-v23, this->blitOffset, v30, ((float)v25/v29)+0.001);
						float v34 = v17+v23;
						Tesselator::instance.vertexUV(v15-v22, v34, this->blitOffset, v30, v33);
						float v35 = v15+v22;
						Tesselator::instance.vertexUV(v35, v34, this->blitOffset, v32, v33);
						Tesselator::instance.vertexUV(v35, v31, this->blitOffset, v32, ((float)v25/v29)+0.001);
					}
				}else{
					float v36 = v17-v23;
					Tesselator::instance.vertexUV(v15-v22, v17-v23, this->blitOffset, 0, 0);
					float v37 = v17+v23;
					Tesselator::instance.vertexUV(v15-v22, v37, this->blitOffset, 0, 1);
					float v38 = v15+v22;
					Tesselator::instance.vertexUV(v38, v37, this->blitOffset, 1, 1);
					Tesselator::instance.vertexUV(v38, v36, this->blitOffset, 1, 0);
				}
				Tesselator::instance.draw();
			}
		}

		if(this->active){
			int32_t v39, v40, a5, v41;
			if(active || this->text){
				v39 = this->x;
				v40 = this->width;
				a5 = this->y+11;
				v41 = 0xffffa0;
			}else{
				v39 = this->x;
				v40 = this->width+((uint32_t)this->width >> 31);
				a5 = this->y+11;
				v41 = 0xffffa0;
			}
			this->drawCenteredString(mc->font, this->msg, v39+(v40>>1), a5, v41);
		}else{
			this->drawCenteredString(mc->font, this->msg, this->x+this->width/2, this->y+11, 0xFFA0A0A0);
		}
	}
}

void ImageWithBackground::renderBg(struct Minecraft* mc, int32_t x, int32_t y){
	// 09 · UI 覆盖：mod 自定义底图/九宫格皮肤优先（跳过原版九宫格）
	if (this->drawModBgIfSet(mc)) return;
	if (!mc->options.classicGUI) {
		int32_t colorA, colorB;
		mc->options.getNeonColors(colorA, colorB);
		int32_t borderCol = colorA;
		if (this->active && this->pressed && x >= this->x && y >= this->y && x < (this->x+this->width) && y < (this->y+this->height)) {
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

	this->field_70->setSize(this->width, this->height);
	this->field_74->setSize(this->width, this->height);

	if(this->field_70){
		NinePatchLayer* l = this->field_74;
		if(l){
			int32_t x, y;
			if(this->active && this->pressed && x >= this->x && y >= this->y && x < (this->x+this->width) && y < (this->y+this->height) || this->text){
				x = this->x;
				y = this->y;
			}else{
				l = this->field_70;
				x = this->x;
				y = this->y;
			}

			l->draw(Tesselator::instance, x, y);
		}
	}
}
