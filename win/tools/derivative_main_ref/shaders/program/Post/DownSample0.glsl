
out vec3 bloomTiles;

/* DRAWBUFFERS:4 */

uniform sampler2D colortex5;

uniform vec2 screenPixelSize;

in vec2 screenCoord;

//----// FUNCTIONS //-----------------------------------------------------------------------------//

vec3 DualBlurDownSample() {
	vec3 bloomTile = textureLod(colortex5, screenCoord, 0).rgb;
	bloomTile += textureLod(colortex5, vec2( 1.0,  1.0) * screenPixelSize + screenCoord, 0).rgb;
	bloomTile += textureLod(colortex5, vec2(-1.0,  1.0) * screenPixelSize + screenCoord, 0).rgb;
	bloomTile += textureLod(colortex5, vec2( 1.0, -1.0) * screenPixelSize + screenCoord, 0).rgb;
	bloomTile += textureLod(colortex5, vec2(-1.0, -1.0) * screenPixelSize + screenCoord, 0).rgb;

	return bloomTile * 0.2;

	//vec3  bloomTile = vec3(0.0);
	//float sumWeight = 0.0;

	//for (int y = -1; y <= 1; ++y) {
	//	for (int x = -1; x <= 1; ++x) {
	//		float weight = clamp(1.0 - length(vec2(x, y)) * 0.25, 0.0, 1.0);
	//		      weight *= weight;

	//		bloomTile += textureLod(colortex5, screenCoord + vec2(x, y) * screenPixelSize, 0).rgb * weight;
	//		sumWeight += weight;
	//	}
	//}

	//return bloomTile / sumWeight;
}

/////////////////////////MAIN///////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////MAIN///////////////////////////////////////////////////////////////////////////////////////////
void main() {
	bloomTiles = clamp(DualBlurDownSample(), 0.0, 65535.0);
}
