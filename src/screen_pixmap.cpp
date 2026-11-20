// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 DoubleDensity <doubledense@gmail.com>
#include <stdio.h>
#include <string.h>
#include <new>
#include <png.h>
#include "screen.h"
#include "fbconfig.h"
#include "font.h"

void Screen::loadStartupPixmap()
{
	char path[4096];
	Config::instance()->getOption("startup-pixmap", path, sizeof(path));
	if (!*path) return;
	if (mBitsPerPixel != 32 || mRotateType != Rotate0) {
		fprintf(stderr, "Startup pixmap requires an unrotated 32-bit display.\n");
		return;
	}
	mPixmapX = FW(2);
	mPixmapY = FH(3);
	Config::instance()->getOption("startup-pixmap-x", mPixmapX);
	Config::instance()->getOption("startup-pixmap-y", mPixmapY);
	if (mPixmapX >= mWidth || mPixmapY >= mHeight) {
		fprintf(stderr, "Startup pixmap position is outside the display.\n");
		return;
	}
	png_image image = {};
	image.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&image, path)) {
		fprintf(stderr, "Cannot read startup PNG: %s\n", image.message);
		png_image_free(&image);
		return;
	}
	if (!image.width || !image.height || image.width > mWidth - mPixmapX ||
	    image.height > mHeight - mPixmapY ||
	    (u64)image.width * image.height * 4 > (size_t)-1) {
		fprintf(stderr, "Startup pixmap does not fit the display.\n");
		png_image_free(&image);
		return;
	}
	image.format = PNG_FORMAT_BGRA;
	u8 *pixels = new(std::nothrow) u8[(size_t)image.width * image.height * 4];
	if (!pixels || !png_image_finish_read(&image, 0, pixels, 0, 0)) {
		fprintf(stderr, "Cannot decode startup PNG: %s\n", pixels ? image.message : "out of memory");
		delete[] pixels;
		png_image_free(&image);
		return;
	}
	// Flatten alpha onto black once, so repeated exposure never darkens the image.
	for (size_t i = 0; i < (size_t)image.width * image.height * 4; i += 4) {
		for (unsigned channel = 0; channel < 3; channel++)
			pixels[i + channel] = (pixels[i + channel] * pixels[i + 3] + 127) / 255;
		pixels[i + 3] = 0;
	}
	mPixmapWidth = image.width;
	mPixmapHeight = image.height;
	mStartupPixmap = pixels;
	png_image_free(&image);
}

void Screen::drawStartupPixmap(u32 x, u32 y, u32 w, u32 h)
{
	if (!mStartupPixmap || !drawingReady() || x >= mWidth || y >= mHeight) return;
	u32 right = x + (w < mWidth - x ? w : mWidth - x);
	u32 bottom = y + (h < mHeight - y ? h : mHeight - y);
	if (x < mPixmapX) x = mPixmapX;
	if (y < mPixmapY) y = mPixmapY;
	if (right > mPixmapX + mPixmapWidth) right = mPixmapX + mPixmapWidth;
	if (bottom > mPixmapY + mPixmapHeight) bottom = mPixmapY + mPixmapHeight;
	if (x >= right || y >= bottom) return;
	for (; y < bottom; y++) {
		u8 *source = mStartupPixmap + ((size_t)(y - mPixmapY) * mPixmapWidth + x - mPixmapX) * 4;
		memcpy(mVMemBase + (size_t)y * mBytesPerLine + x * 4, source, (right - x) * 4);
	}
}

bool Screen::dismissStartupPixmap()
{
	if (!mStartupPixmap) return false;
	delete[] mStartupPixmap;
	mStartupPixmap = 0;
	return true;
}
