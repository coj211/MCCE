
out vec4 indirectData;

/* DRAWBUFFERS:0 */

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);

	indirectData = texelFetch(colortex0, texel, 0);
	if (all(lessThan(texel, ceil(screenSize * 0.5)))) {
        float depth = texelFetch(depthtex0, texel * 2, 0).x;
		if (depth < 1.0) {
			ivec2 shift = ivec2(viewWidth * 0.5, 0);
			vec4 normalDepthData = texelFetch(colortex0, texel + shift, 0);
			vec3 viewPos = ScreenToViewSpace(vec3(gl_FragCoord.xy * screenPixelSize * 2.0, depth));
			float NdotV = saturate(dot(normalDepthData.xyz, -normalize(viewPos)));

			float sumWeight = 1.0;

			for (uint i = 0u; i < 8u; ++i) {
				ivec2 offset = offset3x3N[i];
				ivec2 sampleTexel = texel + offset;
				if (clamp(sampleTexel, ivec2(0), ivec2(screenSize * 0.5) - 1) != sampleTexel) continue;
				//sampleTexel = clamp(sampleTexel, ivec2(1), ivec2(screenSize * 0.5) - 1);

				//vec3 sampleNormal = GetNormals(sampleTexel * 2);
				//float sampleDist = ScreenToViewSpace(GetDepthFix(sampleTexel * 2));
				vec4 prevData = texelFetch(colortex0, sampleTexel + shift, 0);

				float weight = exp2(-dotSelf(offset) * 0.05);
				weight *= exp2(-abs(prevData.w - normalDepthData.w) * 4.0 * NdotV); // Distance weight
				weight *= pow16(max0(dot(prevData.xyz, normalDepthData.xyz))); // Normal weight

				vec4 sampleLight = texelFetch(colortex0, sampleTexel, 0);

				indirectData += sampleLight * weight;
				sumWeight += weight;
			}

			indirectData /= sumWeight;
		}
	}
}
