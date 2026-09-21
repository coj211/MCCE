#include "ScriptedParticle.h"

#include "ParticleEngine.h"
#include "../renderer/Tesselator.h"
#include "../../world/level/Level.h"
#include "../../util/Mth.h"

ScriptedParticle::ScriptedParticle(Level* level, float x, float y, float z, float xd, float yd, float zd, const Def& def)
:	super(level, x, y, z, xd, yd, zd),
	_def(def),
	_startSize(def.size)
{
	// Overwrite the randomized velocities from the base constructor with
	// the exact ones the mod asked for.
	this->xd = xd;
	this->yd = yd;
	this->zd = zd;
	this->lifetime = def.lifetime > 0 ? def.lifetime : 30;
	this->size = def.size;
	if (def.r >= 0) { rCol = def.r; gCol = def.g; bCol = def.b; }
	gravity = def.gravity;
}

void ScriptedParticle::tick() {
	xo = x;
	yo = y;
	zo = z;
	if (age++ >= lifetime) {
		remove();
		return;
	}
	if (gravity != 0.0f)
		yd -= gravity * 0.04f;
	move(xd, yd, zd);
	xd *= 0.94f;
	yd *= 0.94f;
	zd *= 0.94f;
	if (onGround) {
		xd *= 0.7f;
		zd *= 0.7f;
	}
}

void ScriptedParticle::render(Tesselator& t, float a, float xa, float ya, float za, float xa2, float za2, int texRows) {
	// Full-texture quad (0..1 UV). texRows unused: scripted particles use their own texture.
	const float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;

	float sz = _def.size;
	if (_def.shrink && lifetime > 0)
		sz = _startSize * (float)(lifetime - age) / (float)lifetime;
	float r = 0.1f * sz;

	float x = (float)(xo + (this->x - xo) * a - xOff);
	float y = (float)(yo + (this->y - yo) * a - yOff);
	float z = (float)(zo + (this->z - zo) * a - zOff);

	float rr = rCol, gg = gCol, bb = bCol;
	if (_def.r >= 0) {
		rr = _def.r; gg = _def.g; bb = _def.b;
	}
	t.color(rr, gg, bb);
	t.vertexUV(x - xa * r - xa2 * r, y - ya * r, z - za * r - za2 * r, u1, v1);
	t.vertexUV(x - xa * r + xa2 * r, y + ya * r, z - za * r + za2 * r, u1, v0);
	t.vertexUV(x + xa * r + xa2 * r, y + ya * r, z + za * r + za2 * r, u0, v0);
	t.vertexUV(x + xa * r - xa2 * r, y - ya * r, z + za * r - za2 * r, u0, v1);
}
