#version 450 compatibility

//out vec2 screenCoord;

flat out vec3 sunIlluminance;
flat out vec3 moonIlluminance;
flat out vec3 skyIlluminance;

#include "/lib/Head/Common.inc"

#ifdef CLOUDS_WEATHER
	flat out vec3 cloudDynamicWeather;
#endif

uniform sampler2D colortex5;

void main() {
	gl_Position = vec4(gl_Vertex.xy * (2.0 / TEMPORAL_UPSCALING) - 1.0, 0.0, 1.0);
	//screenCoord = gl_Vertex.xy;

	sunIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 2), 0).rgb;
	moonIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 3), 0).rgb;
	skyIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 1), 0).rgb;

	#ifdef CLOUDS_WEATHER
		cloudDynamicWeather = texelFetch(colortex5, ivec2(skyCaptureRes.x, 5), 0).xyz;
	#endif
}
