
#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

//--// Internal Settings //---------------------------------------------------//

/*
	const int 	colortex0Format 			= RGBA16F;
	const int 	colortex1Format 			= RGBA16F;
	const int 	colortex2Format 			= RGBA16F;
	const int 	colortex3Format 			= RGBA16;
	const int 	colortex4Format 			= R11F_G11F_B10F;
	const int 	colortex5Format 			= RGBA16F;
	const int 	colortex6Format 			= RGB8;
	const int 	colortex7Format 			= RGB8;

	const bool	colortex0Clear				= false;
	const bool	colortex1Clear				= false;
	const bool	colortex2Clear				= false;
	const bool	colortex4Clear				= false;
	const bool  colortex5Clear				= false;
	const bool 	colortex7Clear				= true;


	const float shadowIntervalSize 			= 2.0;
	const float ambientOcclusionLevel 		= 0.05f;
	const float	sunPathRotation				= -35.0; // [-90.0 -89.0 -88.0 -87.0 -86.0 -85.0 -84.0 -83.0 -82.0 -81.0 -80.0 -79.0 -78.0 -77.0 -76.0 -75.0 -74.0 -73.0 -72.0 -71.0 -70.0 -69.0 -68.0 -67.0 -66.0 -65.0 -64.0 -63.0 -62.0 -61.0 -60.0 -59.0 -58.0 -57.0 -56.0 -55.0 -54.0 -53.0 -52.0 -51.0 -50.0 -49.0 -48.0 -47.0 -46.0 -45.0 -44.0 -43.0 -42.0 -41.0 -40.0 -39.0 -38.0 -37.0 -36.0 -35.0 -34.0 -33.0 -32.0 -31.0 -30.0 -29.0 -28.0 -27.0 -26.0 -25.0 -24.0 -23.0 -22.0 -21.0 -20.0 -19.0 -18.0 -17.0 -16.0 -15.0 -14.0 -13.0 -12.0 -11.0 -10.0 -9.0 -8.0 -7.0 -6.0 -5.0 -4.0 -3.0 -2.0 -1.0 0.0 1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0 11.0 12.0 13.0 14.0 15.0 16.0 17.0 18.0 19.0 20.0 21.0 22.0 23.0 24.0 25.0 26.0 27.0 28.0 29.0 30.0 31.0 32.0 33.0 34.0 35.0 36.0 37.0 38.0 39.0 40.0 41.0 42.0 43.0 44.0 45.0 46.0 47.0 48.0 49.0 50.0 51.0 52.0 53.0 54.0 55.0 56.0 57.0 58.0 59.0 60.0 61.0 62.0 63.0 64.0 65.0 66.0 67.0 68.0 69.0 70.0 71.0 72.0 73.0 74.0 75.0 76.0 77.0 78.0 79.0 80.0 81.0 82.0 83.0 84.0 85.0 86.0 87.0 88.0 89.0 90.0]
	const float eyeBrightnessHalflife 		= 10.0;

	const float wetnessHalflife				= 180.0;
	const float drynessHalflife				= 60.0;

	const bool 	shadowHardwareFiltering1 	= true;
*/

#ifdef PARALLAX
/*
	const int 	colortex7Format 			= RGBA8;
*/
#endif

//----------------------------------------------------------------------------//

//#define PURKINJE_SHIFT

#if !defined IS_OVERWORLD
	#undef PURKINJE_SHIFT
#endif

#define TONEMAP AcademyFit // [AcademyFit AcademyFull AgX_Minimal AgX_Full]

//#define CINEMATIC_EFFECT

//#define COLOR_GRADING
#define BRIGHTNESS 		1.0  // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0]
#define GAMMA 			1.0  // [0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0]
#define CONTRAST		1.0  // [0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0]
#define SATURATION 		1.0  // [0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0]
#define WHITE_BALANCE	6500 // [2500 3000 3500 4000 4100 4200 4300 4400 4500 4600 4700 4800 4900 5000 5100 5200 5300 5400 5500 5600 5700 5800 5900 6000 6100 6200 6300 6400 6500 6600 6700 6800 6900 7000 7100 7200 7300 7400 7500 7600 7700 7800 7900 8000 8100 8200 8300 8400 8500 8600 8700 8800 8900 9000 9100 9200 9300 9400 9500 9600 9700 9800 9900 10000 10100 10200 10300 10400 10500 10600 10700 10800 10900 11000 11100 11200 11300 11400 11500 11600 11700 11800 11900 12000]

//#define VIGNETTE_ENABLED
#define VIGNETTE_STRENGTH 1.0 // [0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7 1.8 1.9 2.0 2.5 3.0 3.5 4.0 5.0]

//#define DEBUG_COUNTER

//----------------------------------------------------------------------------//


out vec3 sceneColor;

/* DRAWBUFFERS:3 */

in vec2 screenCoord;
//flat in float exposure;

//----// FUNCTIONS //-----------------------------------------------------------------------------//

float ScreenToViewSpace(in float depth) {
    depth = depth * 2.0 - 1.0;
    return 1.0 / (depth * gbufferProjectionInverse[2][3] + gbufferProjectionInverse[3][3]);
}

vec2 CalculateTileOffset(int lod) {
	// https://github.com/zombye/spectrum
	vec2 lodMult = floor(lod * 0.5 + vec2(0.0, 0.5));
	vec2 offset = vec2(1.0 / 3.0, 2.0 / 3.0) * (1.0 - exp2(-2.0 * lodMult));

	return lodMult * 16.0 * screenPixelSize + offset;
}

vec3 DualBlurUpSample(in sampler2D tex, in int lod) {
    float scale = exp2(-lod);
    vec2 coord = screenCoord * scale + CalculateTileOffset(lod - 1);

    return textureBicubic(tex, coord).rgb;
}

void CalculateBloomFog(inout vec3 color, in ivec2 texel) {
	vec3 sampleTile = vec3(0.0);
	vec3 bloomData = vec3(0.0);
	vec3 fogBloom = vec3(0.0);

	sampleTile = DualBlurUpSample(colortex4, 1);
	bloomData += sampleTile;
	fogBloom += sampleTile;

	sampleTile = DualBlurUpSample(colortex4, 2);
	bloomData += sampleTile * 0.83333333;
	fogBloom += sampleTile * 1.5;

	sampleTile = DualBlurUpSample(colortex4, 3);
	bloomData += sampleTile * 0.69444444;
	fogBloom += sampleTile * 2.25;

	sampleTile = DualBlurUpSample(colortex4, 4);
	bloomData += sampleTile * 0.57870370;
	fogBloom += sampleTile * 3.375;

	sampleTile = DualBlurUpSample(colortex4, 5);
	bloomData += sampleTile * 0.48225309;
	fogBloom += sampleTile * 5.0625;

	sampleTile = DualBlurUpSample(colortex4, 6);
	bloomData += sampleTile * 0.40187757;
	fogBloom += sampleTile * 7.59375;

	sampleTile = DualBlurUpSample(colortex4, 7);
	bloomData += sampleTile * 0.33489798;
	fogBloom += sampleTile * 11.328125;

	bloomData *= 0.23118661;
	fogBloom *= 0.03108305;

	fogBloom += bloomData;

	#ifdef BLOOMY_FOG
		float fogTransmittance = texelFetch(colortex6, texel, 0).x;
		color = mix(fogBloom * 0.5, color, fogTransmittance);
	#endif

	float bloomAmount = BLOOM_AMOUNT * 0.15;

	float exposure = texelFetch(colortex5, ivec2(0), 0).a;
	bloomAmount /= fma(max(exposure, 1.0), 0.7, 0.3);

	color += bloomData * bloomAmount;
	#if !defined IS_NETHER
		if (isEyeInWater == 0 && wetness > 1e-2) {
			float rain = texelFetch(colortex0, texel, 0).b * 0.35;
			fogBloom *= 1.0 + weatherSnowySmooth * 2.0;
			color = color * oneMinus(rain) + fogBloom * fma(clamp(exposure, 0.6, 2.0), 0.15, 0.3) * rain;
		}
	#endif
}

const mat3 rgbToXyz = mat3(
	0.4124564, 0.3575761, 0.1804375,
	0.2126729, 0.7151522, 0.0721750,
	0.0193339, 0.1191920, 0.9503041
);

const mat3 xyzToRgb = mat3(
     3.2409699419, -1.5373831776, -0.4986107603,
    -0.9692436363,  1.8759675015,  0.0415550574,
     0.0556300797, -0.2039769589,  1.0569715142
);

#ifdef PURKINJE_SHIFT
	vec3 PurkinjeShift(in vec3 color) {
		const vec3 rodResponse = vec3(7.15e-5, 4.81e-1, 3.28e-1);
		vec3 xyz = color * rgbToXyz;

		vec3 scotopicLuminance = max0(xyz * (1.33 * (1.0 + (xyz.y + xyz.z) / xyz.x) - 1.68));

		float purkinje = dot(rodResponse, scotopicLuminance * xyzToRgb);
		return mix(color, purkinje * vec3(0.5, 0.7, 1.0), fastExp(-purkinje * 90.0));
	}
#endif

#if defined COLOR_GRADING && WHITE_BALANCE != 6500
	mat3 ChromaticAdaptationMatrix(vec3 srcXyz, vec3 dstXyz) {
		const mat3 bradfordConeResponse = mat3(
			0.89510, -0.75020,  0.03890,
			0.26640,  1.71350, -0.06850,
			-0.16140,  0.03670,  1.02960
		);

		vec3 srcLms = srcXyz * bradfordConeResponse;
		vec3 dstLms = dstXyz * bradfordConeResponse;
		vec3 quotient = dstLms / srcLms;

		mat3 vonKries = mat3(
			quotient.x, 0.0, 0.0,
			0.0, quotient.y, 0.0,
			0.0, 0.0, quotient.z
		);

		return (bradfordConeResponse * vonKries) * inverse(bradfordConeResponse);
	}

	mat3 WhiteBalanceMatrix() {
		vec3 srcXyz = Blackbody(float(WHITE_BALANCE)) * rgbToXyz;
		vec3 dstXyz = Blackbody(6500.0) 			  * rgbToXyz;

		return rgbToXyz * ChromaticAdaptationMatrix(srcXyz, dstXyz) * xyzToRgb;
	}
#endif

vec3 Contrast(in vec3 color) {
	const float logMidpoint = log2(0.16);
	color = log2(color + 1e-6) - logMidpoint;
	return max0(exp2(color * CONTRAST + logMidpoint) - 1e-6);
}

#include "/lib/Post/ACES.glsl"
#include "/lib/Post/AgX.glsl"

#ifdef DEBUG_COUNTER
	#include "/lib/Post/PrintFloat.glsl"
#endif

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);
	#ifdef MOTION_BLUR
		vec3 color = texelFetch(colortex2, texel, 0).rgb;
	#else
		vec3 color = texelFetch(colortex5, texel, 0).rgb;
	#endif

	#ifdef BLOOM_ENABLED
		CalculateBloomFog(color, texel);
	#endif

	#ifdef PURKINJE_SHIFT
		color = PurkinjeShift(color);
	#endif

	color *= texelFetch(colortex5, ivec2(0), 0).a; // Exposure

	//color *= pow(vec3(1.0, 1.07, 1.25), vec3(1.1));
	// color *= vec3(1.0, 1.07, 1.25);

	#ifdef VIGNETTE_ENABLED
		color *= fastExp(-2.0 * dotSelf(screenCoord - 0.5) * VIGNETTE_STRENGTH);
	#endif

	color = TONEMAP(color);
	// color = mix(AcademyFit(color), AcademyFull(color), screenCoord.x > 0.5);
	// color = mix(AgX_Minimal(color), AgX_Full(color), screenCoord.x > 0.5);
	// color = mix(AcademyFull(color), AgX_Full(color), screenCoord.x > 0.5);

	#ifdef CINEMATIC_EFFECT
		color *= step(abs(screenCoord.y - 0.5) * 2.0, aspectRatio * (9.0 / 21.0)); // 21:9
	#endif

	#ifdef DEBUG_COUNTER
		const float scale = 5.0, size = 1.0 / scale;

		vec2 tCoord = gl_FragCoord.xy * size;

		if (clamp(tCoord, vec2(0.0, 25.0), vec2(40.0, 50.0)) == tCoord) {
			color = min(color * 0.5, 0.8);
		}

		color += PrintFloat(0.0, vec2(10.0, 35.0) * scale, size);
	#endif

	sceneColor = saturate(color);
}
