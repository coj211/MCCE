#include "ModProjectileRenderer.h"

#include "../Tesselator.h"
#include "../TileRenderer.h"
#include "../gles.h"
#include "../../../mod/ModEngine.h"
#include "../../../world/entity/Entity.h"
#include "../../../world/level/tile/ModBlockPart.h"

#include <vector>

void ModProjectileRenderer::render(Entity* entity, float x, float y, float z, float rot, float a)
{
	ModEngine* me = ModEngine::instance;
	if (me == NULL)
		return;

	// 类型号：ModProjectileEntity 用 getAuxData() 把自己的投射物类型传出来
	// （Entity::getAuxData 是 virtual，Arrow 也覆写了它）
	int typeId = entity->getAuxData();
	const std::vector<ModBlockPart>* parts = me->projectileModelParts(typeId);
	if (parts == NULL || parts->empty())
		return;                     // 没给模型就不画（命中照常算）

	// 贴图：模组包里的 png 是独立 GL 纹理，不在引擎纹理库里 ——
	// 必须直接绑（走 loadAndBindTexture(路径) 会查不到、然后沿用上一个纹理，
	// 这正是之前 AK47 贴图显示成"方块图集"的原因）。
	const std::string* tex = me->projectileTexture(typeId);
	if (tex != NULL && !tex->empty()) {
		unsigned int t = me->getModTexture(*tex);
		if (t != 0) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, t);
		}
	}

	glPushMatrix2();
	glTranslatef(x, y, z);

	// 模型部件是"格内 0..1"的盒子（一整格大），投射物要按定义里的 size 缩小，
	// 并往负方向挪半个身位，让实体位置落在它中心。
	float s = me->projectileSize(typeId);
	if (s <= 0.0f) s = 0.2f;
	glTranslatef(-s * 0.5f, -s * 0.5f, -s * 0.5f);
	glScalef(s, s, s);

	// renderModelBox 只是往 Tesselator 写顶点，不自己 begin/draw；
	// 它也不需要 level（传 NULL 安全，但要保证每个面的 tex[] 不是 -1）。
	static TileRenderer s_tileRenderer;
	Tesselator& tess = Tesselator::instance;
	tess.begin();
	for (size_t i = 0; i < parts->size(); ++i) {
		ModBlockPart p = (*parts)[i];
		s_tileRenderer.renderModelBox(p, NULL, 0.0f, 0.0f, 0.0f, 1.0f);
	}
	tess.draw();

	glPopMatrix2();
}
