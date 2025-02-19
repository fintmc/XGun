#version 330

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 texPos;

out vec2 vPos;

void main() {
  gl_Position = vec4(pos, 0, 1);
  vPos = texPos;
}
