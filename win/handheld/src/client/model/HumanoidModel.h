#ifndef NET_MINECRAFT_CLIENT_MODEL__HumanoidModel_H__
#define NET_MINECRAFT_CLIENT_MODEL__HumanoidModel_H__

//package net.minecraft.client.model;

#include "Model.h"
#include "geom/ModelPart.h"
class ItemInstance;
class HumanoidModel: public Model
{
public:
	// modern64 = true: 玩家皮肤模型。整个模型 texSize=64x64(内层即新版
	// 64x64 上半 0..32 行, UV 归一化到 64), 并带 outer 第二层部件。
	// false = 老式 64x32 纹理模型(怪物/盔甲等), 不建 outer。
	HumanoidModel(float g = 0, float yOffset = 0, bool modern64 = false);

	void setupAnim(float time, float r, float bob, float yRot, float xRot, float scale);

	void render(HumanoidModel* model, float scale);
    void render(Entity* e, float time, float r, float bob, float yRot, float xRot, float scale);
	void renderHorrible(float time, float r, float bob, float yRot, float xRot, float scale);
	void onGraphicsReset();

	ModelPart head, /*hair,*/ body, arm0, arm1, leg0, leg1;//, ear;
	// 第二层(64x64 皮肤 outer): 挂在对应 inner 下自动跟随姿态; 默认 invisible。
	// 仅 64x64 皮肤应用时由外部 setOuterVisible(true) 打开。
	ModelPart head2, body2, arm02, arm12, leg02, leg12;
	void setOuterVisible(bool v) {
		head2.visible = v; body2.visible = v;
		arm02.visible = v; arm12.visible = v;
		leg02.visible = v; leg12.visible = v;
	}
	bool holdingLeftHand;
	bool holdingRightHand;
	bool sneaking;
	bool bowAndArrow;

	// 模组（玩家动作）：正在渲染哪个玩家（entityId）；-1 = 不是玩家
	// （怪物 / 纸娃娃 / 第一人称手）。只有 PlayerRenderer 会设置它，而且
	// 渲染完立刻还原 —— 免得别的实体或预览误用别人的动作。
	int scriptPlayerId;
	float scriptPlayerAge;        // 该玩家的 tickCount，进 JS 当 age
	bool scriptPlayerFirstPerson; // true = 这是第一人称那只手（进 JS 当第 4 参）
};

#endif /*NET_MINECRAFT_CLIENT_MODEL__HumanoidModel_H__*/
