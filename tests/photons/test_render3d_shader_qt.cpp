#include "render3d/qtquick/CompactShaderSources.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>

#include <array>
#include <iostream>

namespace {

int fail(const char* message, const QString& details = {}) {
  std::cerr << message;
  if (!details.isEmpty()) {
    std::cerr << ": " << details.toStdString();
  }
  std::cerr << '\n';
  return 1;
}

} // namespace

int main(int argc, char** argv) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }

  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  QGuiApplication application(argc, argv);

  QOpenGLContext context;
  context.setFormat(format);
  if (!context.create()) {
    return fail("unable to create an OpenGL 3.3 context");
  }

  QOffscreenSurface surface;
  surface.setFormat(context.format());
  surface.create();
  if (!surface.isValid()) {
    return fail("unable to create the offscreen OpenGL surface");
  }
  if (!context.makeCurrent(&surface)) {
    return fail("unable to activate the offscreen OpenGL context");
  }

  QOpenGLShaderProgram program;
  if (!program.addShaderFromSourceCode(
          QOpenGLShader::Vertex,
          accloud::render3d::shader::kCompactVertexShader)) {
    return fail("compact vertex shader compilation failed", program.log());
  }
  if (!program.addShaderFromSourceCode(
          QOpenGLShader::Fragment,
          accloud::render3d::shader::kCompactFragmentShader)) {
    return fail("compact fragment shader compilation failed", program.log());
  }
  if (!program.link()) {
    return fail("compact shader program link failed", program.log());
  }

  constexpr std::array requiredUniforms{
      "u_mvp",
      "u_pitch",
      "u_baseLayer",
      "u_meshColor",
      "u_supportColor",
      "u_supportColoringEnabled",
      "u_lightDirection",
      "u_clipZ",
      "u_clipEpsilon",
      "u_hasLowerCut",
      "u_hasUpperCut",
      "u_cutSurfacePass",
  };
  for (const char* uniform : requiredUniforms) {
    if (program.uniformLocation(uniform) < 0) {
      return fail("required compact shader uniform is inactive", QString::fromLatin1(uniform));
    }
  }

  context.doneCurrent();
  std::cout << "Render3D compact shaders compiled and linked with all required uniforms\n";
  return 0;
}
