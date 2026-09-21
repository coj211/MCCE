#version 450 compatibility

//out vec2 screenCoord;

void main() {
	gl_Position = vec4(gl_Vertex.xy * 2.0 - 1.0, 0.0, 1.0);

	//screenCoord = gl_MultiTexCoord0.xy;
}
