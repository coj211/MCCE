#ifndef NET_MINECRAFT_CLIENT_RENDERER_ENTITY__PlayerRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER_ENTITY__PlayerRenderer_H__

#include "HumanoidMobRenderer.h"

class PlayerRenderer : public HumanoidMobRenderer
{
	typedef HumanoidMobRenderer super;
public:
	PlayerRenderer(HumanoidModel* humanoidModel, float shadow);
	~PlayerRenderer();

	virtual int prepareArmor(Mob* mob, int layer, float a);

	virtual void setupPosition(Entity* mob, float x, float y, float z);
	virtual void setupRotations(Entity* mob, float bob, float bodyRot, float a);

	virtual void renderName(Mob* mob, float x, float y, float z);
	// 玩家皮肤可能为 64x64 双层: 渲染前按格式开关 HumanoidModel 的 outer 层。
	void render(Entity* mob_, float x, float y, float z, float rot, float a);
	virtual void onGraphicsReset();
private:
	HumanoidModel* armorParts1;
	HumanoidModel* armorParts2;
};


#endif /* NET_MINECRAFT_CLIENT_RENDERER_ENTITY__PlayerRenderer_H__ */