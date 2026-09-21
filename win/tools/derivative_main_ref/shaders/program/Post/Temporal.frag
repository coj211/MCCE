
/*
const bool colortex4MipmapEnabled = true;
*/

layout(location = 0) out vec2 velocityData;
layout(location = 1) out vec4 temporalData;

/* DRAWBUFFERS:25 */

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

flat in float exposure;

#ifdef DOF_ENABLED
    flat in float centerDepthSmooth;
#endif

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

vec3 GetClosestFragment(in ivec2 texel, in float depth) {
    vec3 closestFragment = vec3(texel, depth);

    for (uint i = 0u; i < 8u; ++i) {
        ivec2 sampleTexel = offset3x3N[i] + texel;
        float sampleDepth = texelFetch(depthtex0, sampleTexel, 0).x;
        closestFragment = sampleDepth < closestFragment.z ? vec3(sampleTexel, sampleDepth) : closestFragment;
    }

    closestFragment.xy *= screenPixelSize;
    return closestFragment;
}

#if defined DISTANT_HORIZONS
    vec3 GetClosestFragmentDH(in ivec2 texel, in float depth) {
        vec3 closestFragment = vec3(texel, depth);

        for (uint i = 0u; i < 8u; ++i) {
            ivec2 sampleTexel = offset3x3N[i] + texel;
            float sampleDepth = texelFetch(dhDepthTex0, sampleTexel, 0).x;
            closestFragment = sampleDepth < closestFragment.z ? vec3(sampleTexel, sampleDepth) : closestFragment;
        }

        closestFragment.xy *= screenPixelSize;
        return closestFragment;
    }
#endif

#ifdef TAA_ENABLED
    vec3 reinhard(in vec3 color) {
        return color / (1.0 + GetLuminance(color));
    }
    vec3 invReinhard(in vec3 color) {
        return color / (1.0 - GetLuminance(color));
    }

    vec3 RGBtoYCoCgR(in vec3 rgbColor) {
        vec3 YCoCgRColor;

        YCoCgRColor.y = rgbColor.r - rgbColor.b;
        float temp = rgbColor.b + YCoCgRColor.y * 0.5;
        YCoCgRColor.z = rgbColor.g - temp;
        YCoCgRColor.x = temp + YCoCgRColor.z * 0.5;

        return YCoCgRColor;
    }
    vec3 YCoCgRtoRGB(in vec3 YCoCgRColor) {
        vec3 rgbColor;

        float temp = YCoCgRColor.x - YCoCgRColor.z * 0.5;
        rgbColor.g = YCoCgRColor.z + temp;
        rgbColor.b = temp - YCoCgRColor.y * 0.5;
        rgbColor.r = rgbColor.b + YCoCgRColor.y;

        return rgbColor;
    }

    vec3 clipAABB(in vec3 boxMin, in vec3 boxMax, in vec3 previousSample) {
        vec3 p_clip = 0.5 * (boxMax + boxMin);
        vec3 e_clip = 0.5 * (boxMax - boxMin);

        vec3 v_clip = previousSample - p_clip;
        vec3 v_unit = v_clip / e_clip;
        vec3 a_unit = abs(v_unit);
        float ma_unit = maxOf(a_unit);

        if (ma_unit > 1.0) {
            return v_clip / ma_unit + p_clip;
        }else{
            return previousSample;
        }
    }

	//approximation from SMAA presentation from siggraph 2016
	vec4 textureCatmullRomFast(in sampler2D tex, in vec2 coord, in const float sharpness) {
		//vec2 screenSize = textureSize(sampler, 0);
		//vec2 pixelSize = 1.0 / screenSize;

		vec2 position = screenSize * coord;
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

		vec4 color =  texture(tex, vec2(tc12.x, tc0.y )) * l0
					+ texture(tex, vec2(tc0.x,  tc12.y)) * l1
					+ texture(tex, vec2(tc12.x, tc12.y)) * l2
					+ texture(tex, vec2(tc3.x,  tc12.y)) * l3
					+ texture(tex, vec2(tc12.x, tc3.y )) * l4;

		return color / (l0 + l1 + l2 + l3 + l4);
	}

    vec3 TemporalReprojection(in vec2 coord, in vec2 previousCoord) {
        ivec2 texel = ivec2(coord * screenSize);

        vec3 currentSample = texelFetch(colortex4, texel, 0).rgb;
        if (saturate(previousCoord) != previousCoord) return currentSample;

        #define SampleColor(offset) RGBtoYCoCgR(texelFetch(colortex4, texel + offset, 0).rgb);

        vec3 col0 = RGBtoYCoCgR(currentSample);

        vec3 col1 = SampleColor(ivec2(-1,  1));
        vec3 col2 = SampleColor(ivec2( 0,  1));
        vec3 col3 = SampleColor(ivec2( 1,  1));
        vec3 col4 = SampleColor(ivec2(-1,  0));
        vec3 col5 = SampleColor(ivec2( 1,  0));
        vec3 col6 = SampleColor(ivec2(-1, -1));
        vec3 col7 = SampleColor(ivec2( 0, -1));
        vec3 col8 = SampleColor(ivec2( 1, -1));

        // Variance clip
        vec3 clipAvg = (col0 + col1 + col2 + col3 + col4 + col5 + col6 + col7 + col8) * rcp(9.0);
        vec3 sqrVar = (col0 * col0 + col1 * col1 + col2 * col2 + col3 * col3 + col4 * col4 + col5 * col5 + col6 * col6 + col7 * col7 + col8 * col8) * rcp(9.0);

        vec3 variance = sqrt(abs(sqrVar - clipAvg * clipAvg));
        vec3 clipMin = clipAvg - variance * 1.25;
        vec3 clipMax = clipAvg + variance * 1.25;

        #ifdef TAA_SHARPEN
            //currentSample = textureSmoothFilter(colortex4, coord).rgb;
            vec3 previousSample = textureCatmullRomFast(colortex5, previousCoord, TAA_SHARPNESS).rgb;
        #else
            vec3 previousSample = texture(colortex5, previousCoord).rgb;
        #endif

        previousSample = RGBtoYCoCgR(previousSample);
        previousSample = clipAABB(clipMin, clipMax, previousSample);

        previousSample = YCoCgRtoRGB(previousSample);

        float blendWeight = 0.97;
        vec2 pixelVelocity = 1.0 - abs(fract(previousCoord * screenSize) * 2.0 - 1.0);
        blendWeight *= sqrt(pixelVelocity.x * pixelVelocity.y) * 0.25 + 0.75;

        return invReinhard(mix(reinhard(currentSample), reinhard(previousSample), blendWeight));
    }
#endif

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 screenCoord = gl_FragCoord.xy * screenPixelSize;

    float depth = texelFetch(depthtex0, texel, 0).x;

    #if defined DISTANT_HORIZONS
        if (depth < 1.0) {
            vec3 closestFragment = GetClosestFragment(texel, depth);
            velocityData = closestFragment.xy - Reproject(closestFragment).xy;
        } else {
            vec3 closestFragment = GetClosestFragmentDH(texel, GetDepthDH(texel));
            velocityData = closestFragment.xy - ReprojectDH(closestFragment).xy;
        }
    #else
        vec3 closestFragment = GetClosestFragment(texel, depth);
        velocityData = closestFragment.xy - Reproject(closestFragment).xy;
    #endif

    vec2 previousCoord = screenCoord - velocityData;

    #ifdef TAA_ENABLED
        temporalData.rgb = clamp16F(TemporalReprojection(screenCoord + taaOffset * 0.5, previousCoord));
    #else
        temporalData.rgb = texelFetch(colortex4, texel, 0).rgb;
    #endif

    temporalData.a = 0.0;
    if (texel == ivec2(0)) temporalData.a = exposure;
    #ifdef DOF_ENABLED
        if (texel == ivec2(1)) temporalData.a = centerDepthSmooth;
    #endif
}
