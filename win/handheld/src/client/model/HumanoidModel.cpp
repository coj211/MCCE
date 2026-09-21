#include "HumanoidModel.h"
#include "../Minecraft.h"
#include "../../util/Mth.h"
#include "../../world/entity/player/Player.h"
#include "../../world/entity/player/Inventory.h"
#if defined(_WIN32) || defined(__ANDROID__)
#include "../../mod/ModEngine.h"   // 模组：按玩家分别的动作（见 setupAnim 末尾）

// 把脚本给的姿态写到部件上：旋转总是覆盖；位置只在脚本给了 p* 时覆盖。
// 位置是绝对坐标（模型格）——“躺下”这类姿态要把部件整体挪位，只转角度
// 做不到（每个部件是绕自己的点转的）。
static void applyPartPose(ModelPart& p, const ModPartPose& pose) {
	p.xRot = pose.rx;
	p.yRot = pose.ry;
	p.zRot = pose.rz;
	if (pose.hasPos) {
		p.x = pose.px;
		p.y = pose.py;
		p.z = pose.pz;
	}
}
#endif

HumanoidModel::HumanoidModel( float g /*= 0*/, float yOffset /*= 0*/, bool modern64 /*= false*/ )
:	holdingLeftHand(false),
	holdingRightHand(false),
	sneaking(false),
	bowAndArrow(false),
	scriptPlayerId(-1),
	scriptPlayerAge(0),
	scriptPlayerFirstPerson(false),
	head(0, 0),
	//ear (24, 0),
	//hair(32, 0),
	body(16, 16),
	arm0(24 + 16, 16),
	arm1(24 + 16, 16),
	leg0(0, 16),
	leg1(0, 16)
{
	// modern64(玩家皮肤模型): 纹理按 64x64 归一化 —— 内层(老坐标 0..32 行)
	// 采样 64x64 图的上半; 并带 outer 第二层部件(见下)。普通怪物/盔甲模型
	// 保持 64x32(texHeight 默认 32), 行为不变。
	if (modern64)
		texHeight = 64;

	head.setModel(this);
	body.setModel(this);
	arm0.setModel(this);
	arm1.setModel(this);
	leg0.setModel(this);
	leg1.setModel(this);

	head.addBox(-4, -8, -4, 8, 8, 8, g); // Head
	head.setPos(0, 0 + yOffset, 0);

	// 第二层(仅 modern64 玩家模型): 几何比 inner 外扩(头 9x9x9, 其它 8.5x12.5
	// x4.5), UV 按 64x64 布局(权威: 头 32,0 / 身 16,32 / 右臂 40,32 / 左臂
	// 48,48 / 右腿 0,32 / 左腿 0,48)。默认 invisible, 挂 inner 下跟随姿态。
	if (modern64) {
		head2.setModel(this);
		head2.setTexSize(64, 64);
		head2.texOffs(32, 0);
		head2.addBox(-4, -8, -4, 8, 8, 8, 0.5f);   // 头 outer 9x9x9
		head2.setPos(0, 0 + yOffset, 0);
		head.addChild(&head2);

		body2.setModel(this);
		body2.setTexSize(64, 64);
		body2.texOffs(16, 32);
		body2.addBox(-4, 0, -2, 8, 12, 4, 0.25f);  // 8.5x12.5x4.5
		body2.setPos(0, 0 + yOffset, 0);
		body.addChild(&body2);

		arm02.setModel(this);
		arm02.setTexSize(64, 64);
		arm02.texOffs(40, 32);
		arm02.addBox(-3, -2, -2, 4, 12, 4, 0.25f); // 4.5x12.5x4.5
		arm02.setPos(-5, 2 + yOffset, 0);
		arm0.addChild(&arm02);

		arm12.setModel(this);
		arm12.setTexSize(64, 64);
		arm12.texOffs(48, 48);
		arm12.mirror = true;
		arm12.addBox(-1, -2, -2, 4, 12, 4, 0.25f);
		arm12.setPos(5, 2 + yOffset, 0);
		arm1.addChild(&arm12);

		leg02.setModel(this);
		leg02.setTexSize(64, 64);
		leg02.texOffs(0, 32);
		leg02.addBox(-2, 0, -2, 4, 12, 4, 0.25f);
		leg02.setPos(-2, 12 + yOffset, 0);
		leg0.addChild(&leg02);

		leg12.setModel(this);
		leg12.setTexSize(64, 64);
		leg12.texOffs(0, 48);
		leg12.mirror = true;
		leg12.addBox(-2, 0, -2, 4, 12, 4, 0.25f);
		leg12.setPos(2, 12 + yOffset, 0);
		leg1.addChild(&leg12);

		setOuterVisible(false);
	}

	body.addBox(-4, 0, -2, 8, 12, 4, g); // Body
	body.setPos(0, 0 + yOffset, 0);

	arm0.addBox(-3, -2, -2, 4, 12, 4, g); // Arm0
	arm0.setPos(-5, 2 + yOffset, 0);

	arm1.mirror = true;
	arm1.addBox(-1, -2, -2, 4, 12, 4, g); // Arm1
	arm1.setPos(5, 2 + yOffset, 0);

	leg0.addBox(-2, 0, -2, 4, 12, 4, g); // Leg0
	leg0.setPos(-2, 12 + yOffset, 0);

	leg1.mirror = true;
	leg1.addBox(-2, 0, -2, 4, 12, 4, g); // Leg1
	leg1.setPos(2, 12 + yOffset, 0);
}

void HumanoidModel::render(Entity* e, float time, float r, float bob, float yRot, float xRot, float scale )
{
	if(e != NULL && e->isMob()) {		
		Mob* mob = (Mob*)(e);
		ItemInstance* item = mob->getCarriedItem();
		if(item != NULL) {
			if(mob->getUseItemDuration() >  0) {
				UseAnim::UseAnimation anim = item->getUseAnimation();
				if(anim == UseAnim::bow) {
					bowAndArrow = true;
				}
			}
		}
	}
	
	setupAnim(time, r, bob, yRot, xRot, scale);
	
	head.render(scale);
	body.render(scale);
	arm0.render(scale);
	arm1.render(scale);
	leg0.render(scale);
	leg1.render(scale);
	bowAndArrow = false;
	//hair.render(scale);
}

void HumanoidModel::render( HumanoidModel* model, float scale )
{
	head.yRot = model->head.yRot;
	head.y = model->head.y;
	head.xRot = model->head.xRot;
	//hair.yRot = head.yRot;
	//hair.xRot = head.xRot;

	arm0.xRot = model->arm0.xRot;
	arm0.zRot = model->arm0.zRot;

	arm1.xRot = model->arm1.xRot;
	arm1.zRot = model->arm1.zRot;

	leg0.xRot = model->leg0.xRot;
	leg1.xRot = model->leg1.xRot;

	head.render(scale);
	body.render(scale);
	arm0.render(scale);
	arm1.render(scale);
	leg0.render(scale);
	leg1.render(scale);
	//hair.render(scale);
}

void HumanoidModel::renderHorrible( float time, float r, float bob, float yRot, float xRot, float scale )
{
	setupAnim(time, r, bob, yRot, xRot, scale);
	head.renderHorrible(scale);
	body.renderHorrible(scale);
	arm0.renderHorrible(scale);
	arm1.renderHorrible(scale);
	leg0.renderHorrible(scale);
	leg1.renderHorrible(scale);
	//hair.renderHorrible(scale);
}
// Updated to match Minecraft Java, all except hair.
void HumanoidModel::setupAnim( float time, float r, float bob, float yRot, float xRot, float scale )
{
	head.yRot = yRot / (180 / Mth::PI);
	head.xRot = xRot / (180 / Mth::PI);
	const float tcos0 = Mth::cos(time * 0.6662f) * r;
	const float tcos1 = Mth::cos(time * 0.6662f + Mth::PI) * r;

	arm0.xRot = tcos1;
	arm1.xRot = tcos0;
	arm0.zRot = 0;
	arm1.zRot = 0;

	leg0.xRot = tcos0 * 1.4f;
	leg1.xRot = tcos1 * 1.4f;
	leg0.yRot = 0;
	leg1.yRot = 0;

	if (riding) {
		arm0.xRot += -Mth::PI / 2 * 0.4f;
		arm1.xRot += -Mth::PI / 2 * 0.4f;
		leg0.xRot = -Mth::PI / 2 * 0.8f;
		leg1.xRot = -Mth::PI / 2 * 0.8f;
		leg0.yRot = Mth::PI / 2 * 0.2f;
		leg1.yRot = -Mth::PI / 2 * 0.2f;
	}

	if (holdingLeftHand != 0) {
		arm1.xRot = arm1.xRot * 0.5f - Mth::PI / 2.0f * 0.2f * holdingLeftHand;
	}
	if (holdingRightHand != 0) {
		arm0.xRot = arm0.xRot * 0.5f - Mth::PI / 2.0f * 0.2f * holdingRightHand;
	}
	arm0.yRot = 0;
	arm1.yRot = 0;

	if (attackTime > -9990) {
		float swing = attackTime;
		body.yRot = Mth::sin(Mth::sqrt(swing) * Mth::PI * 2) * 0.2f;
		arm0.z = Mth::sin(body.yRot) * 5;
		arm0.x = -Mth::cos(body.yRot) * 5;
		arm1.z = -Mth::sin(body.yRot) * 5;
		arm1.x = Mth::cos(body.yRot) * 5;
		arm0.yRot += body.yRot;
		arm1.yRot += body.yRot;
		arm1.xRot += body.yRot;

		swing = 1 - attackTime;
		swing *= swing;
		swing *= swing;
		swing = 1 - swing;
		float aa = Mth::sin(swing * Mth::PI);
		float bb = Mth::sin(attackTime * Mth::PI) * -(head.xRot - 0.7f) * 0.75f;
		arm0.xRot -= aa * 1.2f + bb;
		arm0.yRot += body.yRot * 2;
		arm0.zRot = Mth::sin(attackTime * Mth::PI) * -0.4f;
	}

	if (sneaking) {
		body.xRot = 0.5f;
		arm0.xRot += 0.4f;
		arm1.xRot += 0.4f;
		leg0.z = +4.0f;
		leg1.z = +4.0f;
		leg0.y = +9.0f;
		leg1.y = +9.0f;
		head.y = +1;
	} else {
		body.xRot = 0.0f;
		leg0.z = +0.0f;
		leg1.z = +0.0f;
		leg0.y = +12.0f;
		leg1.y = +12.0f;
		head.y = 0;
	}

	const float bcos = Mth::cos(bob * 0.09f) * 0.05f + 0.05f;
	const float bsin = Mth::sin(bob * 0.067f) * 0.05f;

	arm0.zRot += bcos;
	arm1.zRot -= bcos;
	arm0.xRot += bsin;
	arm1.xRot -= bsin;

	if (bowAndArrow) {
		float attack2 = 0;
		float attack = 0;

		arm0.zRot = 0;
		arm1.zRot = 0;
		arm0.yRot = -(0.1f - attack2 * 0.6f) + head.yRot;
		arm1.yRot = +(0.1f - attack2 * 0.6f) + head.yRot + 0.4f;
		arm0.xRot = -Mth::PI / 2.0f + head.xRot;
		arm1.xRot = -Mth::PI / 2.0f + head.xRot;
		arm0.xRot -= attack2 * 1.2f - attack * 0.4f;
		arm1.xRot -= attack2 * 1.2f - attack * 0.4f;

		arm0.zRot += bcos;
		arm1.zRot -= bcos;
		arm0.xRot += bsin;
		arm1.xRot -= bsin;
	}

	// -----------------------------------------------------------------
	// 模组（玩家动作）：按玩家分别覆盖部件姿态。
	// 只覆盖"这个动作返回了旋转"的部件，其余保持上面的原版姿态 —— 所以
	// "游泳"只管手臂/腿，抬头看、走路摆臂、挥手等原版行为都还在。
	// scriptPlayerId 只有 PlayerRenderer 画玩家身体时才会 >= 0。
	// 旋转单位是弧度（和生物的 anim 一致）：1.57 ≈ 90°，3.14 ≈ 180°。
	// -----------------------------------------------------------------
#if defined(_WIN32) || defined(__ANDROID__)
	if (scriptPlayerId >= 0 && ModEngine::instance && ModEngine::instance->hasPlayerAction(scriptPlayerId)) {
		ModEngine* me = ModEngine::instance;
		ModPartPose pose;
		if (me->getPlayerAnim(scriptPlayerId, "head", scriptPlayerAge, time, scriptPlayerFirstPerson, pose)) applyPartPose(head, pose);
		if (me->getPlayerAnim(scriptPlayerId, "body", scriptPlayerAge, time, scriptPlayerFirstPerson, pose)) applyPartPose(body, pose);
		if (me->getPlayerAnim(scriptPlayerId, "arm0", scriptPlayerAge, time, scriptPlayerFirstPerson, pose)) applyPartPose(arm0, pose);
		if (me->getPlayerAnim(scriptPlayerId, "arm1", scriptPlayerAge, time, scriptPlayerFirstPerson, pose)) applyPartPose(arm1, pose);
		if (me->getPlayerAnim(scriptPlayerId, "leg0", scriptPlayerAge, time, scriptPlayerFirstPerson, pose)) applyPartPose(leg0, pose);
		if (me->getPlayerAnim(scriptPlayerId, "leg1", scriptPlayerAge, time, scriptPlayerFirstPerson, pose)) applyPartPose(leg1, pose);
	}
#endif
}

void HumanoidModel::onGraphicsReset()
{
	head.onGraphicsReset();
	body.onGraphicsReset();
	arm0.onGraphicsReset();
	arm1.onGraphicsReset();
	leg0.onGraphicsReset();
	leg1.onGraphicsReset();
	//hair.onGraphicsReset();
}

//void renderHair(float scale) {
//    hair.yRot = head.yRot;
//    hair.xRot = head.xRot;
//    hair.render(scale);
//}
//
//void renderEars(float scale) {
//    ear.yRot = head.yRot;
//    ear.xRot = head.xRot;
//    ear.x=0;
//    ear.y=0;
//    ear.render(scale);
//}
//
