
out vec3 finalData;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

#include "/lib/Head/Noise.inc"

#define INFO 0		// [0 1 2 3]
#define Version 0	// [0 1 2 3]

//#define DEBUG_DRAWBUFFERS

//----------------------------------------------------------------------------//

#define minOf(a, b, c, d, e, f, g, h, i) min(a, min(b, min(c, min(d, min(e, min(f, min(g, min(h, i))))))))
#define maxOf(a, b, c, d, e, f, g, h, i) max(a, max(b, max(c, max(d, max(e, max(f, max(g, max(h, i))))))))

#define SampleColor(texel) texelFetch(colortex3, texel, 0).rgb

// Contrast Adaptive Sharpening (CAS)
// Reference: Lou Kramer, FidelityFX CAS, AMD Developer Day 2019,
// https://gpuopen.com/wp-content/uploads/2019/07/FidelityFX-CAS.pptx
vec3 CASFilter(in ivec2 texel) {
	#ifndef CAS_ENABLED
		return SampleColor(texel);
	#endif

	vec3 a = SampleColor(texel + ivec2(-1, -1));
	vec3 b = SampleColor(texel + ivec2( 0, -1));
	vec3 c = SampleColor(texel + ivec2( 1, -1));
	vec3 d = SampleColor(texel + ivec2(-1,  0));
	vec3 e = SampleColor(texel);
	vec3 f = SampleColor(texel + ivec2( 1,  0));
	vec3 g = SampleColor(texel + ivec2(-1,  1));
	vec3 h = SampleColor(texel + ivec2( 0,  1));
	vec3 i = SampleColor(texel + ivec2( 1,  1));

	vec3 minColor = minOf(a, b, c, d, e, f, g, h, i);
	vec3 maxColor = maxOf(a, b, c, d, e, f, g, h, i);

    vec3 sharpeningAmount = sqrt(min(1.0 - maxColor, minColor) / maxColor);
    vec3 w = sharpeningAmount * mix(-0.125, -0.2, CAS_STRENGTH);

	//float minG = minOf(a.g, b.g, c.g, d.g, e.g, f.g, g.g, h.g, i.g);
	//float maxG = maxOf(a.g, b.g, c.g, d.g, e.g, f.g, g.g, h.g, i.g);

    //float sharpeningAmount = sqrt(min(1.0 - maxG, minG) / maxG);
    //float w = sharpeningAmount * mix(-0.125, -0.2, CAS_STRENGTH);

	return ((b + d + f + h) * w + e) / (4.0 * w + 1.0);
}

//----------------------------------------------------------------------------//

//approximation from SMAA presentation from siggraph 2016
vec3 textureCatmullRomFast(in sampler2D tex, in vec2 position, in const float sharpness) {
	//vec2 screenSize = textureSize(sampler, 0);
	//vec2 screenPixelSize = 1.0 / screenSize;

	//vec2 position = screenSize * coord;
	vec2 centerPosition = floor(position - 0.5) + 0.5;
	vec2 f = position - centerPosition;
	vec2 f2 = f * f;
	vec2 f3 = f * f2;

	vec2 w0 = -sharpness        * f3 + 2.0 * sharpness         * f2 - sharpness * f;
	vec2 w1 = (2.0 - sharpness) * f3 - (3.0 - sharpness)       * f2 + 1.0;
	vec2 w2 = (sharpness - 2.0) * f3 + (3.0 - 2.0 * sharpness) * f2 + sharpness * f;
	vec2 w3 = sharpness         * f3 - sharpness               * f2;

	vec2 w12 = w1 + w2;

	vec2 tc0 = screenPixelSize * (centerPosition - 1.0);
	vec2 tc3 = screenPixelSize * (centerPosition + 2.0);
	vec2 tc12 = screenPixelSize * (centerPosition + w2 / w12);

	float l0 = w12.x * w0.y;
	float l1 = w0.x  * w12.y;
	float l2 = w12.x * w12.y;
	float l3 = w3.x  * w12.y;
	float l4 = w12.x * w3.y;

	vec3 color =  texture(tex, vec2(tc12.x, tc0.y )).rgb * l0
				+ texture(tex, vec2(tc0.x,  tc12.y)).rgb * l1
				+ texture(tex, vec2(tc12.x, tc12.y)).rgb * l2
				+ texture(tex, vec2(tc3.x,  tc12.y)).rgb * l3
				+ texture(tex, vec2(tc12.x, tc3.y )).rgb * l4;

	return color / (l0 + l1 + l2 + l3 + l4);
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
    ivec2 texel = ivec2(gl_FragCoord.xy);

	#ifdef DEBUG_DRAWBUFFERS
		finalData = texelFetch(colortex4, texel, 0).rgb;
		return;
	#endif

	if (abs(MC_RENDER_QUALITY - 1.0) < 1e-2) {
    	finalData = CASFilter(texel);
	}else{
		finalData = textureCatmullRomFast(colortex3, texel * MC_RENDER_QUALITY, 0.6);
	}
	finalData += (bayer16(gl_FragCoord.xy) - 0.5) * rcp(255.0);
}