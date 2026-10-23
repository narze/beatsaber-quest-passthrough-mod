#include "alpha_pass.hpp"

#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include "main.hpp"

namespace passthrough {

namespace {

constexpr const char *kVertexShader = R"(#version 300 es
void main() {
  // One triangle that covers the screen, no vertex buffer needed.
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char *kFragmentShader = R"(#version 300 es
#extension GL_EXT_shader_framebuffer_fetch : require
precision highp float;
uniform vec2 uKey;
inout vec4 oColor; // framebuffer fetch: the pixel the game rendered
void main() {
  float m = max(oColor.r, max(oColor.g, oColor.b));
  oColor.a = smoothstep(uKey.x, uKey.y, m);
}
)";

GLuint compile(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024] = {};
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    PaperLogger.error("alpha pass: shader compile failed: {}", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

// GL state the pass changes. Restored after every run.
struct SavedState {
  GLint drawFbo, readFbo, program, vao, viewport[4];
  GLboolean blend, depthTest, cullFace, scissorTest, stencilTest, colorMask[4];

  SavedState() {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_VIEWPORT, viewport);
    blend = glIsEnabled(GL_BLEND);
    depthTest = glIsEnabled(GL_DEPTH_TEST);
    cullFace = glIsEnabled(GL_CULL_FACE);
    scissorTest = glIsEnabled(GL_SCISSOR_TEST);
    stencilTest = glIsEnabled(GL_STENCIL_TEST);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
  }

  ~SavedState() {
    auto set = [](GLenum cap, GLboolean on) { on ? glEnable(cap) : glDisable(cap); };
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
    glUseProgram(program);
    glBindVertexArray(vao);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    set(GL_BLEND, blend);
    set(GL_DEPTH_TEST, depthTest);
    set(GL_CULL_FACE, cullFace);
    set(GL_SCISSOR_TEST, scissorTest);
    set(GL_STENCIL_TEST, stencilTest);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
  }
};

} // namespace

bool AlphaPass::Init() {
  initDone_ = true;
  GLuint vs = compile(GL_VERTEX_SHADER, kVertexShader);
  GLuint fs = compile(GL_FRAGMENT_SHADER, kFragmentShader);
  if (!vs || !fs) return false;
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024] = {};
    glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
    PaperLogger.error("alpha pass: link failed: {}", log);
    return false;
  }
  keyLocation_ = glGetUniformLocation(program_, "uKey");
  glGenVertexArrays(1, &vao_);
  glGenFramebuffers(1, &fbo_);
  PaperLogger.info("alpha pass: ready (program {}, vao {}, fbo {})", program_, vao_, fbo_);
  return true;
}

bool AlphaPass::Apply(const ReleaseTargetInfo &target, float low, float high) {
  if (failed_) return false;
  if (eglGetCurrentContext() == EGL_NO_CONTEXT) {
    PaperLogger.error("alpha pass: no GL context on this thread, disabled");
    failed_ = true;
    return false;
  }

  SavedState saved;
  if (!initDone_ && !Init()) {
    failed_ = true;
    return false;
  }

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);
  glViewport(0, 0, static_cast<GLsizei>(target.info.width), static_cast<GLsizei>(target.info.height));
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glUseProgram(program_);
  glUniform2f(keyLocation_, low, high);
  glBindVertexArray(vao_);

  uint32_t layers = target.info.arraySize > 0 ? target.info.arraySize : 1;
  for (uint32_t layer = 0; layer < layers; layer++) {
    if (layers > 1) {
      glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target.texture, 0,
                                static_cast<GLint>(layer));
    } else {
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    }
    if (applied_ == 0) {
      GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE) {
        PaperLogger.error("alpha pass: framebuffer incomplete 0x{:x}, disabled", status);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        failed_ = true;
        return false;
      }
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  // Detach, so our FBO does not keep a reference to the game's texture.
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

  if (applied_ == 0) {
    GLenum err = glGetError();
    PaperLogger.info("alpha pass: first run on texture {} ({}x{}x{}, format 0x{:x}), glError 0x{:x}",
                     target.texture, target.info.width, target.info.height, layers,
                     target.info.format, err);
  }
  applied_++;
  return true;
}

} // namespace passthrough
