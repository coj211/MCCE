#include "TextureTesselator.h"
#include "Tesselator.h"
#include "TextureData.h"
#include <cmath>

// 六个轴向单位向量（照抄 modifiedeight 的 Vec3::UNIT_X / NEG_UNIT_X / ...）
namespace {
const Vec3 UNIT_X(1.0f, 0.0f, 0.0f);
const Vec3 NEG_UNIT_X(-1.0f, 0.0f, 0.0f);
const Vec3 UNIT_Y(0.0f, 1.0f, 0.0f);
const Vec3 NEG_UNIT_Y(0.0f, -1.0f, 0.0f);
const Vec3 UNIT_Z(0.0f, 0.0f, 1.0f);
const Vec3 NEG_UNIT_Z(0.0f, 0.0f, -1.0f);

inline float clamp01(float v) {
	if (v > 1.0f) return 1.0f;
	if (v <= 0.0f) return 0.0f;
	return v;
}
}

TextureTesselator::TextureTesselator(const TextureData* td, int txmin, int tymin, int txmax, int tymax,
                                     const Vec3& f10,
                                     float f1Cr, float f1Cg, float f1Cb, float f1Ca,
                                     float f2Cr, float f2Cg, float f2Cb, float f2Ca)
	: texXMin(txmin)
	, texYMin(tymin)
	, texXMax(txmax)
	, texYMax(tymax)
	, field_10(f10)
	, field_1Cr(f1Cr), field_1Cg(f1Cg), field_1Cb(f1Cb), field_1Ca(f1Ca)
	, field_2Cr(f2Cr), field_2Cg(f2Cg), field_2Cb(f2Cb), field_2Ca(f2Ca)
	, textureData(td)
	, field_40(false)
{
}

MeshBuffer TextureTesselator::tesselate() {
	const int width = textureData->w;
	const int height = textureData->h;
	const unsigned char* pixels = textureData->data;

	field_40 = (field_10.x != 0.0f || field_10.y != 0.0f || field_10.z != 0.0f);

	// 参考项目一上来就把 texXMax/texYMax 各 +1，后面三段都用 +1 之后的值。
	const int texXMax1 = texXMax + 1;
	const int texYMax1 = texYMax + 1;

	Tesselator& t = Tesselator::instance;
	// 参考项目这里是 begin(4, 0)：GL_TRIANGLES + 显式索引，每个面一个 quad()。
	// 我们的 Tesselator 是"GL_QUADS 模式下每 4 个顶点自动展开成 2 个三角形"，
	// 所以下面每 4 个 vertex 就是一个面；需要翻转的面按相反顺序发顶点
	// （等价于参考项目 quad(1) 里的 a2+3,a2+2,a2+1,a2）。
	t.begin();

	// ── 第一段：每个有云的格子生成一个顶面 + 一个底面 ──
	{
		float fx = 0.0f;
		for (int x = texXMin; x < texXMax1; ++x) {
			float fz = 0.0f;
			for (int y = texYMin; y < texYMax1; ++y) {
				int u = (x % width + width) % width;
				int v = (y % height + height) % height;
				const unsigned char* px = &pixels[4 * u + 4 * width * v];
				// 注意 `x != texYMax1` 这个条件是参考项目（反编译）里就有的，照抄。
				if (px[3] > 9u && x != texYMax1) {
					// 底面（法线 -Y）
					addLighting(NEG_UNIT_Y, px, u, v);
					t.vertex(fx + 1.0f, 0.0f, fz);
					t.vertex(fx + 1.0f, 0.0f, fz + 1.0f);
					t.vertex(fx, 0.0f, fz + 1.0f);
					t.vertex(fx, 0.0f, fz);
					// 顶面（法线 +Y）
					addLighting(UNIT_Y, px, u, v);
					t.vertex(fx, 1.0f, fz);
					t.vertex(fx, 1.0f, fz + 1.0f);
					t.vertex(fx + 1.0f, 1.0f, fz + 1.0f);
					t.vertex(fx + 1.0f, 1.0f, fz);
				}
				fz += 1.0f;
			}
			fx += 1.0f;
		}
	}

	// ── 第二段：沿 x 扫描，在"有云/没云"的交界处生成法线 ±X 的竖直面 ──
	{
		const unsigned char* lastPixel = pixels;   // 参考项目里的 v39
		Vec3 normal;                               // 提到 goto 之外，避免跨初始化跳转
		float fz = 0.0f;
		int z = texYMin;
		for (;;) {
			if (z > texYMax1) break;
			int x = texXMin;
			float fx = 0.0f;
			int prevFlag = 1;
			const unsigned char* prevPx = lastPixel;
			int u = 0, v = 0;
			const unsigned char* px = lastPixel;
			int curFlag = 1;

			for (;;) {
				if (x > texXMax1) {
					++z;
					fz += 1.0f;
					goto nextRow;
				}
				u = (x % width + width) % width;
				v = (z % height + height) % height;
				px = &pixels[4 * u + 4 * width * v];
				if (px[3] > 9u) break;                 // 这个格子有云

				// 没云
				if (prevFlag != 1) { curFlag = 1; goto LABEL_38; }
				curFlag = 1;

			LABEL_21:
				fx += 1.0f;
				++x;
				prevPx = px;
				prevFlag = curFlag;
			}

			// 有云
			curFlag = (x == texXMax1);
			if (curFlag == prevFlag) goto LABEL_21;
			{
				if (x == texXMax1) {
			LABEL_38:
					normal = UNIT_X;
				} else {
					normal = NEG_UNIT_X;
					prevPx = px;
				}
				addLighting(normal, prevPx, u, v);
				if (curFlag) {   // 参考项目 quad(curFlag)：翻转
					t.vertex(fx, 1.0f, fz);
					t.vertex(fx, 1.0f, fz + 1.0f);
					t.vertex(fx, 0.0f, fz + 1.0f);
					t.vertex(fx, 0.0f, fz);
				} else {
					t.vertex(fx, 0.0f, fz);
					t.vertex(fx, 0.0f, fz + 1.0f);
					t.vertex(fx, 1.0f, fz + 1.0f);
					t.vertex(fx, 1.0f, fz);
				}
				goto LABEL_21;
			}
		nextRow:;
		}
	}

	// ── 第三段：沿 z 扫描，在"有云/没云"的交界处生成法线 ±Z 的竖直面 ──
	{
		const unsigned char* lastPixel = pixels;   // 参考项目里的 v39
		int x = texXMin;
		float fx = 0.0f;
		while (x < texXMax1) {
			int z = texYMin;
			float fz = 0.0f;
			int prevFlag = 1;
			const unsigned char* prevPx = lastPixel;
			while (z < texYMax1) {
				int u = (x % width + width) % width;
				int v = (z % height + height) % height;
				const unsigned char* px = &pixels[4 * u + 4 * width * v];
				int curFlag = (px[3] <= 9u);       // v34 = "这个格子没云"
				if (curFlag != prevFlag) {
					Vec3 normal;
					if (px[3] <= 9u) {
						normal = UNIT_Z;
					} else {
						prevPx = px;
						normal = NEG_UNIT_Z;
					}
					addLighting(normal, prevPx, u, v);
					if (curFlag) {   // 参考项目 quad(curFlag)：翻转
						t.vertex(fx, 0.0f, fz);
						t.vertex(fx + 1.0f, 0.0f, fz);
						t.vertex(fx + 1.0f, 1.0f, fz);
						t.vertex(fx, 1.0f, fz);
					} else {
						t.vertex(fx, 1.0f, fz);
						t.vertex(fx + 1.0f, 1.0f, fz);
						t.vertex(fx + 1.0f, 0.0f, fz);
						t.vertex(fx, 0.0f, fz);
					}
				}
				fz += 1.0f;
				++z;
				prevPx = px;
				prevFlag = curFlag;
			}
			fx += 1.0f;
			++x;
		}
	}

	return t.end();
}

void TextureTesselator::addLighting(const Vec3& normal, const unsigned char* px, int u, int v) {
	Tesselator& t = Tesselator::instance;
	if (field_40) {
		// 光照方向 · 面法线 → 亮度；再乘上贴图像素和云色
		float light = ((field_10.y * normal.y + field_10.x * normal.x + field_10.z * normal.z) + 1.0f) * 0.5f;
		float r = clamp01(light + field_1Cr);
		float g = clamp01(light + field_1Cg);
		float b = clamp01(light + field_1Cb);
		t.color((px[0] * r / 255.0f) * field_2Cr,
		        (px[1] * g / 255.0f) * field_2Cg,
		        (px[2] * b / 255.0f) * field_2Cb,
		        (px[3] / 255.0f) * field_2Ca);
	} else {
		t.normal(normal.x, normal.y, normal.z);
		int pxU = u;
		int pxV = v;
		if (normal.x <= 0.0f) {
			if (normal.z > 0.0f) pxV = v - 1;
		} else {
			--pxU;
		}
		t.tex(((float) pxU + 0.5f) / (float) textureData->w,
		      ((float) pxV + 0.5f) / (float) textureData->h);
	}
}
