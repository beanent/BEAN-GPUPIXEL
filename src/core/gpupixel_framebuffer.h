/*
 * GPUPixel
 *
 * Created by PixPark on 2021/6/24.
 * Copyright © 2021 PixPark. All rights reserved.
 */

#pragma once

#include "core/gpupixel_gl_include.h"

#include <memory>
#include <vector>

namespace gpupixel {
typedef struct GPUPIXEL_API {
  GLenum minFilter;
  GLenum magFilter;
  GLenum wrapS;
  GLenum wrapT;
  GLenum internalFormat;
  GLenum format;
  GLenum type;
} TextureAttributes;

class GPUPIXEL_API GPUPixelFramebuffer {
 public:
  GPUPixelFramebuffer(
      int width,
      int height,
      bool only_generate_texture = false,
      const TextureAttributes texture_attributes = default_texture_attributes);
  ~GPUPixelFramebuffer();

  // Wrap a GL texture the caller owns (for example one backed by an IOSurface /
  // CVPixelBuffer) so a source can hand it on, or a filter can draw into it,
  // without a copy pass. The texture is NOT deleted with this object; it must
  // outlive every use. Call with the GPUPixel context current.
  static std::shared_ptr<GPUPixelFramebuffer> WrapTexture(uint32_t texture,
                                                          int width,
                                                          int height);

  uint32_t GetTexture() const { return texture_; }

  uint32_t GetFramebuffer() const { return framebuffer_; }

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  const TextureAttributes& GetTextureAttributes() const {
    return texture_attributes_;
  };
  bool HasFramebuffer() { return has_framebuffer_; };

  void Activate();
  void Deactivate();

  static TextureAttributes default_texture_attributes;

 private:
  int width_;
  int height_;
  TextureAttributes texture_attributes_;
  bool has_framebuffer_;
  bool owns_texture_ = true;
  struct WrapTag {};
  GPUPixelFramebuffer(WrapTag, uint32_t texture, int width, int height);
  uint32_t texture_;
  uint32_t framebuffer_;

  void GenerateTexture();
  void GenerateFramebuffer();

  //    static std::vector<std::shared_ptr<GPUPixelFramebuffer>> framebuffers_;
};

}  // namespace gpupixel
