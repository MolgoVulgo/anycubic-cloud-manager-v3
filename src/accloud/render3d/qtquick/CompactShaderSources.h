#pragma once

namespace accloud::render3d::shader {

inline constexpr char kCompactVertexShader[] = R"glsl(
#version 330 core
layout(location = 0) in uvec2 a_packed;
uniform mat4 u_mvp;
uniform vec3 u_pitch;
uniform float u_baseLayer;
out vec3 v_normal;
out float v_worldZ;
flat out uint v_semantic;

uint readBits(uint offset, uint count) {
  uint mask = (1u << count) - 1u;
  if (offset >= 32u)
    return (a_packed.y >> (offset - 32u)) & mask;
  if (offset + count <= 32u)
    return (a_packed.x >> offset) & mask;
  uint lowCount = 32u - offset;
  uint highCount = count - lowCount;
  uint low = a_packed.x >> offset;
  uint highMask = (1u << highCount) - 1u;
  return (low | ((a_packed.y & highMask) << lowCount)) & mask;
}

int logicalCorner() {
  if (gl_VertexID == 3)
    return 0;
  if (gl_VertexID == 4)
    return 2;
  if (gl_VertexID == 5)
    return 3;
  return gl_VertexID;
}

void main() {
  uint face = readBits(61u, 3u);
  vec3 p0;
  vec3 p1;
  vec3 p2;
  vec3 p3;
  vec3 normal;
  float baseZ = u_baseLayer * u_pitch.z;

  if (face <= 1u) {
    float x = float(readBits(0u, 14u)) * u_pitch.x;
    float y0 = float(readBits(14u, 13u)) * u_pitch.y;
    float y1 = float(readBits(27u, 13u)) * u_pitch.y;
    float z0 = baseZ + float(readBits(40u, 6u)) * u_pitch.z;
    float z1 = baseZ + float(readBits(46u, 6u)) * u_pitch.z;
    if (face == 1u) {
      p0 = vec3(x, y0, z0); p1 = vec3(x, y1, z0);
      p2 = vec3(x, y1, z1); p3 = vec3(x, y0, z1);
      normal = vec3(1.0, 0.0, 0.0);
    } else {
      p0 = vec3(x, y0, z0); p1 = vec3(x, y0, z1);
      p2 = vec3(x, y1, z1); p3 = vec3(x, y1, z0);
      normal = vec3(-1.0, 0.0, 0.0);
    }
  } else if (face <= 3u) {
    float y = float(readBits(0u, 13u)) * u_pitch.y;
    float x0 = float(readBits(13u, 14u)) * u_pitch.x;
    float x1 = float(readBits(27u, 14u)) * u_pitch.x;
    float z0 = baseZ + float(readBits(41u, 6u)) * u_pitch.z;
    float z1 = baseZ + float(readBits(47u, 6u)) * u_pitch.z;
    if (face == 3u) {
      p0 = vec3(x0, y, z0); p1 = vec3(x0, y, z1);
      p2 = vec3(x1, y, z1); p3 = vec3(x1, y, z0);
      normal = vec3(0.0, 1.0, 0.0);
    } else {
      p0 = vec3(x0, y, z0); p1 = vec3(x1, y, z0);
      p2 = vec3(x1, y, z1); p3 = vec3(x0, y, z1);
      normal = vec3(0.0, -1.0, 0.0);
    }
  } else {
    float x0 = float(readBits(0u, 14u)) * u_pitch.x;
    float x1 = float(readBits(14u, 14u)) * u_pitch.x;
    float y0 = float(readBits(28u, 13u)) * u_pitch.y;
    float y1 = float(readBits(41u, 13u)) * u_pitch.y;
    float z = baseZ + float(readBits(54u, 6u)) * u_pitch.z;
    if (face == 5u) {
      p0 = vec3(x0, y0, z); p1 = vec3(x1, y0, z);
      p2 = vec3(x1, y1, z); p3 = vec3(x0, y1, z);
      normal = vec3(0.0, 0.0, 1.0);
    } else {
      p0 = vec3(x0, y0, z); p1 = vec3(x0, y1, z);
      p2 = vec3(x1, y1, z); p3 = vec3(x1, y0, z);
      normal = vec3(0.0, 0.0, -1.0);
    }
  }

  int corner = logicalCorner();
  vec3 position = corner == 0 ? p0 : (corner == 1 ? p1 : (corner == 2 ? p2 : p3));
  v_normal = normal;
  v_worldZ = position.z;
  v_semantic = readBits(60u, 1u);
  gl_Position = u_mvp * vec4(position, 1.0);
}
)glsl";

inline constexpr char kCompactFragmentShader[] = R"glsl(
#version 330 core
in vec3 v_normal;
in float v_worldZ;
flat in uint v_semantic;
uniform vec4 u_meshColor;
uniform vec4 u_supportColor;
uniform bool u_supportColoringEnabled;
uniform vec3 u_lightDirection;
uniform vec2 u_clipZ;
uniform float u_clipEpsilon;
uniform bool u_hasLowerCut;
uniform bool u_hasUpperCut;
uniform bool u_cutSurfacePass;
out vec4 fragColor;
void main() {
  if (v_worldZ < u_clipZ.x || v_worldZ > u_clipZ.y)
    discard;
  if (!u_cutSurfacePass && abs(v_normal.z) > 0.5) {
    float epsilon = u_clipEpsilon;
    if ((u_hasLowerCut && abs(v_worldZ - u_clipZ.x) <= epsilon)
        || (u_hasUpperCut && abs(v_worldZ - u_clipZ.y) <= epsilon))
      discard;
  }
  float diffuse = abs(dot(normalize(v_normal), -u_lightDirection));
  float light = 0.28 + 0.72 * diffuse;
  vec4 materialColor = u_supportColoringEnabled && v_semantic == 1u
                           ? u_supportColor
                           : u_meshColor;
  fragColor = vec4(materialColor.rgb * light, materialColor.a);
}
)glsl";

} // namespace accloud::render3d::shader
