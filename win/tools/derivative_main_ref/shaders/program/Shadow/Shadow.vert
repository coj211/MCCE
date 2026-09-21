
#include "/lib/Head/Common.inc"

uniform vec3 cameraPosition;
uniform mat4 shadowModelViewInverse;

uniform int blockEntityId;

#ifndef MC_GL_VENDOR_INTEL
	#define attribute in
#endif

attribute vec4 mc_Entity;
attribute vec4 at_tangent;

out vec2 texcoord;
out vec3 tint;
out vec2 lightmap;
out vec3 viewPos;
out vec3 minecraftPos;

flat out mat3 tbnMatrix;

flat out float isWater;

uniform mat4 shadowProjection;

#include "/lib/Lighting/ShadowDistortion.glsl"

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	if (blockEntityId == 10030) {
		gl_Position = vec4(-1.0);
		return;
	}

    tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
	#ifdef SHADOW_BACKFACE_CULLING
		if (tbnMatrix[2].z < 0.0) {
			gl_Position = vec4(-1.0);
			return;
		}
	#endif

	tint = gl_Color.rgb;

	isWater = 0.0;
	if (int(mc_Entity.x) == 10017) {
		tbnMatrix[0] = normalize(gl_NormalMatrix * at_tangent.xyz);
		tbnMatrix[1] = cross(tbnMatrix[0], tbnMatrix[2]) * sign(at_tangent.w);

		isWater = 1.0;
	}

	lightmap = gl_MultiTexCoord1.xy * rcp(240.0);
	texcoord = mat2(gl_TextureMatrix[0]) * gl_MultiTexCoord0.xy + gl_TextureMatrix[0][3].xy;

	viewPos = transMAD(gl_ModelViewMatrix, gl_Vertex.xyz);
	minecraftPos = transMAD(shadowModelViewInverse, viewPos) + cameraPosition;

	gl_Position.xyz = DistortShadowSpace(projMAD(gl_ProjectionMatrix, viewPos));
	gl_Position.w = 1.0;
}
