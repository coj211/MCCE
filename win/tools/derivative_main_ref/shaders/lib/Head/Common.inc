/*
--------------------------------------------------------------------------------
		   ____________
		  /________   /
				  /  /
		_________/  /__________ _________
	   /  ______/  /__________//  ______/
	  /  /	   /  //  _______//  /
	 /  /_____/  //  /_______/  /
	/___________//_____________/

	Derivative Shaders by HaringPro

	Copyright (C) 2024 HaringPro. All Rights Reserved

--------------------------------------------------------------------------------
*/


#include "/Settings.glsl"

const float PI 			= radians(180.0);
const float rPI 		= 1.0 / PI;
const float TAU 		= radians(360.0);
const float rTAU 		= 1.0 / TAU;
const float rLOG2 		= 1.0 / log(2.0);

#define rcp(x) 			(1.0 / (x))
#define oneMinus(x) 	(1.0 - (x))
#define fastExp(x) 		exp2((x) * rLOG2)
#define max0(x) 		max(x, 0.0)
#define saturate(x) 	clamp(x, 0.0, 1.0)
#define clamp16F(x) 	clamp(x, 0.0, 65535.0)

#define transMAD(m, v)	(mat3(m) * (v) + (m)[3].xyz)
#define diagonal2(m)	vec2((m)[0].x, (m)[1].y)
#define diagonal3(m)	vec3((m)[0].x, (m)[1].y, m[2].z)
#define diagonal4(m)	vec4(diagonal3(m), (m)[2].w)
#define projMAD(m, v)	(diagonal3(m) * (v) + (m)[3].xyz)

#define RawCoord(coord) ivec2((coord) * screenSize)

float maxOf(vec2 v)   	{ return max(v.x, v.y); }
float maxOf(vec3 v)   	{ return max(v.x, max(v.y, v.z)); }
float maxOf(vec4 v)   	{ return max(v.x, max(v.y, max(v.z, v.w))); }
float minOf(vec2 v)   	{ return min(v.x, v.y); }
float minOf(vec3 v)   	{ return min(v.x, min(v.y, v.z)); }
float minOf(vec4 v)   	{ return min(v.x, min(v.y, min(v.z, v.w))); }

float sqr(float x)    	{ return x * x; }
vec2  sqr(vec2 x)	  	{ return x * x; }
vec3  sqr(vec3 x)	  	{ return x * x; }
vec4  sqr(vec4 x)	  	{ return x * x; }

float cube(float x)   	{ return x * x * x; }
vec2  cube(vec2 x)	  	{ return x * x * x; }
vec3  cube(vec3 x)	  	{ return x * x * x; }

float pow4(float x)   	{ return cube(x) * x; }
vec3  pow4(vec3 x)	  	{ return cube(x) * x; }

float pow5(float x)   	{ return pow4(x) * x; }
vec3  pow5(vec3 x)	  	{ return pow4(x) * x; }

float pow16(float x)   	{ return sqr(pow4(x)); }

float sqrt2(float c)  	{ return sqrt(sqrt(c)); }
vec3  sqrt2(vec3 c)	  	{ return sqrt(sqrt(c)); }

float curve(float x)  	{ return sqr(x) * (3.0 - 2.0 * x); }
vec2  curve(vec2 x)	  	{ return sqr(x) * (3.0 - 2.0 * x); }
vec3  curve(vec3 x)	  	{ return sqr(x) * (3.0 - 2.0 * x); }

float dotSelf(vec2 x) 	{ return dot(x, x); }
float dotSelf(vec3 x) 	{ return dot(x, x); }

vec2  sincos(float x)   { return vec2(sin(x), cos(x)); }
vec2  cossin(float x)   { return vec2(cos(x), sin(x)); }

float remap(float e0, float e1, float x) { return saturate((x - e0) / (e1 - e0)); }

#if defined MC_NORMAL_MAP
	void DecodeNormalTex(inout vec3 normalTex) {
        if (all(lessThan(normalTex, vec3(0.003)))) return;
		normalTex = normalTex * 2.0 - (254.0 / 255.0);
		#if TEXTURE_FORMAT == 0
			normalTex.z = sqrt(saturate(oneMinus(dotSelf(normalTex.xy))));
		#else
			normalTex = normalize(normalTex);
		#endif
		//normalTex.xy = max0(abs(normalTex.xy) - 1.0 * rcp(255.0)) * sign(normalTex.xy);
	}
#endif

vec2 EncodeNormal(in vec3 n) {
    n.xy /= abs(n.x) + abs(n.y) + abs(n.z);
    if (n.z <= 0.0) {
        n.xy = (vec2(1.0) - abs(n.yx)) * (step(0.0, n.xy) * 2.0 - 1.0);
    }
    return n.xy * 0.5 + 0.5;
}

vec3 DecodeNormal(in vec2 en) {
    en = en * 2.0 - 1.0;
	vec3 normal = vec3(en, oneMinus(abs(en.x) + abs(en.y)));
    if (normal.z <= 0.0) {
        normal.xy = (vec2(1.0) - abs(en.yx)) * (step(0.0, en) * 2.0 - 1.0);
    }
	return normalize(normal);
}

// https://github.com/Jessie-LC/open-source-utility-code/blob/main/advanced/packing.glsl

float PackUnorm2x8(vec2 xy) {
	return dot(floor(255.0 * xy + 0.5), vec2(1.0 / 65535.0, 256.0 / 65535.0));
}
//float PackUnorm2x8(float x, float y) { return PackUnorm2x8(vec2(x, y)); }
vec2 UnpackUnorm2x8(float pack) {
	vec2 xy; xy.x = modf(pack * 65535.0 / 256.0, xy.y);
	return xy * vec2(256.0 / 255.0, 1.0 / 255.0);
}

vec3 LinearToSRGB(in vec3 color) {
	return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, lessThan(vec3(0.0031308), color));
}

vec3 SRGBtoLinear(in vec3 color) {
	return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), lessThan(vec3(0.04045), color));
}

float GetLuminance(in vec3 color) {
	//return dot(color, vec3(0.2722287168, 0.6740817658, 0.0536895174));
	return dot(color, vec3(0.2722, 0.6741, 0.0537));
}

vec3 ColorSaturation(in vec3 color, in const float sat) { return mix(vec3(GetLuminance(color)), color, sat); }
/*
vec3 Rand(in vec2 coord) {
    return fract(sin(dot(coord, vec2(12.9898, 78.233))) * vec3(43758.5453, 28001.8384, 50849.4141));
}
*/
vec3 Blackbody(in float t) {
    // http://en.wikipedia.org/wiki/Planckian_locus

    vec4 vx = vec4(-0.2661239e9, -0.2343580e6, 0.8776956e3, 0.179910  );
    vec4 vy = vec4(-1.1063814,   -1.34811020,  2.18555832, -0.20219683);
    float it = 1.0 / t;
    float it2 = it * it;
    float x = dot(vx, vec4(it * it2, it2, it, 1.0));
    float x2 = x * x;
    float y = dot(vy, vec4(x * x2, x2, x, 1.0));

    // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    mat3 xyzToSrgb = mat3(
         3.2404542,-1.5371385,-0.4985314,
        -0.9692660, 1.8760108, 0.0415560,
         0.0556434,-0.2040259, 1.0572252
    );

    vec3 srgb = vec3(x / y, 1.0, oneMinus(x + y) / y) * xyzToSrgb;

    return max0(srgb);
}

/*
// https://en.wikipedia.org/wiki/Planck%27s_law
// wavelength l in nm, temperature t in Kelvin, returns radiance
vec3 Blackbody(float t) {
    //const float h = 6.626070040e-16;
    //const float k = 1.38064852e-5;
    //const float c = 299792458.0e9;

	const float h = 6.63e-16; // Planck constant
	const float k = 1.38e-5;  // Boltzmann constant
	const float c = 3.0e17;   // Speed of light

    const float o0 = 2e-3 * h * (c * c);
    const float o1 = h * c / k * 1.442695;

	const vec3 l = vec3(630.0, 530.0, 465.0);

    vec3 rgb = o0 / (pow5(l) * (fastExp(o1 / (l * t)) - 1.0));
	return rgb / minOf(rgb);
    //return 2.0 * h * (c * c) / (pow5(l) * (fastExp(h * c / (t * l *k)) - 1.0));
}

vec3 Blackbody(float t) {
    const float h = 6.6e-34; 	// Planck constant
    const float k = 1.4e-23; 	// Boltzmann constant
    const float c = 3e8;		// Speed of light

    const vec3 w = vec3(610.0, 549.0, 468.0) * 1e-9; // sRGB approximate wavelength of primaries

    // This would be more accurate if we integrate over a range of wavelengths
    // rather than a single wavelength for r, g, b

    // Planck's law https://en.wikipedia.org/wiki/Planck%27s_law

    vec3 o = 2.0 * h * (c * c) / (pow5(w) * (fastExp(h * c / (w * k * t)) - 1.0));
	return o / minOf(o);
}

vec4 textureSmoothFilter(in sampler2D tex, in vec2 coord) {
	// from https://www.coder.work/article/6260650
	vec2 res = vec2(textureSize(tex, 0));

	coord = coord * res + 0.5;

	vec2 pm = floor(coord);
	vec2 pf = curve(coord - pm);

	coord = (pm + pf - 0.5) / res;
	return texture(tex, coord);
}
*/
vec4 textureSmoothFilter(in sampler2D tex, in vec2 coord) {
	// from https://iquilezles.org/www/articles/texture/texture.htm
	vec2 res = vec2(textureSize(tex, 0));

	coord = coord * res + 0.5;

	vec2 i, f = modf(coord, i);
	f *= f * f * (f * (f * 6.0 - 15.0) + 10.0);
	coord = i + f;

	coord = (coord - 0.5) / res;
	return texture(tex, coord);
}

vec4 cubic(in float x) {
    float x2 = x * x;
    float x3 = x2 * x;
    vec4 w;
    w.x = -x3 + 3.0 * x2 - 3.0 * x + 1.0;
    w.y = 3.0 * x3 - 6.0 * x2 + 4.0;
    w.z = -3.0 * x3 + 3.0 * x2 + 3.0 * x + 1.0;
    w.w = x3;
    return w * rcp(6.0);
}

vec4 textureBicubic(in sampler2D tex, in vec2 coord) {
	vec2 res = textureSize(tex, 0);

	coord = coord * res - 0.5;

	vec2 fTexel = fract(coord);
	coord -= fTexel;

    vec4 xCubic = cubic(fTexel.x);
    vec4 yCubic = cubic(fTexel.y);

	vec4 c = coord.xxyy + vec2(-0.5, 1.5).xyxy;

	vec4 s = vec4(xCubic.xz + xCubic.yw, yCubic.xz + yCubic.yw);

    vec4 offset = c + vec4(xCubic.y, xCubic.w, yCubic.y, yCubic.w) / s;
	offset *= 1.0 / res.xxyy;

	vec4 sample0 = texture(tex, offset.xz);
	vec4 sample1 = texture(tex, offset.yz);
	vec4 sample2 = texture(tex, offset.xw);
	vec4 sample3 = texture(tex, offset.yw);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}
/*
vec4 textureBicubicLod(in sampler2D tex, in vec2 coord, in int lod) {
	vec2 res = textureSize(tex, lod);

	coord = coord * res - 0.5;

	vec2 fTexel = fract(coord);
	coord -= fTexel;

    vec4 xCubic = cubic(fTexel.x);
    vec4 yCubic = cubic(fTexel.y);

	vec4 c = coord.xxyy + vec2(-0.5, 1.5).xyxy;

	vec4 s = vec4(xCubic.xz + xCubic.yw, yCubic.xz + yCubic.yw);

    vec4 offset = c + vec4(xCubic.y, xCubic.w, yCubic.y, yCubic.w) / s;
	offset *= 1.0 / res.xxyy;

	vec4 sample0 = textureLod(tex, offset.xz, lod);
	vec4 sample1 = textureLod(tex, offset.yz, lod);
	vec4 sample2 = textureLod(tex, offset.xw, lod);
	vec4 sample3 = textureLod(tex, offset.yw, lod);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}
*/