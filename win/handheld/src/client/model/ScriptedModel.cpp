#include "ScriptedModel.h"

#include "../../mod/ModEngine.h"
#include "../../util/Mth.h"
#include "geom/ModelPart.h"

ScriptedModel::ScriptedModel(const std::string& animKey, const std::vector<ScriptedBoxDef>& boxes, int texW, int texH)
:	currentAge(0),
	_scaleFactor(1.0f),
	_animKey(animKey),
	scriptPlayerId(-1),
	scriptPlayerAge(0),
	scriptPlayerFirstPerson(false)
{
	this->texWidth = texW;
	this->texHeight = texH;
	for (size_t i = 0; i < boxes.size(); ++i) {
		const ScriptedBoxDef& b = boxes[i];
		ModelPart* part = new ModelPart(b.texX, b.texY);
		part->setModel(this);
		part->addBox(b.x, b.y, b.z, (int)b.w, (int)b.h, (int)b.d);
		part->setPos(b.px, b.py, b.pz);
		_parts.push_back(std::make_pair(b.name, part));
	}
}

ScriptedModel::~ScriptedModel() {
	for (size_t i = 0; i < _parts.size(); ++i)
		delete _parts[i].second;
	_parts.clear();
}

ModelPart* ScriptedModel::getPart(const std::string& name) const {
	for (size_t i = 0; i < _parts.size(); ++i)
		if (_parts[i].first == name)
			return _parts[i].second;
	return NULL;
}

// 人形自然动作：把原版人形模型那套走路摆动套到同名部件上（不动位置，只动旋转）。
// 这样“玩家穿外形”不会变成一块木头；之后 mod 自己的 anim 和玩家动作还能覆盖它。
void ScriptedModel::applyHumanoidMotion(float time, float r, float bob, float yRot, float xRot) {
	const float tcos0 = Mth::cos(time * 0.6662f) * r;
	const float tcos1 = Mth::cos(time * 0.6662f + Mth::PI) * r;
	ModelPart* p;
	if ((p = getPart("head"))) { p->yRot = yRot / (180 / Mth::PI); p->xRot = xRot / (180 / Mth::PI); }
	if ((p = getPart("arm0"))) { p->xRot = tcos1; p->yRot = 0; p->zRot = 0; }
	if ((p = getPart("arm1"))) { p->xRot = tcos0; p->yRot = 0; p->zRot = 0; }
	if ((p = getPart("leg0"))) { p->xRot = tcos0 * 1.4f; p->yRot = 0; }
	if ((p = getPart("leg1"))) { p->xRot = tcos1 * 1.4f; p->yRot = 0; }
	const float bcos = Mth::cos(bob * 0.09f) * 0.05f + 0.05f;
	const float bsin = Mth::sin(bob * 0.067f) * 0.05f;
	if ((p = getPart("arm0"))) { p->zRot += bcos; p->xRot += bsin; }
	if ((p = getPart("arm1"))) { p->zRot -= bcos; p->xRot -= bsin; }
}

void ScriptedModel::setupAnim(float time, float r, float bob, float yRot, float xRot, float scale) {
	ModEngine* me = ModEngine::instance;

	// 被玩家穿着：先套上人形自然动作（走路摆臂/摆腿/抬头）。
	if (scriptPlayerId >= 0)
		applyHumanoidMotion(time, r, bob, yRot, xRot);

	// mod 自己定义的动作（Mob.defineMob 的 anim）—— 覆盖人形的基础姿态。
	if (!_animKey.empty() && me) {
		for (size_t i = 0; i < _parts.size(); ++i) {
			float rx, ry, rz;
			if (me->getAnimRotation(_animKey, _parts[i].first, currentAge, time, rx, ry, rz)) {
				ModelPart* p = _parts[i].second;
				p->xRot = rx;
				p->yRot = ry;
				p->zRot = rz;
			}
		}
	}

	// 玩家动作（Player.defineAction）也套在外形上 —— 这样变了身还能游泳/挥手。
	if (me && scriptPlayerId >= 0 && me->hasPlayerAction(scriptPlayerId)) {
		ModPartPose pose;
		for (size_t i = 0; i < _parts.size(); ++i) {
			if (me->getPlayerAnim(scriptPlayerId, _parts[i].first, scriptPlayerAge, time, scriptPlayerFirstPerson, pose)) {
				ModelPart* p = _parts[i].second;
				p->xRot = pose.rx; p->yRot = pose.ry; p->zRot = pose.rz;
				if (pose.hasPos) { p->x = pose.px; p->y = pose.py; p->z = pose.pz; }
			}
		}
	}
}

void ScriptedModel::render(Entity* e, float time, float r, float bob, float yRot, float xRot, float scale) {
	setupAnim(time, r, bob, yRot, xRot, scale);
	scale *= _scaleFactor;
	for (size_t i = 0; i < _parts.size(); ++i)
		_parts[i].second->render(scale);
}
