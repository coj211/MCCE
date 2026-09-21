#ifndef NET_MINECRAFT_CLIENT_PARTICLE_SCRIPTEDPARTICLE_H__
#define NET_MINECRAFT_CLIENT_PARTICLE_SCRIPTEDPARTICLE_H__

#include "Particle.h"

// Particle driven by JS (Particle.define). It uses a texture from the mod's
// zip package (registered in ModEngine::_modTextures) and follows the
// definition's size / lifetime / color / gravity / shrink behaviour.
class ScriptedParticle: public Particle {
	typedef Particle super;
public:
	struct Def {
		std::string name;
		unsigned int textureId; // GL texture id (0 = invalid)
		float size;
		int lifetime;
		float r, g, b;          // color tint; -1 = leave texture colors
		float gravity;
		bool shrink;            // fade/shrink towards the end of life
		Def()
		: name(), textureId(0), size(0.5f), lifetime(30),
		  r(-1.0f), g(-1.0f), b(-1.0f), gravity(0.0f), shrink(true) {}
	};

	ScriptedParticle(Level* level, float x, float y, float z, float xd, float yd, float zd, const Def& def);

	virtual void tick();
	virtual void render(Tesselator& t, float a, float xa, float ya, float za, float xa2, float za2, int texRows);
	virtual int getParticleTexture() { return 0; } // ParticleEngine::MISC_TEXTURE

	unsigned int getTextureId() const { return _def.textureId; }

private:
	Def _def;
	float _startSize;
};

#endif /*NET_MINECRAFT_CLIENT_PARTICLE_SCRIPTEDPARTICLE_H__*/
