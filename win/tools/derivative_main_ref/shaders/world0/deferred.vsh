#version 450 compatibility

//out vec2 screenCoord;

flat out vec3 directIlluminance;
flat out vec3 skyIlluminance;

flat out vec3 sunIlluminance;
flat out vec3 moonIlluminance;

#include "/lib/Head/Common.inc"

#ifdef CLOUDS_WEATHER
	flat out vec3 cloudDynamicWeather;
#endif

uniform float eyeAltitude;
uniform float nightVision;
uniform float wetness;
uniform float isLightningFlashing;
uniform float BiomeGreenShift;

uniform vec3 worldSunVector;
uniform vec3 worldLightVector;

uniform int moonPhase;

uniform vec2 screenPixelSize;

uniform sampler3D colortex4;

#define PRECOMPUTED_ATMOSPHERIC_SCATTERING
#include "/lib/Atmosphere/Atmosphere.glsl"

uniform int worldDay;
uniform int worldTime;

float hash1(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

uint triple32(uint x) {
    // https://nullprogram.com/blog/2018/07/31/
    x ^= x >> 17;
    x *= 0xed5ad4bbu;
    x ^= x >> 11;
    x *= 0xac4c1b51u;
    x ^= x >> 15;
    x *= 0x31848babu;
    x ^= x >> 14;
    return x;
}

#define RandWeather(state) vec2(hash1(triple32(uint(state)) / float(0xffffffffu)))

void main() {
	gl_Position.xy = gl_Vertex.xy * vec2(skyCaptureRes + ivec2(1, skyCaptureRes.y)) * screenPixelSize;
	gl_Position = vec4(gl_Position.xy * 2.0 - 1.0, 0.0, 1.0);
	//screenCoord = gl_Vertex.xy;

	vec3 camera = vec3(0.0, planetRadius + eyeAltitude, 0.0);
	// vec3 camera = vec3(0.0, planetRadius + 800.0, 0.0);
	skyIlluminance = GetSunAndSkyIrradiance(atmosphereModel, camera, worldSunVector, sunIlluminance, moonIlluminance);
	skyIlluminance = skyIlluminance * oneMinus(BiomeGreenShift * 0.5) + maxOf(skyIlluminance) * vec3(0.3, 1.0, 0.24) * BiomeGreenShift;
	directIlluminance = sunIlluminance + moonIlluminance;

	#ifdef CLOUDS_WEATHER
		// cloudDynamicWeather = texelFetch(noisetex, ivec2(worldDay) % 255, 0).rgb;
		// cloudDynamicWeather = mix(cloudDynamicWeather, texelFetch(noisetex, ivec2(worldDay + 1) % 255, 0).rgb, curve(fract(float(worldTime) * rcp(24000.0))));
		vec2 weatherMap = mix(RandWeather(worldDay), RandWeather(worldDay + 1), curve(fract(float(worldTime) * rcp(24000.0) + vec2(0.65, 0.25))));

		cloudDynamicWeather.x = curve(remap(0.25, 0.4, weatherMap.x)) * 0.5;
		cloudDynamicWeather.y = sqr(1.0 - remap(0.65, 0.8, weatherMap.y)) * 0.5;

		cloudDynamicWeather.z = remap(0.4, 0.55, weatherMap.x * 2.0 - weatherMap.y);
		cloudDynamicWeather.z *= 2.0 - cloudDynamicWeather.z;
	#endif
}
