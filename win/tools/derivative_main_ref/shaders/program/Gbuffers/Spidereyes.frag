
layout(location = 0) out vec3 albedoData;
layout(location = 1) out vec3 colortex7Out;

/* DRAWBUFFERS:67 */

uniform sampler2D tex;

uniform vec4 entityColor;

in vec4 tint;
in vec2 texcoord;
//in vec2 lightmap;

//#include "/lib/Head/Common.inc"
#include "/Settings.glsl"

//float bayer2 (	    vec2 a)  { a = 0.5 * floor(a); return fract(1.5 * fract(a.y) + a.x); }
//float bayer4 (const vec2 a)  { return bayer2 (0.5   * a) * 0.25     + bayer2(a); }

void main() {
	#ifdef ENTITY_EYES_LIGHTING 
	#endif

	vec4 albedo = texture(tex, texcoord) * tint;

	#ifdef ENTITY_STATUS_COLOR
		albedo.rgb = mix(albedo.rgb, entityColor.rgb, entityColor.a);
	#endif

	#ifdef WHITE_WORLD
		albedo.rgb = vec3(1.0);
	#endif

    //mat3 tbnMatrix = manualTBN(viewPos.xyz, texcoord);

	if (albedo.a < 0.1) { discard; return; }

	albedoData = albedo.rgb;

	//colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.z = 36.1 / 255.0;
}
