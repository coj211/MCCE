
layout (location = 0) out vec3 shadowcolor0Out;
layout (location = 1) out vec4 shadowcolor1Out;

#include "/lib/Head/Common.inc"

in vec2 texcoord;
in vec2 lightmap;

in vec3 tint;
in vec3 viewPos;
in vec3 minecraftPos;

flat in float isWater;

flat in mat3 tbnMatrix;

uniform sampler2D tex;

#ifdef WATER_CAUSTICS
	uniform sampler2D noisetex;

	uniform float frameTimeCounter;

	#if defined DISTANT_HORIZONS
		uniform float dhFarPlane;
	#else
		uniform float far;
	#endif

	// uniform vec3 cameraPosition;
	// uniform vec3 worldLightVector;
	// uniform mat4 shadowModelViewInverse;

	#include "/lib/Water/WaterWave.glsl"

	vec3 fastRefract(in vec3 dir, in vec3 normal, in float eta) {
		float NdotD = dot(normal, dir);
		float k = 1.0 - eta * eta * oneMinus(NdotD * NdotD);
		if (k < 0.0) return vec3(0.0);

		return dir * eta - normal * (sqrt(k) + NdotD * eta);
	}
#endif

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	if (isWater > 0.5) {
	#ifdef WATER_CAUSTICS
		vec3 wavesNormal = GetWavesNormal(minecraftPos.xz - minecraftPos.y);

		vec3 normal = tbnMatrix * wavesNormal;

		// vec3 oldPos = minecraftPos - cameraPosition;
		vec3 oldPos = viewPos;
		vec3 newPos = oldPos + fastRefract(vec3(0.0, 0.0, -1.0), normal, 1.0 / WATER_REFRACT_IOR) * 6.0;
		// vec3 newPos = oldPos + refract(worldLightVector, (mat3(shadowModelViewInverse) * normal).xzy, 1.0 / WATER_REFRACT_IOR);

		float oldArea = dotSelf(dFdx(oldPos)) * dotSelf(dFdy(oldPos));
		float newArea = dotSelf(dFdx(newPos)) * dotSelf(dFdy(newPos));

		float caustics = inversesqrt(oldArea / newArea) * 0.3;

		shadowcolor0Out = vec3(sqrt2(caustics));
		shadowcolor1Out.xy = EncodeNormal(normal);
		shadowcolor1Out.w = minecraftPos.y * rcp(512.0) + 0.25;
	#else
		shadowcolor0Out = vec3(0.8);
		shadowcolor1Out.xy = EncodeNormal(tbnMatrix[2]);
	#endif
	} else {
		vec4 albedo = texture(tex, texcoord);
		if (albedo.a < 0.1) discard;

        if (albedo.a > 254.0 / 255.0) {
			shadowcolor0Out = albedo.rgb * tint;
		} else {
			shadowcolor0Out = mix(vec3(1.0), albedo.rgb * tint, pow(albedo.a, 0.4));
		}
		shadowcolor1Out.xy = EncodeNormal(tbnMatrix[2]);
	}

	shadowcolor1Out.z = lightmap.y;
}
