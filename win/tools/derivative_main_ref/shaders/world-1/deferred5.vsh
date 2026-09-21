#version 450 compatibility

out vec2 screenCoord;

flat out vec3 ambientColor;
flat out vec3 blocklightColor;

#include "/lib/Head/Common.inc"

uniform float nightVision;

uniform float BiomeNetherWastesSmooth;
uniform float BiomeWarpedForestSmooth;
uniform float BiomeCrimsonForestSmooth;
uniform float BiomeSoulSandValleySmooth;
uniform float BiomeBasaltDeltasSmooth;

vec3 NetherLightingColor() {
	vec3 color = BiomeNetherWastesSmooth   * vec3(0.99, 0.34, 0.1) * 0.9;
	color +=	 BiomeWarpedForestSmooth   * vec3(0.79, 0.82, 1.0) * 0.5;
	color +=	 BiomeCrimsonForestSmooth  * vec3(1.0, 0.80, 0.57);
	color +=	 BiomeSoulSandValleySmooth * vec3(0.6, 0.77, 1.0)  * 0.35;
	color +=	 BiomeBasaltDeltasSmooth   * vec3(1.0, 0.78, 0.62) * 2.0;
	return color;
}

void main() {
	gl_Position = vec4(gl_Vertex.xy * 2.0 - 1.0, 0.0, 1.0);
	screenCoord = gl_MultiTexCoord0.xy;

	ambientColor = NetherLightingColor() * (BASIC_BRIGHTNESS_NETHER + nightVision * 0.2);
	blocklightColor = Blackbody(float(TORCHLIGHT_COLOR_TEMPERATURE));
}
