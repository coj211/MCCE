#include "ScriptedMobRenderer.h"

#include "../../../world/entity/monster/ScriptedMob.h"
#include "../../model/ScriptedModel.h"
#include "../../../mod/ModEngine.h"
#include "../gles.h"

ScriptedMobRenderer::ScriptedMobRenderer()
:	MobRenderer(NULL, 0.5f)
{
}

void ScriptedMobRenderer::bindTexture(const std::string& texture) {
	// Textures shipped inside the mod's zip package win over assets.
	if (ModEngine::instance) {
		unsigned int modTex = ModEngine::instance->getModTexture(texture);
		if (modTex) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, modTex);
			return;
		}
	}
	super::bindTexture(texture);
}

void ScriptedMobRenderer::render(Entity* e, float x, float y, float z, float rot, float a)
{
	ScriptedMob* mob = (ScriptedMob*)e;
	ScriptedModel* model = mob->getScriptedModel();
	if (!model) {
		// No registered model; nothing sensible to draw.
		return;
	}
	model->currentAge = (float)mob->tickCount;
	model->scriptPlayerId = -1;   // 这是生物，不是玩家穿着的；模型对象是共享的
	Model* saved = getModel();
	setModel(model);
	super::render(e, x, y, z, rot, a);
	setModel(saved);
}
