#version 450 compatibility

void main() {
	gl_Position = vec4(gl_Vertex.xy * vec2(1.0, 2.0) - 1.0, 0.0, 1.0);
}
