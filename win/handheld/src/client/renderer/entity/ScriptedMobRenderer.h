#ifndef NET_MINECRAFT_CLIENT_RENDERER_ENTITY_SCRIPTEDMOBRENDERER_H__
#define NET_MINECRAFT_CLIENT_RENDERER_ENTITY_SCRIPTEDMOBRENDERER_H__

#include "MobRenderer.h"

// Renderer for scripted (JS-defined) mobs. Each mob carries its own
// ScriptedModel, so render() swaps the base class model per entity and
// feeds the age counter for time-driven animation.
class ScriptedMobRenderer: public MobRenderer {
	typedef MobRenderer super;
public:
	ScriptedMobRenderer();
	void render(Entity* e, float x, float y, float z, float rot, float a);
protected:
	// Prefer textures registered by zip mod packages; fall back to the
	// normal asset path.
	void bindTexture(const std::string& resourceName);
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER_ENTITY_SCRIPTEDMOBRENDERER_H__*/
