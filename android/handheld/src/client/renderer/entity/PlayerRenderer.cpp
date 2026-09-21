#include "PlayerRenderer.h"
#include "EntityRenderDispatcher.h"
#include "../../../world/entity/player/Player.h"
#include "../../../world/level/Level.h"
#include "../../../world/item/ArmorItem.h"
#include "../../../mod/ModEngine.h"
#include "../../Minecraft.h"
#include "../../model/ScriptedModel.h"

static const std::string armorFilenames[10] = {
	"armor/cloth_1.png",	"armor/cloth_2.png",
	"armor/chain_1.png",	"armor/chain_2.png",
	"armor/iron_1.png",		"armor/iron_2.png",
	"armor/diamond_1.png",	"armor/diamond_2.png",
	"armor/gold_1.png",		"armor/gold_2.png",
};

PlayerRenderer::PlayerRenderer( HumanoidModel* humanoidModel, float shadow )
:	super(humanoidModel, shadow),
	armorParts1(new HumanoidModel(1.0f)),
	armorParts2(new HumanoidModel(0.5f))
{
}

PlayerRenderer::~PlayerRenderer() {
	delete armorParts1;
	delete armorParts2;
}

void PlayerRenderer::setupPosition( Entity* mob, float x, float y, float z ) {
	Player* player = (Player*) mob;
	if(player->isAlive() && player->isSleeping()) {
		return super::setupPosition(mob, x + player->bedOffsetX, y + player->bedOffsetY, z + player->bedOffsetZ);
	}
	return super::setupPosition(mob, x, y, z);
}

void PlayerRenderer::setupRotations( Entity* mob, float bob, float bodyRot, float a ) {
	Player* player = (Player*) mob;
	if(player->isAlive() && player->isSleeping()) {
		glRotatef(player->getSleepRotation(), 0, 1, 0);
		glRotatef(getFlipDegrees(player), 0, 0, 1);
		glRotatef(270, 0, 1, 0);
		return;
	}
	super::setupRotations(mob, bob, bodyRot, a);
}

void PlayerRenderer::renderName( Mob* mob, float x, float y, float z ){
	//@todo: figure out how to handle HideGUI
	if (mob != entityRenderDispatcher->cameraEntity && mob->level->adventureSettings.showNameTags) {
		renderNameTag(mob, ((Player*)mob)->name, x, y, z, 32);
	}
}

// 玩家皮肤 64x64 时显示 outer 第二层(帽子/外套等), 否则隐藏(老式单层/无皮肤)。
void PlayerRenderer::render( Entity* mob_, float x, float y, float z, float rot, float a ) {
	HumanoidModel* m = getHumanoidModel();
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	const bool outer = me && ModEngine::isPlayerSkin64();
	const int pid = mob_ ? mob_->entityId : -1;

	// ---- 模组外形：把这个玩家换成别的样子（用自定义生物那份外形）----
	// 临时换上那份模型 + 贴图，画完立刻还原（不影响其他实体、纸娃娃）。
	ScriptedModel* extModel = NULL;
	Model* savedModel = NULL;
	std::string savedTex;
	bool texSwapped = false, headHidden = false;
	if (me && mc && pid >= 0 && mob_->isMob()) {
		Model* ext = me->getPlayerModelObject(pid);
		if (ext) {
			extModel = (ScriptedModel*)ext;
			savedModel = getModel();
			setModel(extModel);
			// 告诉外形“现在是我在穿着它”：它会自动套上走路摆臂这类人形动作，
			// 还能接住玩家自己的动作（Player.defineAction）。
			extModel->scriptPlayerId = pid;
			extModel->scriptPlayerAge = (float)mob_->tickCount;
			extModel->scriptPlayerFirstPerson = false;
			std::string tex = me->getPlayerModelTexture(pid);
			if (!tex.empty()) {
				savedTex = ((Mob*)mob_)->getTexture();
				((Mob*)mob_)->setTextureName(tex);
				texSwapped = true;
			}
			// 第一人称（摄像机就在这个玩家的眼睛位置）：把头藏起来，否则摄像机
			// 卡在头里面，屏幕会被头内壁糊满。身体/手臂照常画 —— 于是第一人称
			// 看到的就是这个模型真实的样子（手臂伸在前面就看得见）。
			if (mc->cameraTargetPlayer == mob_ && !mc->options.thirdPersonView) {
				ModelPart* head = extModel->getPart("head");
				if (head) { head->visible = false; headHidden = true; }
			}
		}
	}
	if (m) {
		m->setOuterVisible(outer);
		// 模组（玩家动作）：告诉模型"现在画的是哪个玩家"，按玩家分别取动作。
		// 只在这一个玩家渲染期间生效，渲染完立刻还原 —— 免得纸娃娃、第一人称手
		// 或其他实体捡到别人的动作。
		m->scriptPlayerId = mob_ ? mob_->entityId : -1;
		m->scriptPlayerAge = mob_ ? (float)mob_->tickCount : 0.0f;
		m->scriptPlayerFirstPerson = false;   // 第三人称身体
	}
	super::render(mob_, x, y, z, rot, a);

	if (m) m->scriptPlayerId = -1;
	if (headHidden && extModel) {
		ModelPart* head = extModel->getPart("head");
		if (head) head->visible = true;
	}
	if (texSwapped) ((Mob*)mob_)->setTextureName(savedTex);
	if (extModel) {
		extModel->scriptPlayerId = -1;   // 还原：别让后面的生物/其他人捡到
		setModel(savedModel);
	}
}

int PlayerRenderer::prepareArmor(Mob* mob, int layer, float a) {
	Player* player = (Player*) mob;

	ItemInstance* itemInstance = player->getArmor(layer);
	if (!ItemInstance::isArmorItem(itemInstance))
		return -1;

	ArmorItem* armorItem = (ArmorItem*) itemInstance->getItem();
	int fnIndex = (armorItem->modelIndex + armorItem->modelIndex) + (layer == 2 ? 1 : 0);
	bindTexture(armorFilenames[fnIndex]);

	HumanoidModel* armor = layer == 2 ? armorParts2 : armorParts1;

	armor->head.visible = layer == 0;
	//armor.hair.visible = layer == 0;
	armor->body.visible = layer == 1 || layer == 2;
	armor->arm0.visible = layer == 1;
	armor->arm1.visible = layer == 1;
	armor->leg0.visible = layer == 2 || layer == 3;
	armor->leg1.visible = layer == 2 || layer == 3;

	setArmor(armor);

	/*if (itemInstance.isEnchanted())
		return 15; */

	return 1;
}

void PlayerRenderer::onGraphicsReset() {
	super::onGraphicsReset();

	if (armorParts1) armorParts1->onGraphicsReset();
	if (armorParts2) armorParts2->onGraphicsReset();
}
