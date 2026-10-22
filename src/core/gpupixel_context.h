/*
 * GPUPixel
 *
 * Created by PixPark on 2021/6/24.
 * Copyright © 2021 PixPark. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "core/gpupixel_framebuffer_factory.h"
#include "gpupixel/filter/filter.h"
#include "gpupixel/gpupixel_define.h"

#include "core/gpupixel_gl_include.h"
#include "core/gpupixel_program.h"

class DispatchQueue;

namespace gpupixel {

class GPUPIXEL_API GPUPixelContext {
 public:
  static GPUPixelContext* GetInstance();
  static void Destroy();

  FramebufferFactory* GetFramebufferFactory() const;
  void SetActiveGlProgram(GPUPixelGLProgram* shaderProgram);
  void Clean();

  void SyncRunWithContext(std::function<void(void)> func);
  void UseAsCurrent(void);
  void PresentBufferForDisplay();

#if defined(GPUPIXEL_IOS)
  EAGLContext* GetEglContext() const { return egl_context_; };
#elif defined(GPUPIXEL_MAC)
  NSOpenGLContext* GetOpenGLContext() const {
    return image_processing_context_;
  };
#elif defined(GPUPIXEL_ANDROID) || defined(GPUPIXEL_ANGLE)
  EGLContext GetEglContext() const { return egl_context_; };
  EGLDisplay GetEglDisplay() const { return egl_display_; };
  EGLSurface GetEglSurface() const { return egl_surface_; };
  EGLConfig GetEglConfig() const { return egl_config_; };
#elif defined(GPUPIXEL_WIN) || defined(GPUPIXEL_LINUX)
  GLFWwindow* GetGLContext() const { return gl_context_; };
#endif

#if defined(GPUPIXEL_ANGLE)
  // BEAN (Windows): which ANGLE back end draws. Call BEFORE the first
  // GetInstance(). 0 = automatic (Direct3D 11 -> Direct3D 11 software (WARP)
  // -> system OpenGL), 1 = Direct3D 11, 2 = Direct3D 11 software, 3 = system
  // OpenGL. A back end that cannot be created or fails the self test is
  // skipped and the next one is tried.
  static void SetAngleBackend(int backend);
  // Pin Direct3D 11 to one graphics card by its adapter LUID (DXGI). Call
  // before the first GetInstance(). Not set = ANGLE's default adapter.
  static void SetAngleAdapterLuid(int32_t high, uint32_t low);
  // The back end that was actually created: 1, 2 or 3 (0 = none yet).
  static int GetAngleBackendInUse();
  // GL_RENDERER of the created context, e.g.
  // "ANGLE (NVIDIA, NVIDIA GeForce RTX 3080 Ti Direct3D11 vs_5_0 ps_5_0)".
  static std::string GetAngleDescription();
#endif

 private:
  GPUPixelContext();
  ~GPUPixelContext();

  void Init();

  void CreateContext();
  void ReleaseContext();
#if defined(GPUPIXEL_ANGLE)
  bool CreateAngleContext();
  bool TryAngleBackend(int backend);
  void TeardownAngle();
#endif

 private:
  static GPUPixelContext* instance_;
  static std::mutex mutex_;
  FramebufferFactory* framebuffer_factory_;
  GPUPixelGLProgram* current_shader_program_;
  std::shared_ptr<DispatchQueue> task_queue_;

#if defined(GPUPIXEL_IOS)
  EAGLContext* egl_context_;
#elif defined(GPUPIXEL_MAC)
  NSOpenGLPixelFormat* pixel_format_;
  NSOpenGLContext* image_processing_context_;
#elif defined(GPUPIXEL_ANDROID) || defined(GPUPIXEL_ANGLE)
  EGLDisplay egl_display_;
  EGLConfig egl_config_;
  EGLSurface egl_surface_;
  EGLContext egl_context_;
#elif defined(GPUPIXEL_WIN) || defined(GPUPIXEL_LINUX)
  GLFWwindow* gl_context_;
#elif defined(GPUPIXEL_WASM)
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE wasm_context_;
#endif
};

}  // namespace gpupixel
