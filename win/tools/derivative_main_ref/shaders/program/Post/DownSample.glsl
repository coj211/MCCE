
/*
const bool colortex5MipmapEnabled = true;
*/

out vec3 bloomTiles;

/* DRAWBUFFERS:4 */

uniform sampler2D colortex5;

uniform vec2 screenPixelSize;

#include "/Settings.glsl"

//----------------------------------------------------------------------------//

vec2 CalculateTileOffset(int lod) {
	// https://github.com/zombye/spectrum
	vec2 lodMult = floor(lod * 0.5 + vec2(0.0, 0.5));
	vec2 offset = vec2(1.0 / 3.0, 2.0 / 3.0) * (1.0 - exp2(-2.0 * lodMult));

	return lodMult * 16.0 * screenPixelSize + offset;
}

vec3 DualBlurDownSample(in int lod) {
    float scale = exp2(lod);
    vec2 texelOffset = screenPixelSize * scale;

	vec2 coord = gl_FragCoord.xy * screenPixelSize - CalculateTileOffset(lod - 1);
	coord *= scale;

	if (any(greaterThanEqual(abs(coord - 0.5), texelOffset + 0.5))) return vec3(0.0);

	//vec3 bloomTile = textureLod(colortex4, coord, lod).rgb;
	//bloomTile += textureLod(colortex4, vec2( 1.0,  1.0) * texelOffset + coord, lod).rgb;
	//bloomTile += textureLod(colortex4, vec2(-1.0,  1.0) * texelOffset + coord, lod).rgb;
	//bloomTile += textureLod(colortex4, vec2( 1.0, -1.0) * texelOffset + coord, lod).rgb;
	//bloomTile += textureLod(colortex4, vec2(-1.0, -1.0) * texelOffset + coord, lod).rgb;

	//return bloomTile * 0.2;

	vec3  bloomTile = vec3(0.0);
	float sumWeight = 0.0;

	for (int y = -BLUR_SAMPLES; y <= BLUR_SAMPLES; ++y) {
		for (int x = -BLUR_SAMPLES; x <= BLUR_SAMPLES; ++x) {
			float weight = clamp(1.0 - length(vec2(x, y)) * 0.25, 0.0, 1.0);
			      weight *= weight;

			bloomTile += textureLod(colortex5, coord + vec2(x, y) * texelOffset, lod).rgb * weight;
			sumWeight += weight;
		}
	}

	return bloomTile / sumWeight;
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	bloomTiles = vec3(0.0);
	bloomTiles += DualBlurDownSample(1);
	bloomTiles += DualBlurDownSample(2);
	bloomTiles += DualBlurDownSample(3);
	bloomTiles += DualBlurDownSample(4);
	bloomTiles += DualBlurDownSample(5);
	bloomTiles += DualBlurDownSample(6);
	bloomTiles += DualBlurDownSample(7);

	bloomTiles = clamp(bloomTiles, 0.0, 65535.0);
}
