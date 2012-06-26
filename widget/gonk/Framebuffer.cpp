/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include "android/log.h"

#include "Framebuffer.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "mozilla/FileUtils.h"
#include "nsTArray.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)

using namespace std;

namespace mozilla {

namespace Framebuffer {

static int sFd = -1;
static size_t sMappedSize;
static struct fb_var_screeninfo sVi;
static size_t sActiveBuffer;
typedef vector<nsRefPtr<gfxImageSurface> > BufferVector;
BufferVector* sBuffers;

BufferVector& Buffers() { return *sBuffers; }

bool
SetGraphicsMode()
{
    ScopedClose fd(open("/dev/tty0", O_RDWR | O_SYNC));
    if (0 > fd.get()) {
        // This is non-fatal; post-Cupcake kernels don't have tty0.
        LOG("No /dev/tty0?");
    } else if (ioctl(fd.get(), KDSETMODE, (void*) KD_GRAPHICS)) {
        LOG("Error setting graphics mode on /dev/tty0");
        return false;
    }
    return true;
}

bool
Open(nsIntSize* aScreenSize)
{
    if (0 <= sFd)
        return true;

    if (!SetGraphicsMode())
        return false;

    ScopedClose fd(open("/dev/graphics/fb0", O_RDWR));
    if (0 > fd.get()) {
        LOG("Error opening framebuffer device");
        return false;
    }

    struct fb_fix_screeninfo fi;
    if (0 > ioctl(fd.get(), FBIOGET_FSCREENINFO, &fi)) {
        LOG("Error getting fixed screeninfo");
        return false;
    }

    if (0 > ioctl(fd.get(), FBIOGET_VSCREENINFO, &sVi)) {
        LOG("Error getting variable screeninfo");
        return false;
    }

    sMappedSize = fi.smem_len;
    void* mem = mmap(0, sMappedSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd.rwget(), 0);
    if (MAP_FAILED == mem) {
        LOG("Error mmap'ing framebuffer");
        return false;
    }

    sFd = fd.get();
    fd.forget();

    // The android porting doc requires a /dev/graphics/fb0 device
    // that's double buffered with r5g6b5 format.  Hence the
    // hard-coded numbers here.
    gfxASurface::gfxImageFormat format = gfxASurface::ImageFormatRGB16_565;
    int bytesPerPixel = gfxASurface::BytePerPixelFromFormat(format);
    gfxIntSize size(sVi.xres, sVi.yres);
    long stride = size.width * bytesPerPixel;
    size_t numFrameBytes = stride * size.height;

    sBuffers = new BufferVector(2);
    unsigned char* data = static_cast<unsigned char*>(mem);
    for (size_t i = 0; i < 2; ++i, data += numFrameBytes) {
      memset(data, 0, numFrameBytes);
      Buffers()[i] = new gfxImageSurface(data, size, stride, format);
    }

    // Clear the framebuffer to a known state.
    Present(nsIntRect());

    *aScreenSize = size;
    return true;
}

bool
GetSize(nsIntSize *aScreenSize) {
    if (0 <= sFd)
        return true;

    ScopedClose fd(open("/dev/graphics/fb0", O_RDWR));
    if (0 > fd.get()) {
        LOG("Error opening framebuffer device");
        return false;
    }

    if (0 > ioctl(fd.get(), FBIOGET_VSCREENINFO, &sVi)) {
        LOG("Error getting variable screeninfo");
        return false;
    }

    *aScreenSize = gfxIntSize(sVi.xres, sVi.yres);
    return true;
}

void
Close()
{
    if (0 > sFd)
        return;

    munmap(Buffers()[0]->Data(), sMappedSize);
    delete sBuffers;
    sBuffers = NULL;

    close(sFd);
    sFd = -1;
}

gfxASurface*
BackBuffer()
{
    return Buffers()[!sActiveBuffer];
}

static gfxASurface*
FrontBuffer()
{
    return Buffers()[sActiveBuffer];
}

void
Present(const nsIntRegion& aUpdated)
{
    sActiveBuffer = !sActiveBuffer;

    sVi.yres_virtual = sVi.yres * 2;
    sVi.yoffset = sActiveBuffer * sVi.yres;
    sVi.bits_per_pixel = 16;
    if (ioctl(sFd, FBIOPUT_VSCREENINFO, &sVi) < 0) {
        LOG("Error presenting front buffer");
    }

    nsRefPtr<gfxContext> ctx = new gfxContext(BackBuffer());
    gfxUtils::PathFromRegion(ctx, aUpdated);
    ctx->Clip();
    ctx->SetSource(FrontBuffer());
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->Paint(1.0);
}

} // namespace Framebuffer

} // namespace mozilla
