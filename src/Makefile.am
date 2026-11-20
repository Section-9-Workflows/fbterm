SUBDIRS = lib

bin_PROGRAMS = fbterm

fbterm_SOURCES = fbconfig.cpp fbconfig.h fbio.cpp fbio.h fbshell.cpp fbshell.h fbshellman.cpp fbshellman.h fbterm.cpp \
	fbterm.h font.cpp font.h input.cpp input.h input_key.h mouse.cpp mouse.h screen.cpp screen.h improxy.cpp improxy.h \
	screen_render.cpp screen_pixmap.cpp mouse_mousedev.cpp fbdev.cpp fbdev.h vesadev.cpp vesadev.h vbe.h
EXTRA_fbterm_SOURCES = signalfd.h

fbterm_CXXFLAGS = -fno-exceptions -fno-rtti -Ilib @FT2_CFLAGS@ @FC_CFLAGS@ @DRM_CFLAGS@ @PNG_CFLAGS@
fbterm_LDADD = lib/libshell.a @FT2_LIBS@ @FC_LIBS@ @X86_LIBS@ @DRM_LIBS@ @PNG_LIBS@ -lutil

if POULSBACK
fbterm_SOURCES += poulsbackdev.cpp poulsbackdev.h poulsback/poulsback_bo_uapi.h poulsback/poulsback_exec_uapi.h
endif
