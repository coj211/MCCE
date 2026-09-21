
#include "/lib/Head/Common.inc"

uniform vec3 cameraPosition;
uniform mat4 shadowModelViewInverse;

#ifndef MC_GL_VENDOR_INTEL
	#define attribute in
#endif

attribute vec4 at_tangent;

out vec4 tint;
out vec2 lightmap;
out vec3 viewPos;
out vec3 minecraftPos;

flat out mat3 tbnMatrix;

flat out float isWater;

uniform mat4 shadowProjection;

#include "/lib/Lighting/ShadowDistortion.glsl"

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
    tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
	#ifdef SHADOW_BACKFACE_CULLING
		if (tbnMatrix[2].z < 0.0) {
			gl_Position = vec4(-1.0);
			return;
		}
	#endif

	tint = gl_Color;

	isWater = 0.0;
	if (dhMaterialId == DH_BLOCK_WATER) {
		tbnMatrix[0] = normalize(gl_NormalMatrix * at_tangent.xyz);
		tbnMatrix[1] = cross(tbnMatrix[0], tbnMatrix[2]) * sign(at_tangent.w);

		isWater = 1.0;
	}

	lightmap = mat2(gl_TextureMatrix[1]) * gl_MultiTexCoord1.xy + gl_TextureMatrix[1][3].xy;
	lightmap = saturate((lightmap - 0.03125) * 1.06667);

	viewPos = transMAD(gl_ModelViewMatrix, gl_Vertex.xyz);
	minecraftPos = transMAD(shadowModelViewInverse, viewPos) + cameraPosition;

	gl_Position.xyz = DistortShadowSpace(projMAD(gl_ProjectionMatrix, viewPos));
	gl_Position.w = 1.0;
}
