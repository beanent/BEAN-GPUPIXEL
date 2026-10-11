//
//  source_raw_output.h
//  GPUPixel iOS
//
//  Created by PixPark on 2021/6/24.
//  Copyright © 2021 PixPark. All rights reserved.
//

#pragma once

#include <stdio.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "gpupixel/sink/sink.h"

#if defined(GPUPIXEL_IOS) || defined(GPUPIXEL_MAC)
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#endif

namespace gpupixel {
class GPUPixelGLProgram;
class GPUPIXEL_API SinkRawData : public Sink {
 public:
  static std::shared_ptr<SinkRawData> Create();
  virtual ~SinkRawData();
  void Render() override;

  const uint8_t* GetRgbaBuffer();

  // Render the next frames straight into this GL texture (for example one
  // backed by an IOSurface / CVPixelBuffer) instead of the internal buffer —
  // no glReadPixels. Pass 0 to go back to the internal buffer. The texture
  // must match the frame size. Call with the GPUPixel context current.
  void SetExternalTarget(uint32_t texture, int width, int height);
  const uint8_t* GetI420Buffer();
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

 private:
  int RenderToOutput();
  bool InitWithShaderString(const std::string& vertex_shader_source,
                            const std::string& fragment_shader_source);
  void InitTextureCache(int width, int height);
  void InitFramebuffer(int width, int height);
  void InitOutputBuffer(int width, int height);

 private:
  SinkRawData();
  std::mutex mutex_;
  GPUPixelGLProgram* shader_program_;
  uint32_t position_attribute_;
  uint32_t tex_coord_attribute_;

  std::shared_ptr<GPUPixelFramebuffer> framebuffer_;

  bool is_initialized_ = false;

  // Image dimensions
  int32_t width_ = 0;
  uint32_t external_texture_ = 0;
  uint32_t external_fbo_ = 0;
  int32_t external_width_ = 0;
  int32_t external_height_ = 0;
  int32_t height_ = 0;

  // Frame buffers for pixel data
  uint8_t* rgba_buffer_ = nullptr;  // RGBA buffer
  uint8_t* yuv_buffer_ = nullptr;   // YUV buffer
};

class GPUPIXEL_API SinkRawYuv : public Sink {
 public:
  static std::shared_ptr<SinkRawYuv> Create();
  virtual ~SinkRawYuv();
  void Render() override;

  const uint8_t* GetI420Buffer();
  void GetI420Buffer(uint8_t* input_buffer);
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

 private:
  int RenderToOutput(uint8_t* buffer);
  bool InitWithShaderString(const std::string& vertex_shader_source,
                            const std::string& fragment_shader_source);
  void InitTextureCache(int width, int height);
  void InitFramebuffer(int width, int height);
  void InitOutputBuffer(int width, int height);

 private:
  SinkRawYuv();
  std::mutex mutex_;
  GPUPixelGLProgram* shader_program_;
  uint32_t position_attribute_;
  uint32_t tex_coord_attribute_;

  std::shared_ptr<GPUPixelFramebuffer> framebuffer_;

  // Image dimensions
  int32_t width_ = 0;
  int32_t height_ = 0;

  uint8_t* yuv_buffer_ = nullptr;   // YUV buffer
  uint8_t* uv_buffer_ = nullptr;   // UV reconstruct buffer

};

}  // namespace gpupixel
