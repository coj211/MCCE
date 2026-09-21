#ifndef NET_MINECRAFT_CLIENT_RENDERER__TextureTesselator_H__
#define NET_MINECRAFT_CLIENT_RENDERER__TextureTesselator_H__

// 照抄 Multiplatform-1.6.4_classic（modifiedeight）的 TextureTesselator：
// 把一张贴图的 alpha 通道当成"哪里有一个方块"的高度图，逐格挤出顶面/底面
// 和四周的竖直面，生成一个体积网格（云就是这么来的：clouds.png 的 alpha
// 决定哪些格子有云，然后长成立体的云块）。
//
// 注意参考工程是反编译产物，里面 texXMax/texYMax 会在 tesselate() 里被 ++，
// 且侧面扫描用的 goto 标签结构很绕——这里按原样保留（换成局部变量，行为等价）。

#include "MeshBuffer.h"
#include "../../world/phys/Vec3.h"

struct TextureData;

struct TextureTesselator {
	int texXMin, texYMin, texXMax, texYMax;   // 要挤出的贴图格子范围（世界格坐标）
	Vec3 field_10;                            // 光照方向（非零 = 顶点着色，零 = 法线+纹理）
	// 参考项目这里是 Color4 field_1C / field_2C，但本工程 gui08 里还有一个
	// 同名的 struct Color4（gui08/include/util/Color4.hpp），两边都实例化会
	// 在链接期撞符号（LNK2005），所以这里直接用 4 个 float。
	float field_1Cr, field_1Cg, field_1Cb, field_1Ca;   // 环境色
	float field_2Cr, field_2Cg, field_2Cb, field_2Ca;   // 云色
	const TextureData* textureData;
	bool field_40;                            // field_10 是否非零

	TextureTesselator(const TextureData* td, int txmin, int tymin, int txmax, int tymax,
	                  const Vec3& f10,
	                  float f1Cr, float f1Cg, float f1Cb, float f1Ca,
	                  float f2Cr, float f2Cg, float f2Cb, float f2Ca);

	MeshBuffer tesselate();

	// 照抄 modifiedeight 的 TextureTesselator::_addLighting：
	// field_10 非零 → 按光照方向把像素色调成顶点色；否则写法线 + 纹理坐标。
	void addLighting(const Vec3& normal, const unsigned char* px, int u, int v);
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__TextureTesselator_H__*/
