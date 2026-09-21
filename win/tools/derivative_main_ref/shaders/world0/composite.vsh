#version 450 compatibility


flat out vec3 directIlluminance;
flat out vec3 skyIlluminance;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

#if defined CLOUDS_WEATHER && defined CLOUDS_SHADOW
	flat out vec3 cloudDynamicWeather;
#endif

void main() {
	gl_Position = vec4(gl_Vertex.xy * 2.0 - 1.0, 0.0, 1.0);

	directIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 0), 0).rgb;
	skyIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 1), 0).rgb;

	#if defined CLOUDS_WEATHER && defined CLOUDS_SHADOW
		cloudDynamicWeather = texelFetch(colortex5, ivec2(skyCaptureRes.x, 5), 0).xyz;
	#endif
}
