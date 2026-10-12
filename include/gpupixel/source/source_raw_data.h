/*
 * GPUPixel
 *
 * Created by PixPark on 2021/6/24.
 * Copyright © 2021 PixPark. All rights reserved.
 */

#pragma once

#include <functional>

#include "gpupixel/filter/filter.h"
#include "gpupixel/source/source.h"

// Texture in / texture out (ProcessTexture, ProcessTextureDirect,
// SinkRawData::SetExternalTarget, GPUPixelFramebuffer::WrapTexture) is present.
#define GPUPIXEL_HAS_TEXTURE_IO 1

namespace gpupixel {
class GPUPixelGLProgram;
class GPUPIXEL_API SourceRawData : public Filter {
 public:
  static std::shared_ptr<SourceRawData> Create();

  ~SourceRawData() override;

  // Feed a frame that already lives in a GL texture (for example one backed by
  // an IOSurface / CVPixelBuffer) — no CPU upload. The texture must be sampled
  // as RGBA and stay valid until this call returns. Must be called with the
  // GPUPixel context current (use SyncRunWithContext).
  void ProcessTexture(uint32_t texture, int width, int height);

  // Same, but hands the texture straight to the first sink — no copy pass at
  // all. The texture must be set up for sampling by the caller (filter and
  // wrap modes) and stay valid until this call returns.
  void ProcessTextureDirect(uint32_t texture, int width, int height);

  void ProcessData(const uint8_t* data,
                   int width,
                   int height,
                   int stride,
                   GPUPIXEL_FRAME_TYPE type);

  void SetRotation(RotationMode rotation);

  bool Init();

 private:
  SourceRawData();

  int GenerateTextureWithPixels(const uint8_t* pixels,
                                int width,
                                int height,
                                int stride,
                                GPUPIXEL_FRAME_TYPE type);

 private:
  GPUPixelGLProgram* filter_program_;
  uint32_t filter_position_attribute_;
  uint32_t filter_tex_coord_attribute_;

  uint32_t texture_ = 0;
  RotationMode rotation_ = NoRotation;
  std::shared_ptr<GPUPixelFramebuffer> framebuffer_;
};

}  // namespace gpupixel
