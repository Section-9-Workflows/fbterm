// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 DoubleDensity <doubledense@gmail.com>
#ifndef POULSBACKDEV_H
#define POULSBACKDEV_H
#include "screen.h"
#include <xf86drmMode.h>

class PoulsbackDev : public Screen {
public:
	static Screen *create();
	virtual ~PoulsbackDev();
	virtual bool handoff(bool enter);
	virtual void blank(bool on);
protected:
	virtual bool drawingReady() { return mActive && !mFailed; }
	virtual bool copyRect(u32 sx, u32 sy, u32 dx, u32 dy, u32 w, u32 h);
private:
	PoulsbackDev();
	bool initialize();
	bool fail(const char *operation);
	virtual const s8 *drvId() { return "poulsback DRM/2D"; }
	int mFd, mDmaFd, mOldKdMode;
	u32 mHandle, mFramebuffer, mContext, mConnector, mDpms;
	u64 mSize, mCopies;
	drmModeCrtc *mSaved;
	bool mActive, mFailed, mBlanked;
};
#endif
