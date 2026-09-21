
uniform sampler2D noisetex;

uniform mat4 gbufferModelView;
uniform mat4 gbufferModelViewInverse;
uniform vec3 cameraPosition;
uniform float frameTimeCounter;
uniform float wetnessCustom;

uniform vec2 taaOffset;

#ifndef MC_GL_VENDOR_INTEL
	#define attribute in
#endif

attribute vec4 mc_Entity;
attribute vec4 mc_midTexCoord;
attribute vec4 at_tangent;

out vec4 tint;
out vec2 texcoord;
out vec3 minecraftPos;
out vec3 viewPos;

flat out mat3 tbnMatrix;

flat out uint materialIDs;

out vec2 lightmap;

#include "/lib/Head/Common.inc"

#if defined PARALLAX || ANISOTROPIC_FILTER > 0
	out vec2 tileCoord;
	flat out vec2 tileOffset;
	flat out vec2 tileScale;
#endif

#define PLANTS_WAVE_EFFECTS
//#define PLANTS_TOUCH_EFFECTS

void main() {
	tint = gl_Color;
	texcoord = gl_MultiTexCoord0.xy;

	lightmap = saturate(gl_MultiTexCoord1.xy * rcp(240.0));

	#if defined PARALLAX || ANISOTROPIC_FILTER > 0
		vec2 midCoord = (gl_TextureMatrix[0] * mc_midTexCoord).xy;
		vec2 minMidCoord = texcoord - midCoord;
		tileOffset = min(texcoord, midCoord - minMidCoord);
		tileScale = abs(minMidCoord) * 2.0;
		tileCoord = sign(minMidCoord) * 0.5 + 0.5;
	#endif

	vec4 position = gbufferModelViewInverse * gl_ModelViewMatrix * gl_Vertex;

    tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
	#if defined MC_NORMAL_MAP || defined RAIN_SPLASH_EFFECT
		tbnMatrix[0] = normalize(gl_NormalMatrix * at_tangent.xyz);
		tbnMatrix[1] = cross(tbnMatrix[0], tbnMatrix[2]) * sign(at_tangent.w);
	#endif

	materialIDs = uint(max0(mc_Entity.x - 1e4));

	#ifdef MOD_BLOCK_SUPPORT
	#endif

	#ifdef PLANTS_TOUCH_EFFECTS
		if (materialIDs == 1u || materialIDs == 2u) {
			if (length((position.xyz + vec3(0.0, 2.0, 0.0))) < 2.0) position.xz *= 1.0 + max0(5.0 / max(length((position.xyz + vec3(0.0, 2.0, 0.0)) * vec3(8.0, 2.0, 8.0) - vec3(0.0, 2.0, 0.0)), 2.0) - 0.625);
		}
		if (materialIDs == 4u || materialIDs == 5u) {
			if (length(position.xyz) < 2.0) position.xz *= 1.0 + max0(5.0 / max(length(position.xyz * vec3(8.0, 2.0, 8.0)), 2.0) - 0.625);
		}
	#endif

	position.xyz += cameraPosition.xyz;

	#ifdef PLANTS_WAVE_EFFECTS
		float tick = frameTimeCounter * PI;

		float grassWeight = step(gl_MultiTexCoord0.y, mc_midTexCoord.y);
		const float lightWeight = pow4(saturate(lightmap.y * 1.5 - 0.5));

		if (materialIDs == 5u) {
			grassWeight *= 0.8;
		} else if (materialIDs == 4u) {
			grassWeight = grassWeight * 0.8 + 0.8;
		}

		// Grass
		if (materialIDs > 0u && materialIDs < 6u) {
			vec2 noise = texture(noisetex, position.xz * rcp(256.0) + sin(tick * 7e-4) * 2.0 - 1.0).xy * 1.3 - 0.3;
			vec2 wave = sin(dot(position.xz, vec2(1.0)) + tick) * fma(wetnessCustom, 0.12, 0.06) * noise - cossin(PI * 0.2) * position.y * 0.0015;
			position.xz += wave * grassWeight * lightWeight;
		}

		// Leaves
		if (materialIDs == 7u) {
			vec2 noise = texture(noisetex, position.xz * rcp(256.0) + sin(tick * 7e-4) * 2.0 - 1.0).xy * 1.3 - 0.3;
			vec3 wave = sin(dot(position.xyz, vec3(1.0)) + tick) * vec3(noise.x, noise.x * noise.y, noise.y);
			position.xyz += wave * fma(wetnessCustom, 0.12, 0.06) * lightWeight;
		}
	#endif

	if (materialIDs > 0u) { materialIDs = max(materialIDs, 6u); }
	#ifdef GENERAL_GRASS_FIX
	else if (abs(gl_Normal.x) > 0.01 && abs(gl_Normal.x) < 0.99 ||
			 abs(gl_Normal.y) > 0.01 && abs(gl_Normal.y) < 0.99 ||
			 abs(gl_Normal.z) > 0.01 && abs(gl_Normal.z) < 0.99
			) materialIDs = 6u;
	#endif

	minecraftPos = position.xyz;

	position.xyz -= cameraPosition;
	gl_Position = gl_ProjectionMatrix * gbufferModelView * position;
	
	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	viewPos = mat3(gl_ModelViewMatrix) * gl_Vertex.xyz + gl_ModelViewMatrix[3].xyz;
}