// Copyright (c) 2019 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/data_url.h"
#include "shell/common/asar/asar_util.h"
#include "shell/common/node_includes.h"
#include "shell/common/skia_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_util.h"

#if defined(OS_WIN)
#include "ui/gfx/icon_util.h"
#endif

namespace electron {

namespace util {

struct ScaleFactorPair {
  const char* name;
  float scale;
};

ScaleFactorPair kScaleFactorPairs[] = {
    // The "@2x" is put as first one to make scale matching faster.
    {"@2x", 2.0f},   {"@3x", 3.0f},     {"@1x", 1.0f},     {"@4x", 4.0f},
    {"@5x", 5.0f},   {"@1.25x", 1.25f}, {"@1.33x", 1.33f}, {"@1.4x", 1.4f},
    {"@1.5x", 1.5f}, {"@1.8x", 1.8f},   {"@2.5x", 2.5f},
};

float GetScaleFactorFromPath(const base::FilePath& path) {
  std::string filename(path.BaseName().RemoveExtension().AsUTF8Unsafe());

  // We don't try to convert string to float here because it is very very
  // expensive.
  for (const auto& kScaleFactorPair : kScaleFactorPairs) {
    if (base::EndsWith(filename, kScaleFactorPair.name,
                       base::CompareCase::INSENSITIVE_ASCII))
      return kScaleFactorPair.scale;
  }

  return 1.0f;
}

bool AddImageSkiaRepFromPNG(gfx::ImageSkia* image,
                            const unsigned char* data,
                            size_t size,
                            double scale_factor) {
  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(data, size, &bitmap))
    return false;

  image->AddRepresentation(gfx::ImageSkiaRep(bitmap, scale_factor));
  return true;
}

bool AddImageSkiaRepFromJPEG(gfx::ImageSkia* image,
                             const unsigned char* data,
                             size_t size,
                             double scale_factor) {
  LOG(INFO) << "IN AddImageSkiaRepFromJPEG";
  auto bitmap = gfx::JPEGCodec::Decode(data, size);
  if (!bitmap) {
    LOG(INFO) << "IN AddImageSkiaRepFromJPEG DO NOT have bitmap";
    return false;
  }
  LOG(INFO) << "IN AddImageSkiaRepFromJPEG Have bitmap";

  // `JPEGCodec::Decode()` doesn't tell `SkBitmap` instance it creates
  // that all of its pixels are opaque, that's why the bitmap gets
  // an alpha type `kPremul_SkAlphaType` instead of `kOpaque_SkAlphaType`.
  // Let's fix it here.
  // TODO(alexeykuzmin): This workaround should be removed
  // when the `JPEGCodec::Decode()` code is fixed.
  // See https://github.com/electron/electron/issues/11294.
  bitmap->setAlphaType(SkAlphaType::kOpaque_SkAlphaType);
  LOG(INFO) << "IN AddImageSkiaRepFromJPEG just set alphatype";

  image->AddRepresentation(gfx::ImageSkiaRep(*bitmap, scale_factor));
  LOG(INFO) << "IN AddImageSkiaRepFromJPEG just did image->AddRepresentation";
  return true;
}

bool AddImageSkiaRepFromBuffer(gfx::ImageSkia* image,
                               const unsigned char* data,
                               size_t size,
                               int width,
                               int height,
                               double scale_factor) {
  LOG(INFO) << "IN AddImageSkiaRepFromBuffer, try PNG";
  // Try PNG first.
  if (AddImageSkiaRepFromPNG(image, data, size, scale_factor)) {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, PNG worked";
    return true;
  }

  LOG(INFO) << "IN AddImageSkiaRepFromBuffer, PNG did not work; try JPEG";
  // Try JPEG second.
  if (AddImageSkiaRepFromJPEG(image, data, size, scale_factor)) {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, JPEG worked";
    return true;
  }
  LOG(INFO)
      << "IN AddImageSkiaRepFromBuffer, JPEG did not work try the other way";
  if (width == 0 || height == 0) {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, returning false because width "
              << width << " or height " << height << " are 0.";
    return false;
  }

  LOG(INFO) << "IN AddImageSkiaRepFromBuffer, about to SkImageInfo::MakeN32";
  auto info = SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType);
  LOG(INFO) << "IN AddImageSkiaRepFromBuffer, about to computeMinByteSize";
  if (size < info.computeMinByteSize()) {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, size < info.computeMinByteSize";
    return false;
  }
  LOG(INFO) << "IN AddImageSkiaRepFromBuffer, size is ok";

  SkBitmap bitmap;
  LOG(INFO) << "IN AddImageSkiaRepFromBuffer, about to call "
               "bitmap.allocN32Pixels with width:"
            << width << " and height " << height << ".";
  bitmap.allocN32Pixels(width, height, false);
  LOG(INFO) << "IN AddImageSkiaRepFromBuffer, successfully called "
               "bitmap.allocN32Pixels with width:"
            << width << " and height " << height << ".";
  bool didWritePixels = bitmap.writePixels({info, data, bitmap.rowBytes()});
  if (didWritePixels) {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, successfully called "
                 "bitmap.didWritePixels.";
  } else {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, FAILED calling "
                 "bitmap.didWritePixels.";
  }

  gfx::ImageSkiaRep imgSkiaRep = gfx::ImageSkiaRep(bitmap, scale_factor);
  if (imgSkiaRep.is_null()) {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, image_rep.is_null UH OH.";
  } else {
    LOG(INFO) << "IN AddImageSkiaRepFromBuffer, image_rep.is NOT null YES.";
  }
  image->AddRepresentation(imgSkiaRep);
  return true;
}

bool AddImageSkiaRepFromPath(gfx::ImageSkia* image,
                             const base::FilePath& path,
                             double scale_factor) {
  std::string file_contents;
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!asar::ReadFileToString(path, &file_contents))
      return false;
  }

  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());
  size_t size = file_contents.size();

  return AddImageSkiaRepFromBuffer(image, data, size, 0, 0, scale_factor);
}

bool PopulateImageSkiaRepsFromPath(gfx::ImageSkia* image,
                                   const base::FilePath& path) {
  bool succeed = false;
  std::string filename(path.BaseName().RemoveExtension().AsUTF8Unsafe());
  if (base::MatchPattern(filename, "*@*x"))
    // Don't search for other representations if the DPI has been specified.
    return AddImageSkiaRepFromPath(image, path, GetScaleFactorFromPath(path));
  else
    succeed |= AddImageSkiaRepFromPath(image, path, 1.0f);

  for (const ScaleFactorPair& pair : kScaleFactorPairs)
    succeed |= AddImageSkiaRepFromPath(
        image, path.InsertBeforeExtensionASCII(pair.name), pair.scale);
  return succeed;
}
#if defined(OS_WIN)
bool ReadImageSkiaFromICO(gfx::ImageSkia* image, HICON icon) {
  // Convert the icon from the Windows specific HICON to gfx::ImageSkia.
  SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(icon);
  if (bitmap.isNull())
    return false;

  image->AddRepresentation(gfx::ImageSkiaRep(bitmap, 1.0f));
  return true;
}
#endif

}  // namespace util

}  // namespace electron
