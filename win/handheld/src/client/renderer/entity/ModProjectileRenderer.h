#ifndef NET_MINECRAFT_CLIENT_RENDERER_ENTITY__ModProjectileRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER_ENTITY__ModProjectileRenderer_H__

#include "EntityRenderer.h"

// 画模组自定义投射物（Projectile.defineProjectile）。
//   · 用定义里的模型部件渲染（和方块/物品同一套 ModBlockPart）
//   · 贴图走模组包里的独立 GL 纹理 —— 必须自己 glBindTexture，
//     不能走 bindTexture(路径)：模组 png 不在引擎的纹理库里，查不到会沿用上一个纹理
//   · 投射物类型号从 entity->getAuxData() 拿（Entity::getAuxData 是 virtual，
//     ModProjectileEntity 用自己的类型号覆写它）
class ModProjectileRenderer : public EntityRenderer {
	void render(Entity* entity, float x, float y, float z, float rot, float a);
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER_ENTITY__ModProjectileRenderer_H__*/
