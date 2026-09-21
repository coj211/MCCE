#version 450 compatibility

out vec3 sceneData;

in vec2 screenCoord;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

//----// STRUCTS //-------------------------------------------------------------------------------//

#include "/lib/Head/Mask.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

#include "/lib/Water/WaterFog.glsl"

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);

	int materialIDT = int(texelFetch(colortex7, texel, 0).z * 255.0);
	TranslucentMask materialMaskT = CalculateMasksT(materialIDT);

	sceneData = texelFetch(colortex4, texel, 0).rgb;
	if ((materialMaskT.water || materialMaskT.ice) && isEyeInWater == 0) {
		float depthSoild = GetDepthSoild(texel);
		vec3 viewPos = ScreenToViewSpace(vec3(screenCoord, GetDepth(texel)));
		vec3 viewPosSoild = ScreenToViewSpace(vec3(screenCoord, depthSoild));

		// #if defined DISTANT_HORIZONS
		// 	if (depthSoild >= 1.0) {
		// 		viewPos = ScreenToViewSpaceDH(vec3(screenCoord, GetDepthDH(texel)));
		// 		viewPosSoild = ScreenToViewSpaceDH(vec3(screenCoord, GetDepthSoildDH(texel)));
		// 	}
		// #endif
		WaterFog(sceneData, materialMaskT, 0.0, 0.0, distance(viewPos, viewPosSoild));
	}
}

/* DRAWBUFFERS:4 */
