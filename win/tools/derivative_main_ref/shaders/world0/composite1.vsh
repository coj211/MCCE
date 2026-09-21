#version 450 compatibility


out vec2 screenCoord;

flat out vec3 directIlluminance;
//out vec3 colorMistlight;
flat out vec3 skyIlluminance;


#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"
//#include "/lib/Atmosphere/Atmosphere.glsl"


void main() {
	gl_Position = vec4(gl_Vertex.xy * 2.0 - 1.0, 0.0, 1.0);
	screenCoord = gl_MultiTexCoord0.xy;

	directIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 0), 0).rgb;
	//colorMistlight = CumulusSunlightColor();
/*
	vec4 skySHR,
	skySHG,
	skySHB;

	GetSkylightData(worldSunVector,
		skySHR, skySHG, skySHB,
		skyIlluminance);
*/
	skyIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 1), 0).rgb;
}
