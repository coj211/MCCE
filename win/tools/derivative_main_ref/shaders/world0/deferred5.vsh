#version 450 compatibility


#include "/lib/Head/Common.inc"

out vec2 screenCoord;

flat out vec3 directIlluminance;
//out vec3 colorMistlight;
flat out vec3 skyIlluminance;
flat out vec3 blocklightColor;

flat out vec4 skySHR;
flat out vec4 skySHG;
flat out vec4 skySHB;

#if defined CLOUDS_WEATHER && defined CLOUDS_SHADOW
	flat out vec3 cloudDynamicWeather;
#endif

//----------------------------------------------------------------------------//

uniform sampler2D colortex5;

uniform float eyeAltitude;
uniform float nightVision;
uniform float wetness;
uniform float isLightningFlashing;

uniform vec3 worldSunVector;
uniform vec3 worldLightVector;

uniform int moonPhase;

uniform vec2 screenPixelSize;

#include "/lib/Atmosphere/Atmosphere.glsl"

void main() {
	gl_Position = vec4(gl_Vertex.xy * 2.0 - 1.0, 0.0, 1.0);
	screenCoord = gl_MultiTexCoord0.xy;

	directIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 0), 0).rgb;
	skyIlluminance = texelFetch(colortex5, ivec2(skyCaptureRes.x, 1), 0).rgb;

	skySHR = vec4(0.0);
	skySHG = vec4(0.0);
	skySHB = vec4(0.0);

	for (uint i = 0u; i < 5u; ++i) {
		float latitude = float(i) * 0.62831853;
		float cosLatitude = cos(latitude), sinLatitude = sin(latitude);
		for (uint j = 0u; j < 5u; ++j) {
			float longitude = float(j) * 1.25663706;
			vec3 rayDir = vec3(cosLatitude * cos(longitude), sinLatitude, cosLatitude * sin(longitude));

			// vec3 skyCol = SkyShading(rayDir);
			vec3 skyCol = texture(colortex5, ProjectSky(rayDir)).rgb;
			//skyIlluminance += skyCol;

			skySHR += ToSH(skyCol.r, rayDir);
			skySHG += ToSH(skyCol.g, rayDir);
			skySHB += ToSH(skyCol.b, rayDir);
		}
	}

	skySHR /= 25.0;
	skySHG /= 25.0;
	skySHB /= 25.0;

	blocklightColor = Blackbody(float(TORCHLIGHT_COLOR_TEMPERATURE));

	#if defined CLOUDS_WEATHER && defined CLOUDS_SHADOW
		cloudDynamicWeather = texelFetch(colortex5, ivec2(skyCaptureRes.x, 5), 0).xyz;
	#endif
}
