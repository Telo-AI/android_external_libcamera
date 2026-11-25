/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2020, Google Inc.
 *
 * Simple image thumbnailer
 */

#include "thumbnailer.h"

#include <libyuv/convert_from_argb.h>
#include <libyuv/scale_argb.h>

#include <libcamera/base/log.h>

#include <libcamera/formats.h>

#include "libcamera/internal/mapped_framebuffer.h"

using namespace libcamera;

LOG_DEFINE_CATEGORY(Thumbnailer)

namespace {

/* Crop to the thumbnail's aspect ratio, then scale and convert to NV12. */
void thumbnailFromXRGB8888(const uint8_t *source, unsigned int stride,
			   const Size &sourceSize, const Size &targetSize,
			   std::vector<unsigned char> *destination)
{
	const Size crop = sourceSize.boundedToAspectRatio(targetSize);
	const Point topLeft = crop.centeredTo(Rectangle(sourceSize).center()).topLeft();
	const unsigned int tw = targetSize.width;
	const unsigned int th = targetSize.height;
	std::vector<uint8_t> scaled(tw * th * 4);

	int ret = libyuv::ARGBScale(source + topLeft.y * stride + topLeft.x * 4, stride,
				    crop.width, crop.height, scaled.data(), tw * 4,
				    tw, th, libyuv::FilterMode::kFilterBox);
	if (ret)
		return;

	destination->resize(tw * th + (th / 2) * tw);
	ret = libyuv::ARGBToNV12(scaled.data(), tw * 4, destination->data(), tw,
				 destination->data() + tw * th, tw, tw, th);
	if (ret)
		destination->clear();
}

} /* namespace */

Thumbnailer::Thumbnailer()
	: valid_(false)
{
}

void Thumbnailer::configure(const Size &sourceSize, PixelFormat pixelFormat,
			    unsigned int stride)
{
	sourceSize_ = sourceSize;
	pixelFormat_ = pixelFormat;
	stride_ = stride;

	if (pixelFormat_ != formats::NV12 && pixelFormat_ != formats::XRGB8888) {
		LOG(Thumbnailer, Error)
			<< "Failed to configure: Pixel Format "
			<< pixelFormat_ << " unsupported.";
		return;
	}

	valid_ = true;
}

void Thumbnailer::createThumbnail(const FrameBuffer &source,
				  const Size &targetSize,
				  std::vector<unsigned char> *destination)
{
	MappedFrameBuffer frame(&source, MappedFrameBuffer::MapFlag::Read);
	if (!frame.isValid()) {
		LOG(Thumbnailer, Error)
			<< "Failed to map FrameBuffer : "
			<< strerror(frame.error());
		return;
	}

	if (!valid_) {
		LOG(Thumbnailer, Error) << "Config is unconfigured or invalid.";
		return;
	}

	if (pixelFormat_ == formats::XRGB8888) {
		thumbnailFromXRGB8888(frame.planes()[0].data(), stride_, sourceSize_,
				      targetSize, destination);
		return;
	}

	const unsigned int sw = sourceSize_.width;
	const unsigned int sh = sourceSize_.height;
	const unsigned int tw = targetSize.width;
	const unsigned int th = targetSize.height;

	ASSERT(frame.planes().size() == 2);
	ASSERT(tw % 2 == 0 && th % 2 == 0);

	/* Image scaling block implementing nearest-neighbour algorithm. */
	unsigned char *src = frame.planes()[0].data();
	unsigned char *srcC = frame.planes()[1].data();
	unsigned char *srcCb, *srcCr;
	unsigned char *dstY, *srcY;

	size_t dstSize = (th * tw) + ((th / 2) * tw);
	destination->resize(dstSize);
	unsigned char *dst = destination->data();
	unsigned char *dstC = dst + th * tw;

	for (unsigned int y = 0; y < th; y += 2) {
		unsigned int sourceY = (sh * y + th / 2) / th;

		dstY = dst + y * tw;
		srcY = src + sw * sourceY;
		srcCb = srcC + (sourceY / 2) * sw + 0;
		srcCr = srcC + (sourceY / 2) * sw + 1;

		for (unsigned int x = 0; x < tw; x += 2) {
			unsigned int sourceX = (sw * x + tw / 2) / tw;

			dstY[x] = srcY[sourceX];
			dstY[tw + x] = srcY[sw + sourceX];
			dstY[x + 1] = srcY[sourceX + 1];
			dstY[tw + x + 1] = srcY[sw + sourceX + 1];

			dstC[(y / 2) * tw + x + 0] = srcCb[(sourceX / 2) * 2];
			dstC[(y / 2) * tw + x + 1] = srcCr[(sourceX / 2) * 2];
		}
	}
}
