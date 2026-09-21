#version 450 compatibility


#define IS_NETHER

layout(location = 0) out vec2 specularData;
layout(location = 1) out vec3 sceneData;

const float shadowDistanceRenderMul = 0.0;

uniform sampler2D shadowtex0;

//uniform vec3 fogColor;

in vec2 screenCoord;

flat in vec3 ambientColor;
flat in vec3 blocklightColor;

//in vec3 worldLightVector;
//in vec3 worldSunVector;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

//----// STRUCTS //-------------------------------------------------------------------------------//

#include "/lib/Head/Mask.inc"
#include "/lib/Head/Material.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

#include "/lib/Atmosphere/Fogs.glsl"

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);

	float depth = GetDepthFix(texel);

	vec3 viewPos = ScreenToViewSpace(vec3(screenCoord, depth));

	#if defined DISTANT_HORIZONS
		if (depth >= 1.0) {
			depth = GetDepthDH(texel);
			viewPos = ScreenToViewSpaceDH(vec3(screenCoord, depth));
		}
	#endif

	if (depth < 1.0) {
		vec3 albedoRaw = texelFetch(colortex6, texel, 0).rgb;
		vec3 albedo = SRGBtoLinear(albedoRaw);

		vec4 gbuffer3 = texelFetch(colortex3, texel, 0);

		int materialID = int(texelFetch(colortex7, texel, 0).z * 255.0);
		//int materialIDT = int(texelFetch(colortex1, texel, 0).b * 255.0);

		//MaterialMask materialMask = CalculateMasks(materialID);
		//TranslucentMask materialMaskT = CalculateMasksT(materialIDT);

		vec3 worldPos = mat3(gbufferModelViewInverse) * viewPos;
		vec3 worldDir = normalize(worldPos);
		worldPos += gbufferModelViewInverse[3].xyz;

		vec2 mcLightmap = texelFetch(colortex7, texel, 0).rg;
		mcLightmap.g = cube(mcLightmap.g);

		vec3 normal = DecodeNormal(gbuffer3.xy);
		vec3 worldNormal = mat3(gbufferModelViewInverse) * normal;

		vec4 specTex = vec4(UnpackUnorm2x8(gbuffer3.z), UnpackUnorm2x8(gbuffer3.w));
		Material material = GetMaterialData(specTex);
		specTex.x = sqr(1.0 - specTex.x);
		specularData = specTex.xy;

		// Grass points up
		if (materialID == 6 || materialID == 27 || materialID == 28 || materialID == 33) worldNormal = vec3(0.0, 1.0, 0.0);

		float NdotV = saturate(dot(worldNormal, -worldDir));
		float metalMask = oneMinus(material.isMetal * 0.9);

		// Basic light
		sceneData = ambientColor * metalMask * (worldNormal.y + 3.0);

		// AO
		#ifdef SSAO_ENABLED
			float ao = texelFetch(colortex0, texel / 2, 0).a;
			sceneData *= ao;
		#else
			float ao = 1.0;
		#endif

		//torchlight
		#include "/lib/Lighting/BlockLighting.glsl"

		sceneData *= albedo;
		sceneData *= mix(vec3(1.0), NetherFogColor() * 0.5, material.isMetal);
	} else {
		specularData = vec2(1.0, 0.0);
	}

	sceneData = clamp16F(sceneData);
}

/* DRAWBUFFERS:04 */