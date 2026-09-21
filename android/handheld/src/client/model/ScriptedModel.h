#ifndef NET_MINECRAFT_CLIENT_MODEL_SCRIPTEDMODEL_H__
#define NET_MINECRAFT_CLIENT_MODEL_SCRIPTEDMODEL_H__

#include "Model.h"

#include <string>
#include <vector>

// One box of a scripted (JS-defined) mob model.
struct ScriptedBoxDef {
	std::string name;
	int texX, texY;
	float x, y, z;   // addBox origin (model space)
	float w, h, d;   // addBox size
	float px, py, pz; // setPos offset (attach point)
};

// Data-driven model for scripted mobs: the geometry is a list of boxes
// (like every 0.6.1 model), and per-frame animation is driven by a JS
// callback (Mob.defineMob's `anim`), which returns rotation radians for
// each named part.
class ScriptedModel: public Model {
public:
	ScriptedModel(const std::string& animKey, const std::vector<ScriptedBoxDef>& boxes, int texW, int texH);
	~ScriptedModel();

	// Uniform model scale (default 1). JS Mob.defineMob `scale` config.
	void setScale(float s) { _scaleFactor = s; }

	void setupAnim(float time, float r, float bob, float yRot, float xRot, float scale);
	// Model::render is a no-op in the base class; every concrete model must
	// override it to draw its parts (HumanoidModel does the same).
	void render(Entity* e, float time, float r, float bob, float yRot, float xRot, float scale);
	ModelPart* getPart(const std::string& name) const;

	// Age counter (ticks since spawn), set by the renderer each frame so
	// time-driven animations (wing flaps, idle bob) work.
	float currentAge;

	// 玩家穿着它渲染时由 PlayerRenderer 设置：scriptPlayerId >= 0 表示正被这个
	// 玩家穿着（-1 = 普通生物）。这时会额外套上“人形”的自然动作（走路摆臂/
	// 摆腿/抬头）和玩家自己的动作（Player.defineAction）——
	// 否则外形站着像块木头（原版玩家模型自带这些，脚本模型没有）。
	int scriptPlayerId;
	float scriptPlayerAge;
	bool scriptPlayerFirstPerson;

	// 人形自然动作：只对名字跟人形一致的部件（head/body/arm0/arm1/leg0/leg1）生效。
	void applyHumanoidMotion(float time, float r, float bob, float yRot, float xRot);

private:
	float _scaleFactor;
	std::string _animKey;
	std::vector<std::pair<std::string, ModelPart*> > _parts;
};

#endif /*NET_MINECRAFT_CLIENT_MODEL_SCRIPTEDMODEL_H__*/
