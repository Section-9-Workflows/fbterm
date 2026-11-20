// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 DoubleDensity <doubledense@gmail.com>

#include <errno.h>
#include <fcntl.h>
#include <linux/keyboard.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include "mouse.h"
#include "font.h"
#include "screen.h"
#include "fbshell.h"
#include "fbshellman.h"

static bool mouseCommand(s32 fd, u8 command, u8 *reply, u32 size)
{
	ssize_t result;
	do result = ::write(fd, &command, 1); while (result < 0 && errno == EINTR);
	if (result != 1) return false;
	for (u32 read = 0; read < size;) {
		result = ::read(fd, reply + read, size - read);
		if (result < 0 && errno == EINTR) continue;
		if (result <= 0) return false;
		read += result;
	}
	return reply[0] == 0xfa;
}

void Mouse::switchMousedev(bool enter)
{
	setFd(-1);
	mPacketLen = 0;
	mButtons = 0;
	mLastClick = 0;
	if (!enter) return;

	// GPM withholds client events in KD_GRAPHICS. Each mousedev open has its own queue.
	s32 mouse = open("/dev/input/mice", O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (mouse == -1) {
		perror("Mouse: open /dev/input/mice");
		return;
	}

	// Negotiate wheel packets on this file descriptor, without changing physical devices.
	const u8 sequence[] = { 0xf3, 200, 0xf3, 100, 0xf3, 80 };
	u8 reply[2];
	bool ok = true;
	for (u32 i = 0; ok && i < sizeof(sequence); i++)
		ok = mouseCommand(mouse, sequence[i], reply, 1);
	if (!ok || !mouseCommand(mouse, 0xf2, reply, sizeof(reply)) || reply[1] != 3) {
		fprintf(stderr, "Mouse: /dev/input/mice did not accept wheel protocol; mouse disabled on this VT.\n");
		close(mouse);
		return;
	}
	setFd(mouse);
}

void Mouse::ioError(bool read, s32 err)
{
	fprintf(stderr, "Mouse: input closed (error %d).\n", err);
	setFd(-1);
	mPacketLen = 0;
}

void Mouse::readMousedev(s8 *buf, u32 len)
{
	for (u32 i = 0; i < len; i++) {
		u8 byte = (u8)buf[i];
		if (!mPacketLen && !(byte & 0x08)) continue;
		mPacket[mPacketLen++] = byte;
		if (mPacketLen == sizeof(mPacket)) {
			mPacketLen = 0;
			mousedevPacket();
		}
	}
}

void Mouse::mousedevPacket()
{
	FbShell *shell = FbShellManager::instance()->activeShell();
	if (!shell) return;
	Screen *screen = Screen::instance();
	s32 dx = mPacket[1] - ((mPacket[0] & 0x10) ? 256 : 0);
	s32 dy = mPacket[2] - ((mPacket[0] & 0x20) ? 256 : 0);
	mPixelX += dx;
	mPixelY -= dy;
	if (mPixelX < 0) mPixelX = 0;
	if (mPixelY < 0) mPixelY = 0;
	if (mPixelX >= (s32)FW(screen->cols())) mPixelX = FW(screen->cols()) - 1;
	if (mPixelY >= (s32)FH(screen->rows())) mPixelY = FH(screen->rows()) - 1;
	u16 newx = mPixelX / FW(1), newy = mPixelY / FH(1);

	u8 shift = 6;
	s32 modifiers = 0;
	if (!ioctl(STDIN_FILENO, TIOCLINUX, &shift)) {
		if (shift & (1 << KG_SHIFT)) modifiers |= ShiftButton;
		if (shift & (1 << KG_CTRL)) modifiers |= ControlButton;
		if (shift & ((1 << KG_ALT) | (1 << KG_ALTGR))) modifiers |= AltButton;
	}
	s32 buttons = 0;
	if (mPacket[0] & 1) buttons |= LeftButton;
	if (mPacket[0] & 2) buttons |= RightButton;
	if (mPacket[0] & 4) buttons |= MidButton;
	if (newx != x || newy != y) {
		mLastClick = 0;
		shell->mouseInput(newx, newy, Move, mButtons | modifiers);
	}
	x = newx;
	y = newy;
	for (s32 button = LeftButton; button <= MidButton; button <<= 1) {
		if ((mButtons & button) && !(buttons & button))
			shell->mouseInput(x, y, Release, button | modifiers);
		if (!(mButtons & button) && (buttons & button)) {
			s32 type = Press;
			if (button == LeftButton) {
				struct timespec now;
				if (!clock_gettime(CLOCK_MONOTONIC, &now)) {
					u64 ms = (u64)now.tv_sec * 1000 + now.tv_nsec / 1000000;
					if (mLastClick && ms - mLastClick <= 250) type = DblClick;
					mLastClick = type == DblClick ? 0 : ms;
				}
			}
			shell->mouseInput(x, y, type, button | modifiers);
		}
	}
	mButtons = buttons;
	s32 wheel = (signed char)mPacket[3];
	while (wheel) {
		shell->mouseInput(x, y, Wheel, modifiers | (wheel > 0 ? WheelDown : WheelUp));
		wheel += wheel > 0 ? -1 : 1;
	}
}
