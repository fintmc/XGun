#version 330

in vec2 vPos;

out vec4 color;

uniform sampler2D tex;
uniform vec2 screenSize;

uniform vec2 sel1;
uniform vec2 sel2;

float insideBox(vec2 v, vec2 bottomLeft, vec2 topRight) {
  vec2 s = step(bottomLeft, v) - step(topRight, v);
  return abs(s.x * s.y);
}

void main() {
  vec4 interm = texture(tex, vPos);

  float dim = insideBox(vPos,
                        sel1/screenSize,
                        sel2/screenSize);

  color = mix(interm, vec4(vec3(0), 1), (1-dim)*0.6);
}
