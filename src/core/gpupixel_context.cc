/*
 * GPUPixel
 *
 * Created by PixPark on 2021/6/24.
 * Copyright © 2021 PixPark. All rights reserved.
 */

#include "core/gpupixel_context.h"
#include "utils/dispatch_queue.h"
#include "utils/logging.h"
#include "utils/util.h"
#if defined(GPUPIXEL_WASM)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace gpupixel {

GPUPixelContext* GPUPixelContext::instance_ = 0;
std::mutex GPUPixelContext::mutex_;

GPUPixelContext::GPUPixelContext() : current_shader_program_(0) {
  LOG_DEBUG("Creating GPUPixelContext");
#if !defined(GPUPIXEL_WASM)
  task_queue_ = std::make_shared<DispatchQueue>();
#endif
  framebuffer_factory_ = new FramebufferFactory();
  Init();
}

GPUPixelContext::~GPUPixelContext() {
  LOG_DEBUG("Destroying GPUPixelContext");
  ReleaseContext();
  delete framebuffer_factory_;
  task_queue_->stop();
}

GPUPixelContext* GPUPixelContext::GetInstance() {
  if (!instance_) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!instance_) {
      instance_ = new (std::nothrow) GPUPixelContext;
    }
  }
  return instance_;
};

void GPUPixelContext::Destroy() {
  if (instance_) {
    delete instance_;
    instance_ = 0;
  }
}

void GPUPixelContext::Init() {
  SyncRunWithContext([=] {
    LOG_INFO("Initializing GPUPixelContext");
    this->CreateContext();
  });
}

FramebufferFactory* GPUPixelContext::GetFramebufferFactory() const {
  return framebuffer_factory_;
}

void GPUPixelContext::SetActiveGlProgram(GPUPixelGLProgram* shaderProgram) {
  if (current_shader_program_ != shaderProgram) {
    current_shader_program_ = shaderProgram;
    shaderProgram->UseProgram();
  }
}

void GPUPixelContext::Clean() {
  LOG_DEBUG("Cleaning GPUPixelContext resources");
  framebuffer_factory_->Clean();
}

#if defined(GPUPIXEL_ANGLE)
// ── BEAN: Windows ANGLE (OpenGL ES 2 -> Direct3D 11) ────────────────────────
namespace {
int g_angle_pref = 0;        // asked back end (0 auto)
int g_angle_used = 0;        // back end that was created (1 D3D11, 2 WARP, 3 OpenGL)
bool g_luid_set = false;
int32_t g_luid_high = 0;
uint32_t g_luid_low = 0;
std::string g_angle_desc;

// Draw one triangle strip into a 4x4 texture and read it back — a back end whose
// driver compiles shaders but draws nothing / garbage is rejected here, so the
// next back end is tried instead of every frame coming out black.
bool AngleSelfTest() {
  GLuint tex = 0, fbo = 0, vs = 0, fs = 0, prog = 0;
  bool ok = false;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
    const char* vsrc = "attribute vec4 p; void main(){ gl_Position = p; }";
    const char* fsrc = "precision highp float; void main(){ gl_FragColor = vec4(0.2, 0.4, 0.6, 1.0); }";
    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, nullptr);
    glCompileShader(vs);
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, nullptr);
    glCompileShader(fs);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "p");
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked) {
      static const float quad[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};
      glViewport(0, 0, 4, 4);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);
      glClearColor(1.f, 0.f, 0.f, 1.f);
      glClear(GL_COLOR_BUFFER_BIT);
      glUseProgram(prog);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      unsigned char px[4 * 4 * 4] = {0};
      glReadPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, px);
      auto close = [](int a, int b) { return a - b <= 3 && b - a <= 3; };
      ok = glGetError() == GL_NO_ERROR && close(px[0], 51) && close(px[1], 102) &&
           close(px[2], 153) && close(px[3], 255) && close(px[60], 51) &&
           close(px[61], 102) && close(px[62], 153);
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(0);
  if (prog) glDeleteProgram(prog);
  if (vs) glDeleteShader(vs);
  if (fs) glDeleteShader(fs);
  if (fbo) glDeleteFramebuffers(1, &fbo);
  if (tex) glDeleteTextures(1, &tex);
  return ok;
}
}  // namespace

void GPUPixelContext::SetAngleBackend(int backend) {
  g_angle_pref = (backend >= 1 && backend <= 3) ? backend : 0;
}

void GPUPixelContext::SetAngleAdapterLuid(int32_t high, uint32_t low) {
  g_luid_set = true;
  g_luid_high = high;
  g_luid_low = low;
}

int GPUPixelContext::GetAngleBackendInUse() {
  return g_angle_used;
}

std::string GPUPixelContext::GetAngleDescription() {
  return g_angle_desc;
}

void GPUPixelContext::TeardownAngle() {
  if (egl_display_ != EGL_NO_DISPLAY) {
    eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl_surface_ != EGL_NO_SURFACE)
      eglDestroySurface(egl_display_, egl_surface_);
    if (egl_context_ != EGL_NO_CONTEXT)
      eglDestroyContext(egl_display_, egl_context_);
    eglTerminate(egl_display_);
  }
  egl_display_ = EGL_NO_DISPLAY;
  egl_surface_ = EGL_NO_SURFACE;
  egl_context_ = EGL_NO_CONTEXT;
  egl_config_ = nullptr;
}

bool GPUPixelContext::TryAngleBackend(int backend) {
  auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
      eglGetProcAddress("eglGetPlatformDisplayEXT"));
  if (!getPlatformDisplay) {
    LOG_ERROR("ANGLE: eglGetPlatformDisplayEXT is missing");
    return false;
  }
  std::vector<EGLint> attribs;
  attribs.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
  attribs.push_back(backend == 3 ? EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE
                                 : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
  if (backend == 2) {
    attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
    attribs.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE);
  }
  if (backend == 1 && g_luid_set) {
    attribs.push_back(EGL_PLATFORM_ANGLE_D3D_LUID_HIGH_ANGLE);
    attribs.push_back(static_cast<EGLint>(g_luid_high));
    attribs.push_back(EGL_PLATFORM_ANGLE_D3D_LUID_LOW_ANGLE);
    attribs.push_back(static_cast<EGLint>(g_luid_low));
  }
  attribs.push_back(EGL_NONE);

  egl_display_ = getPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, attribs.data());
  if (egl_display_ == EGL_NO_DISPLAY) {
    LOG_ERROR("ANGLE back end {}: no display", backend);
    return false;
  }
  EGLint major = 0, minor = 0;
  if (!eglInitialize(egl_display_, &major, &minor)) {
    LOG_ERROR("{}", "ANGLE back end " + std::to_string(backend) + ": eglInitialize failed, EGL error " + std::to_string((int)eglGetError()));
    return false;
  }
  const EGLint configAttribs[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                  EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
                                  EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
  EGLint numConfigs = 0;
  if (!eglChooseConfig(egl_display_, configAttribs, &egl_config_, 1, &numConfigs) || numConfigs < 1) {
    LOG_ERROR("ANGLE back end {}: no config", backend);
    return false;
  }
  const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, contextAttribs);
  if (egl_context_ == EGL_NO_CONTEXT) {
    LOG_ERROR("{}", "ANGLE back end " + std::to_string(backend) + ": eglCreateContext failed, EGL error " + std::to_string((int)eglGetError()));
    return false;
  }
  const EGLint pbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  egl_surface_ = eglCreatePbufferSurface(egl_display_, egl_config_, pbufferAttribs);
  if (egl_surface_ == EGL_NO_SURFACE) {
    LOG_ERROR("ANGLE back end {}: pbuffer surface failed", backend);
    return false;
  }
  if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_)) {
    LOG_ERROR("ANGLE back end {}: eglMakeCurrent failed", backend);
    return false;
  }
  if (!AngleSelfTest()) {
    LOG_ERROR("ANGLE back end {}: self test failed", backend);
    return false;
  }
  const char* r = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  g_angle_desc = r ? r : "";
  g_angle_used = backend;
  LOG_INFO("{}", "ANGLE back end " + std::to_string(backend) + " ready (EGL " + std::to_string(major) + "." + std::to_string(minor) + "): " + g_angle_desc);
  return true;
}

bool GPUPixelContext::CreateAngleContext() {
  egl_display_ = EGL_NO_DISPLAY;
  egl_surface_ = EGL_NO_SURFACE;
  egl_context_ = EGL_NO_CONTEXT;
  egl_config_ = nullptr;
  int order[3];
  switch (g_angle_pref) {
    case 2: order[0] = 2; order[1] = 1; order[2] = 3; break;
    case 3: order[0] = 3; order[1] = 1; order[2] = 2; break;
    default: order[0] = 1; order[1] = 2; order[2] = 3; break;
  }
  if (g_angle_pref == 1) { order[0] = 1; order[1] = 2; order[2] = 3; }
  for (int i = 0; i < 3; ++i) {
    if (TryAngleBackend(order[i]))
      return true;
    TeardownAngle();
  }
  g_angle_used = 0;
  g_angle_desc.clear();
  return false;
}
#endif  // GPUPIXEL_ANGLE


void GPUPixelContext::CreateContext() {
#if defined(GPUPIXEL_IOS)
  LOG_DEBUG("Creating iOS OpenGL ES 2.0 context");
  egl_context_ = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
  if (!egl_context_) {
    LOG_ERROR("Failed to create iOS OpenGL ES 2.0 context");
    return;
  }
  [EAGLContext setCurrentContext:egl_context_];
  LOG_INFO("iOS OpenGL ES 2.0 context created successfully");
#elif defined(GPUPIXEL_MAC)
  LOG_DEBUG("Creating macOS OpenGL context");
  NSOpenGLPixelFormatAttribute pixelFormatAttributes[] = {
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAOpenGLProfile,
      NSOpenGLProfileVersionLegacy,
      NSOpenGLPFAAccelerated,
      0,
      NSOpenGLPFAColorSize,
      24,
      NSOpenGLPFAAlphaSize,
      8,
      NSOpenGLPFADepthSize,
      24,
      0};

  pixel_format_ =
      [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelFormatAttributes];
  if (!pixel_format_) {
    LOG_ERROR("Failed to create NSOpenGLPixelFormat");
    return;
  }

  image_processing_context_ =
      [[NSOpenGLContext alloc] initWithFormat:pixel_format_ shareContext:nil];
  if (!image_processing_context_) {
    LOG_ERROR("Failed to create NSOpenGLContext");
    return;
  }

  GLint interval = 0;
  [image_processing_context_ makeCurrentContext];
  [image_processing_context_ setValues:&interval
                          forParameter:NSOpenGLContextParameterSwapInterval];
  LOG_INFO("macOS OpenGL context created successfully");
#elif defined(GPUPIXEL_ANDROID)
  LOG_DEBUG("Creating Android EGL context");
  // Initialize EGL
  egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (egl_display_ == EGL_NO_DISPLAY) {
    LOG_ERROR("Failed to get EGL display");
    return;
  }

  EGLint major, minor;
  if (!eglInitialize(egl_display_, &major, &minor)) {
    LOG_ERROR("Failed to initialize EGL");
    return;
  }
  LOG_DEBUG("EGL initialized: version major:{} minor:{}", major, minor);

  // Configure EGL - support both PBUFFER (off-screen) and WINDOW (window) Surface
  // This allows it to be used for both off-screen rendering (filter processing) and window rendering (display to screen)
  const EGLint configAttribs[] = {
      EGL_RED_SIZE, 8,           // 8 bits for red channel
      EGL_GREEN_SIZE, 8,         // 8 bits for green channel
      EGL_BLUE_SIZE, 8,          // 8 bits for blue channel
      EGL_ALPHA_SIZE, 8,         // 8 bits for alpha channel
      EGL_DEPTH_SIZE, 16,        // 16 bits for depth buffer
      EGL_STENCIL_SIZE, 0,       // 0 bits for stencil buffer (not used)
      EGL_SURFACE_TYPE,
      EGL_PBUFFER_BIT | EGL_WINDOW_BIT,  // Key: support both off-screen and window Surface
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT,        // Use OpenGL ES 2.0
      EGL_NONE};

  EGLint numConfigs;
  if (!eglChooseConfig(egl_display_, configAttribs, &egl_config_, 1,
                       &numConfigs)) {
    LOG_ERROR("Failed to choose EGL config");
    return;
  }

  // Create EGL context
  const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT,
                                  contextAttribs);
  if (egl_context_ == EGL_NO_CONTEXT) {
    LOG_ERROR("Failed to create EGL context");
    return;
  }

  // Create offscreen rendering surface
  const EGLint pbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};

  egl_surface_ =
      eglCreatePbufferSurface(egl_display_, egl_config_, pbufferAttribs);
  if (egl_surface_ == EGL_NO_SURFACE) {
    LOG_ERROR("Failed to create EGL surface");
    return;
  }

  // Set current context
  if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_)) {
    LOG_ERROR("Failed to make EGL context current");
    return;
  }
  LOG_INFO("Android EGL context created successfully");
#elif defined(GPUPIXEL_ANGLE)
  LOG_DEBUG("Creating Windows ANGLE context");
  if (!CreateAngleContext()) {
    LOG_ERROR("Failed to create any ANGLE context");
    return;
  }
#elif defined(GPUPIXEL_WIN) || defined(GPUPIXEL_LINUX)
  LOG_DEBUG("Creating Windows/Linux OpenGL context");
  int ret = glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  if (ret) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  } else {
    LOG_ERROR("Failed to initialize GLFW");
    return;
  }
  gl_context_ = glfwCreateWindow(1, 1, "gpupixel opengl context", NULL, NULL);
  if (!gl_context_) {
    LOG_ERROR("Failed to create GLFW window");
    glfwTerminate();
    return;
  }
  glfwMakeContextCurrent(gl_context_);

  if (!gladLoadGL()) {
    LOG_ERROR("Failed to initialize GLAD");
    return;
  }
  LOG_INFO("Windows/Linux OpenGL context created successfully");
#elif defined(GPUPIXEL_WASM)
  LOG_DEBUG("Creating WebGL context");
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;  // Use WebGL 2.0
  attrs.minorVersion = 0;
  wasm_context_ = emscripten_webgl_create_context("#gpupixel_canvas", &attrs);
  if (wasm_context_ <= 0) {
    LOG_ERROR("Failed to create WebGL context: {}", wasm_context_);
    return;
  }
  emscripten_webgl_make_context_current(wasm_context_);
  LOG_INFO("WebGL context created successfully");
#endif
}

void GPUPixelContext::UseAsCurrent() {
#if defined(GPUPIXEL_IOS)
  if ([EAGLContext currentContext] != egl_context_) {
    LOG_TRACE("Setting current EAGLContext");
    [EAGLContext setCurrentContext:egl_context_];
  }
#elif defined(GPUPIXEL_MAC)
  if ([NSOpenGLContext currentContext] != image_processing_context_) {
    LOG_TRACE("Setting current NSOpenGLContext");
    [image_processing_context_ makeCurrentContext];
  }
#elif defined(GPUPIXEL_ANDROID) || defined(GPUPIXEL_ANGLE)
  if (eglGetCurrentContext() != egl_context_) {
    LOG_TRACE("Setting current EGL context");
    eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
  }
#elif defined(GPUPIXEL_WIN) || defined(GPUPIXEL_LINUX)
  if (glfwGetCurrentContext() != gl_context_) {
    LOG_TRACE("Setting current GLFW context");
    glfwMakeContextCurrent(gl_context_);
  }
#elif defined(GPUPIXEL_WASM)
  LOG_TRACE("Setting current WebGL context");
  emscripten_webgl_make_context_current(wasm_context_);
#endif
}

void GPUPixelContext::PresentBufferForDisplay() {
#if defined(GPUPIXEL_IOS)
  [egl_context_ presentRenderbuffer:GL_RENDERBUFFER];
#elif defined(GPUPIXEL_MAC)
  // No implementation needed
#elif defined(GPUPIXEL_ANDROID) || defined(GPUPIXEL_ANGLE)
  // For offscreen rendering, no need to swap buffers
  // If display to screen is needed, use eglSwapBuffers(egl_display_,
  // egl_surface_);
#endif
}

void GPUPixelContext::ReleaseContext() {
  LOG_DEBUG("Releasing OpenGL context");
#if defined(GPUPIXEL_ANDROID) || defined(GPUPIXEL_ANGLE)
  if (egl_display_ != EGL_NO_DISPLAY) {
    eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    if (egl_surface_ != EGL_NO_SURFACE) {
      LOG_TRACE("Destroying EGL surface");
      eglDestroySurface(egl_display_, egl_surface_);
      egl_surface_ = EGL_NO_SURFACE;
    }

    if (egl_context_ != EGL_NO_CONTEXT) {
      LOG_TRACE("Destroying EGL context");
      eglDestroyContext(egl_display_, egl_context_);
      egl_context_ = EGL_NO_CONTEXT;
    }

    LOG_TRACE("Terminating EGL display");
    eglTerminate(egl_display_);
    egl_display_ = EGL_NO_DISPLAY;
  }
#elif defined(GPUPIXEL_WIN) || defined(GPUPIXEL_LINUX)
  if (gl_context_) {
    LOG_TRACE("Destroying GLFW window");
    glfwDestroyWindow(gl_context_);
  }
  LOG_TRACE("Terminating GLFW");
  glfwTerminate();
#elif defined(GPUPIXEL_WASM)
  LOG_TRACE("Destroying WebGL context");
  emscripten_webgl_destroy_context(wasm_context_);
#endif
  LOG_INFO("OpenGL context released successfully");
}

void GPUPixelContext::SyncRunWithContext(std::function<void(void)> task) {
#if defined(GPUPIXEL_IOS) || defined(GPUPIXEL_MAC)
  if (!Util::IsAppleAppActive()) {
    return;
  }
#endif

#if defined(GPUPIXEL_WASM)
  LOG_TRACE("Running task synchronously (WebGL)");
  UseAsCurrent();
  task();
#else
  LOG_TRACE("Running task on task queue");
  task_queue_->runTask([=]() {
    UseAsCurrent();
    task();
  });
#endif
}
}  // namespace gpupixel
