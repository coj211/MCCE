#version 450 compatibility

#define IS_END


layout(location = 0) out vec4 fogData;
layout(location = 1) out vec3 sceneData;


uniform sampler2D shadowtex0;
uniform sampler2D shadowtex1;
uniform sampler2D shadowcolor0;
uniform sampler2D shadowcolor1;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"
#include "/lib/Atmosphere/Atmosphere.glsl"

//----// STRUCTS //-------------------------------------------------------------------------------//

#include "/lib/Head/Mask.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

#include "/lib/Atmosphere/VolumetricFogEnd.glsl"

#include "/lib/Water/WaterFog.glsl"

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 screenCoord = gl_FragCoord.xy * screenPixelSize;

	int materialIDT = int(texelFetch(colortex7, texel, 0).z * 255.0);
	TranslucentMask materialMaskT = CalculateMasksT(materialIDT);

	sceneData = texelFetch(colortex4, texel, 0).rgb;
	if ((materialMaskT.water || materialMaskT.ice) && isEyeInWater == 0) {
		float depthSoild = GetDepthSoild(texel);
		vec3 viewPos = ScreenToViewSpace(vec3(screenCoord, GetDepth(texel)));
		vec3 viewPosSoild = ScreenToViewSpace(vec3(screenCoord, depthSoild));

		#if defined DISTANT_HORIZONS
			if (depthSoild >= 1.0) {
				viewPos = ScreenToViewSpaceDH(vec3(screenCoord, GetDepthDH(texel)));
				viewPosSoild = ScreenToViewSpaceDH(vec3(screenCoord, GetDepthSoildDH(texel)));
			}
		#endif
		vec3 worldDir = mat3(gbufferModelViewInverse) * normalize(viewPos);
		float LdotV = dot(worldLightVector, worldDir);
		float skyLightmap = cube(texelFetch(colortex7, texel, 0).g);
		WaterFog(sceneData, materialMaskT, 1.0, LdotV, distance(viewPos, viewPosSoild));
	}

	if (any(greaterThanEqual(screenCoord, vec2(0.5)))) return;

	texel *= 2;
	screenCoord *= 2.0;
	float depth = GetDepth(texel);
	vec3 viewPos = ScreenToViewSpaceRaw(vec3(screenCoord, depth));

	#if defined DISTANT_HORIZONS
		if (depth >= 1.0) viewPos = ScreenToViewSpaceRawDH(vec3(screenCoord, GetDepthDH(texel)));
	#endif

	vec3 worldPos = mat3(gbufferModelViewInverse) * viewPos;
	vec3 worldDir = normalize(worldPos);
	// worldPos += gbufferModelViewInverse[3].xyz;

	fogData = vec4(0.0, 0.0, 0.0, 1.0);
	float dither = R1(frameCounter, texelFetch(noisetex, texel & 255, 0).a);

	if (isEyeInWater == 0) fogData = CalculateVolumetricFog(worldPos, worldDir, dither);

	#ifdef UW_VOLUMETRIC_LIGHT
		if (isEyeInWater == 1) fogData.rgb = UnderwaterVolumetricLight(worldPos, worldDir, dither);
	#endif
}

/* DRAWBUFFERS:14 */
