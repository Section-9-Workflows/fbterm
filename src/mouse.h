/*
 *   Copyright © 2008-2010 dragchan <zgchan317@gmail.com>
 *   This file is part of FbTerm.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef MOUSE_H
#define MOUSE_H

#include "io.h"
#include "instance.h"

class Mouse : public IoPipe {
	DECLARE_INSTANCE(Mouse)
public:
	void switchVc(bool enter);
private:
	virtual void readyRead(s8 *buf, u32 len);
	virtual void ioError(bool read, s32 err);
	void switchMousedev(bool enter);
	void readMousedev(s8 *buf, u32 len);
	void mousedevPacket();

	u16 x, y;
	bool mDirect;
	u8 mPacket[4];
	u32 mPacketLen;
	s32 mPixelX, mPixelY, mButtons;
	u64 mLastClick;
};

#endif
