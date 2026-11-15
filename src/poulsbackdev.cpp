// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 DoubleDensity <doubledense@gmail.com>
#include "config.h"
#include "poulsbackdev.h"
#include "fbterm.h"
#include "poulsback/poulsback_bo_uapi.h"
#include "poulsback/poulsback_exec_uapi.h"
#include <xf86drm.h>
#include <drm_fourcc.h>
#include <linux/dma-buf.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

PoulsbackDev::PoulsbackDev() : mFd(-1), mDmaFd(-1), mOldKdMode(KD_TEXT),
	mHandle(0), mFramebuffer(0), mContext(0), mConnector(0), mDpms(0),
	mSize(0), mCopies(0), mSaved(0), mActive(false), mFailed(false), mBlanked(false)
{
}

Screen *PoulsbackDev::create()
{
	PoulsbackDev *screen = new PoulsbackDev();
	if (screen->initialize()) return screen;
	delete screen;
	return 0;
}

bool PoulsbackDev::fail(const char *operation)
{
	if (!mFailed)
		fprintf(stderr, "Poulsback: %s: %s. Drawing stopped; no GPU reset or fallback.\n",
			operation, strerror(errno));
	mFailed = true;
	if (mActive) FbTerm::instance()->exit();
	return false;
}

bool PoulsbackDev::initialize()
{
	struct vt_stat vt;
	struct stat tty;
	if (ioctl(0, VT_GETSTATE, &vt) || fstat(0, &tty) ||
	    !S_ISCHR(tty.st_mode) || major(tty.st_rdev) != 4 ||
	    !minor(tty.st_rdev) || minor(tty.st_rdev) > 63) {
		fprintf(stderr, "Poulsback requires a Linux virtual console.\n");
		return false;
	}
	// Background login services must wait before opening the display device.
	if (minor(tty.st_rdev) != vt.v_active) {
		int ret;
		do { ret = ioctl(0, VT_WAITACTIVE, minor(tty.st_rdev)); } while (ret && errno == EINTR);
		if (ret) return fail("wait for console activation");
	}
	if (ioctl(0, VT_GETSTATE, &vt) || minor(tty.st_rdev) != vt.v_active ||
	    ioctl(0, KDGETMODE, &mOldKdMode)) {
		fprintf(stderr, "Poulsback console is no longer active.\n");
		return false;
	}
	if (mRotateType != Rotate0 || getenv("FBTERM_BACKGROUND_IMAGE")) {
		fprintf(stderr, "Poulsback currently requires an unrotated screen without a background image.\n");
		return false;
	}
	mFd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (mFd < 0) return fail("open DRM device");
	// Opening the first DRM file can grant master before this VT owns the screen.
	if (drmIsMaster(mFd) && drmDropMaster(mFd)) return fail("release initial DRM master");
	drmVersion *version = drmGetVersion(mFd);
	bool matched = version && version->name && !strcmp(version->name, "poulsback");
	if (version) drmFreeVersion(version);
	if (!matched) { errno = ENODEV; return fail("driver identity"); }

	struct poulsback_display_info info = {};
	info.abi = POULSBACK_BO_ABI;
	if (drmIoctl(mFd, DRM_IOCTL_PB_DISPLAY_INFO, &info)) return fail("inherited display geometry");
	if (!info.width || !info.height || info.width > POULSBACK_2D_MAX_WIDTH_32BPP ||
	    info.height > POULSBACK_2D_MAX_DIM || info.format != DRM_FORMAT_XRGB8888 ||
	    info.stride % POULSBACK_2D_STRIDE_ALIGN || info.stride < (u64)info.width * 4 ||
	    info.stride > POULSBACK_2D_MAX_STRIDE || (u64)info.stride * info.height > 16 * 1024 * 1024) {
		errno = EOPNOTSUPP; return fail("display layout");
	}
	drmModeRes *resources = drmModeGetResources(mFd);
	if (!resources) return fail("display resources");
	unsigned active = 0;
	for (int i = 0; i < resources->count_connectors; ++i) {
		drmModeConnector *connector = drmModeGetConnector(mFd, resources->connectors[i]);
		if (!connector) { drmModeFreeResources(resources); return fail("connector"); }
		drmModeEncoder *encoder = connector->encoder_id ? drmModeGetEncoder(mFd, connector->encoder_id) : 0;
		if (connector->encoder_id && !encoder) {
			drmModeFreeConnector(connector); drmModeFreeResources(resources); return fail("encoder");
		}
		drmModeCrtc *crtc = encoder && encoder->crtc_id ? drmModeGetCrtc(mFd, encoder->crtc_id) : 0;
		if (encoder && encoder->crtc_id && !crtc) {
			drmModeFreeEncoder(encoder); drmModeFreeConnector(connector);
			drmModeFreeResources(resources); return fail("CRTC");
		}
		if (crtc && crtc->mode_valid && crtc->buffer_id) {
			++active;
			if (!mSaved && connector->connection == DRM_MODE_CONNECTED &&
			    connector->connector_type == DRM_MODE_CONNECTOR_LVDS && !crtc->x && !crtc->y &&
			    crtc->mode.hdisplay == info.width && crtc->mode.vdisplay == info.height) {
				mSaved = crtc; crtc = 0; mConnector = connector->connector_id;
				for (int j = 0; j < connector->count_props; ++j) {
					drmModePropertyRes *property = drmModeGetProperty(mFd, connector->props[j]);
					if (property && !strcmp(property->name, "DPMS")) mDpms = property->prop_id;
					if (property) drmModeFreeProperty(property);
				}
			}
		}
		if (crtc) drmModeFreeCrtc(crtc);
		if (encoder) drmModeFreeEncoder(encoder);
		drmModeFreeConnector(connector);
	}
	drmModeFreeResources(resources);
	if (active != 1 || !mSaved) { errno = EOPNOTSUPP; return fail("single inherited LVDS output"); }
	mWidth = info.width; mHeight = info.height; mBytesPerLine = info.stride; mBitsPerPixel = 32;
	struct poulsback_bo_create create = {};
	create.abi = POULSBACK_BO_ABI;
	create.flags = POULSBACK_BO_SCANOUT | POULSBACK_BO_CPU_WC;
	create.bo_class = POULSBACK_BO_CLASS_2D;
	create.size = (u64)mBytesPerLine * mHeight;
	if (drmIoctl(mFd, DRM_IOCTL_PB_BO_CREATE, &create)) return fail("scanout buffer");
	mHandle = create.handle; mSize = create.size;
	mVMemBase = (u8 *)mmap(0, mSize, PROT_READ | PROT_WRITE, MAP_SHARED, mFd, (off_t)create.mmap_offset);
	if (mVMemBase == MAP_FAILED) { mVMemBase = 0; return fail("scanout mapping"); }
	memset(mVMemBase, 0, mSize);
	if (drmPrimeHandleToFD(mFd, mHandle, DRM_CLOEXEC | DRM_RDWR, &mDmaFd))
		return fail("buffer synchronization handle");
	return true;
}

bool PoulsbackDev::handoff(bool enter)
{
	if (mFailed) return false;
	if (enter == mActive) return true;
	if (enter) {
		if (drmSetMaster(mFd)) return fail("acquire DRM master");
		// Read the current framebuffer again; another VT may have been visible at startup.
		drmModeCrtc *saved = drmModeGetCrtc(mFd, mSaved->crtc_id);
		if (!saved) return fail("current inherited framebuffer");
		if (!saved->mode_valid || !saved->buffer_id || saved->x || saved->y ||
		    saved->mode.hdisplay != mWidth || saved->mode.vdisplay != mHeight) {
			drmModeFreeCrtc(saved); errno = EOPNOTSUPP; return fail("changed display mode");
		}
		drmModeFreeCrtc(mSaved); mSaved = saved;
		u32 handles[4] = {mHandle}, pitches[4] = {mBytesPerLine}, offsets[4] = {0};
		if (drmModeAddFB2(mFd, mWidth, mHeight, DRM_FORMAT_XRGB8888, handles, pitches, offsets, &mFramebuffer, 0))
			return fail("typed framebuffer");
		if (drmModeSetCrtc(mFd, mSaved->crtc_id, mFramebuffer, 0, 0, &mConnector, 1, &mSaved->mode))
			return fail("native scanout handoff");
		mActive = true;
		// Wait for scanout to succeed before switching the text console to graphics mode.
		if (ioctl(0, KDSETMODE, KD_GRAPHICS)) return fail("graphics console mode");
		fprintf(stderr, "Poulsback: native %ux%u stride %u; GPU scrolling enabled.\n", mWidth, mHeight, mBytesPerLine);
	} else {
		if (mBlanked) blank(false);
		if (mFailed) return false;
		if (drmModeSetCrtc(mFd, mSaved->crtc_id, mSaved->buffer_id, mSaved->x, mSaved->y,
			&mConnector, 1, &mSaved->mode)) return fail("restore inherited framebuffer");
		// Inactive VTs keep their pixels but release the limited scanout slots.
		if (drmModeRmFB(mFd, mFramebuffer)) return fail("release inactive framebuffer");
		mFramebuffer = 0;
		mActive = false;
		if (ioctl(0, KDSETMODE, mOldKdMode)) return fail("restore console mode");
		if (drmDropMaster(mFd)) return fail("release DRM master");
	}
	return true;
}

bool PoulsbackDev::copyRect(u32 sx, u32 sy, u32 dx, u32 dy, u32 w, u32 h)
{
	// Report the copy as handled after failure to avoid an unsynchronized CPU redraw.
	if (!drawingReady() || !w || !h) return true;
	if (sx >= mWidth || dx >= mWidth || sy >= mHeight || dy >= mHeight ||
	    w > mWidth - sx || w > mWidth - dx || h > mHeight - sy || h > mHeight - dy) {
		errno = EINVAL; fail("scroll bounds"); return true;
	}
	if (!mContext) {
		struct poulsback_context_create create = {};
		create.abi = POULSBACK_EXEC_ABI;
		if (drmIoctl(mFd, DRM_IOCTL_PB_CONTEXT_CREATE, &create)) { fail("2D engine context"); return true; }
		mContext = create.handle;
	}
	struct poulsback_submit_bo bo = {};
	bo.handle = mHandle;
	struct poulsback_2d_surface surface = {};
	surface.stride = mBytesPerLine; surface.width = mWidth; surface.height = mHeight;
	surface.format = POULSBACK_2D_XRGB8888;
	struct poulsback_2d_op op = {};
	op.kind = POULSBACK_2D_COPY; op.rop = 3; // GXcopy. The driver handles overlap.
	op.src_x = sx; op.src_y = sy;
	op.rect.x = dx; op.rect.y = dy; op.rect.width = w; op.rect.height = h;
	struct poulsback_submit_2d submit = {};
	submit.abi = POULSBACK_EXEC_ABI; submit.context = mContext;
	submit.bo_count = submit.surface_count = submit.op_count = 1;
	submit.bos = (uintptr_t)&bo; submit.surfaces = (uintptr_t)&surface; submit.ops = (uintptr_t)&op;
	__sync_synchronize(); // Publish CPU glyph writes before the GPU reads WC memory.
	if (drmIoctl(mFd, DRM_IOCTL_PB_SUBMIT_2D, &submit)) { fail("2D scroll submission"); return true; }
	struct dma_buf_sync sync = {};
	sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
	int ret;
	do { ret = ioctl(mDmaFd, DMA_BUF_IOCTL_SYNC, &sync); } while (ret && (errno == EINTR || errno == EAGAIN));
	if (ret) { fail("wait for scroll before CPU drawing"); return true; }
	++mCopies;
	return true;
}

void PoulsbackDev::blank(bool on)
{
	if (!drawingReady() || on == mBlanked) return;
	if (!mDpms) { fprintf(stderr, "Poulsback: connector has no DPMS property; blanking unavailable.\n"); return; }
	if (drmModeConnectorSetProperty(mFd, mConnector, mDpms, on ? DRM_MODE_DPMS_OFF : DRM_MODE_DPMS_ON)) {
		fail("panel blanking"); return;
	}
	mBlanked = on;
}

PoulsbackDev::~PoulsbackDev()
{
	if (mActive && !mFailed) handoff(false);
	if (!mFailed && mFramebuffer) drmModeRmFB(mFd, mFramebuffer);
	if (mVMemBase) munmap(mVMemBase, mSize);
	if (mDmaFd >= 0) close(mDmaFd);
	// The driver retains buffers on failure. Do not reset or unload it.
	if (mFd >= 0) close(mFd);
	if (mSaved) drmModeFreeCrtc(mSaved);
	fprintf(stderr, "Poulsback: %llu synchronized GPU scroll copies; %s.\n", mCopies,
		mFailed ? "failure retained; inspect kernel log before further GPU use" : "normal shutdown");
}
