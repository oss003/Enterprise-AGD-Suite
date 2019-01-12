
// ep128emu -- portable Enterprise 128 emulator
// Copyright (C) 2003-2016 Istvan Varga <istvanv@users.sourceforge.net>
// https://sourceforge.net/projects/ep128emu/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "ep128emu.hpp"
#include "system.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include "fldisp.hpp"

static int defaultFLTKEventCallback(void *userData, int event)
{
  (void) userData;
  (void) event;
  return 0;
}

namespace Ep128Emu {

  void FLTKDisplay_::decodeLine(unsigned char *outBuf,
                                const unsigned char *inBuf, size_t nBytes)
  {
    const unsigned char *bufp = inBuf;
    unsigned char *endp = outBuf + 768;
    do {
      switch (bufp[0]) {
      case 0x00:                        // blank
        do {
          outBuf[15] = outBuf[14] =
          outBuf[13] = outBuf[12] =
          outBuf[11] = outBuf[10] =
          outBuf[ 9] = outBuf[ 8] =
          outBuf[ 7] = outBuf[ 6] =
          outBuf[ 5] = outBuf[ 4] =
          outBuf[ 3] = outBuf[ 2] =
          outBuf[ 1] = outBuf[ 0] = 0x00;
          outBuf = outBuf + 16;
          bufp = bufp + 1;
          if (outBuf >= endp)
            break;
        } while (bufp[0] == 0x00);
        break;
      case 0x01:                        // 1 pixel, 256 colors
        do {
          outBuf[15] = outBuf[14] =
          outBuf[13] = outBuf[12] =
          outBuf[11] = outBuf[10] =
          outBuf[ 9] = outBuf[ 8] =
          outBuf[ 7] = outBuf[ 6] =
          outBuf[ 5] = outBuf[ 4] =
          outBuf[ 3] = outBuf[ 2] =
          outBuf[ 1] = outBuf[ 0] = bufp[1];
          outBuf = outBuf + 16;
          bufp = bufp + 2;
          if (outBuf >= endp)
            break;
        } while (bufp[0] == 0x01);
        break;
      case 0x02:                        // 2 pixels, 256 colors
        do {
          outBuf[ 7] = outBuf[ 6] =
          outBuf[ 5] = outBuf[ 4] =
          outBuf[ 3] = outBuf[ 2] =
          outBuf[ 1] = outBuf[ 0] = bufp[1];
          outBuf[15] = outBuf[14] =
          outBuf[13] = outBuf[12] =
          outBuf[11] = outBuf[10] =
          outBuf[ 9] = outBuf[ 8] = bufp[2];
          outBuf = outBuf + 16;
          bufp = bufp + 3;
          if (outBuf >= endp)
            break;
        } while (bufp[0] == 0x02);
        break;
      case 0x03:                        // 8 pixels, 2 colors
        do {
          unsigned char c0 = bufp[1];
          unsigned char c1 = bufp[2];
          unsigned char b = bufp[3];
          outBuf[ 1] = outBuf[ 0] = ((b & 128) ? c1 : c0);
          outBuf[ 3] = outBuf[ 2] = ((b &  64) ? c1 : c0);
          outBuf[ 5] = outBuf[ 4] = ((b &  32) ? c1 : c0);
          outBuf[ 7] = outBuf[ 6] = ((b &  16) ? c1 : c0);
          outBuf[ 9] = outBuf[ 8] = ((b &   8) ? c1 : c0);
          outBuf[11] = outBuf[10] = ((b &   4) ? c1 : c0);
          outBuf[13] = outBuf[12] = ((b &   2) ? c1 : c0);
          outBuf[15] = outBuf[14] = ((b &   1) ? c1 : c0);
          outBuf = outBuf + 16;
          bufp = bufp + 4;
          if (outBuf >= endp)
            break;
        } while (bufp[0] == 0x03);
        break;
      case 0x04:                        // 4 pixels, 256 colors
        do {
          outBuf[ 3] = outBuf[ 2] =
          outBuf[ 1] = outBuf[ 0] = bufp[1];
          outBuf[ 7] = outBuf[ 6] =
          outBuf[ 5] = outBuf[ 4] = bufp[2];
          outBuf[11] = outBuf[10] =
          outBuf[ 9] = outBuf[ 8] = bufp[3];
          outBuf[15] = outBuf[14] =
          outBuf[13] = outBuf[12] = bufp[4];
          outBuf = outBuf + 16;
          bufp = bufp + 5;
          if (outBuf >= endp)
            break;
        } while (bufp[0] == 0x04);
        break;
      case 0x06:                        // 16 (2*8) pixels, 2*2 colors
        do {
          unsigned char c0 = bufp[1];
          unsigned char c1 = bufp[2];
          unsigned char b = bufp[3];
          outBuf[ 0] = ((b & 128) ? c1 : c0);
          outBuf[ 1] = ((b &  64) ? c1 : c0);
          outBuf[ 2] = ((b &  32) ? c1 : c0);
          outBuf[ 3] = ((b &  16) ? c1 : c0);
          outBuf[ 4] = ((b &   8) ? c1 : c0);
          outBuf[ 5] = ((b &   4) ? c1 : c0);
          outBuf[ 6] = ((b &   2) ? c1 : c0);
          outBuf[ 7] = ((b &   1) ? c1 : c0);
          c0 = bufp[4];
          c1 = bufp[5];
          b = bufp[6];
          outBuf[ 8] = ((b & 128) ? c1 : c0);
          outBuf[ 9] = ((b &  64) ? c1 : c0);
          outBuf[10] = ((b &  32) ? c1 : c0);
          outBuf[11] = ((b &  16) ? c1 : c0);
          outBuf[12] = ((b &   8) ? c1 : c0);
          outBuf[13] = ((b &   4) ? c1 : c0);
          outBuf[14] = ((b &   2) ? c1 : c0);
          outBuf[15] = ((b &   1) ? c1 : c0);
          outBuf = outBuf + 16;
          bufp = bufp + 7;
          if (outBuf >= endp)
            break;
        } while (bufp[0] == 0x06);
        break;
      case 0x08:                        // 8 pixels, 256 colors
        do {
          outBuf[ 1] = outBuf[ 0] = bufp[1];
          outBuf[ 3] = outBuf[ 2] = bufp[2];
          outBuf[ 5] = outBuf[ 4] = bufp[3];
          outBuf[ 7] = outBuf[ 6] = bufp[4];
          outBuf[ 9] = outBuf[ 8] = bufp[5];
          outBuf[11] = outBuf[10] = bufp[6];
          outBuf[13] = outBuf[12] = bufp[7];
          outBuf[15] = outBuf[14] = bufp[8];
          outBuf = outBuf + 16;
          bufp = bufp + 9;
          if (outBuf >= endp)
            break;
        } while (bufp[0] == 0x08);
        break;
      default:                          // invalid flag byte
        do {
          *(outBuf++) = 0x00;
        } while (outBuf < endp);
        break;
      }
    } while (outBuf < endp);

    (void) nBytes;
#if 0
    if (size_t(bufp - inBuf) != nBytes)
      throw std::exception();
#endif
  }

  // --------------------------------------------------------------------------

  void FLTKDisplay_::Message_LineData::copyLine(const uint8_t *buf,
                                                size_t nBytes)
  {
    nBytes_ = (unsigned int) nBytes;
    if (nBytes_ & 3U)
      buf_[nBytes_ / 4U] = 0U;
    if (nBytes_)
      std::memcpy(&(buf_[0]), buf, nBytes_);
  }

  FLTKDisplay_::Message_LineData&
      FLTKDisplay_::Message_LineData::operator=(const Message_LineData& r)
  {
    std::memcpy(&nBytes_, &(r.nBytes_),
                size_t((const char *) &(r.buf_[((r.nBytes_ + 7U) & (~7U)) / 4U])
                       - (const char *) &(r.nBytes_)));
    return (*this);
  }

  void FLTKDisplay_::deleteMessage(Message *m)
  {
    messageQueueMutex.lock();
    m->nxt = freeMessageStack;
    freeMessageStack = m;
    messageQueueMutex.unlock();
  }

  void FLTKDisplay_::queueMessage(Message *m)
  {
    messageQueueMutex.lock();
    if (exitFlag) {
      messageQueueMutex.unlock();
      std::free(m);
      return;
    }
    m->nxt = (Message *) 0;
    if (lastMessage)
      lastMessage->nxt = m;
    else
      messageQueue = m;
    lastMessage = m;
    bool    isFrameDone = (m->msgType == Message::MsgType_FrameDone);
    messageQueueMutex.unlock();
    if (EP128EMU_UNLIKELY(isFrameDone)) {
      if (!videoResampleEnabled) {
        Fl::awake();
        threadLock.wait(1);
      }
    }
  }

  // --------------------------------------------------------------------------

  FLTKDisplay_::FLTKDisplay_()
    : VideoDisplay(),
      messageQueue((Message *) 0),
      lastMessage((Message *) 0),
      freeMessageStack((Message *) 0),
      messageQueueMutex(),
      lineBuffers((Message_LineData **) 0),
      curLine(0),
      vsyncCnt(0),
      framesPending(0),
      skippingFrame(false),
      framesPendingFlag(false),
      vsyncState(false),
      oddFrame(false),
      videoResampleEnabled(false),
      exitFlag(false),
      limitFrameRateFlag(false),
      displayParameters(),
      savedDisplayParameters(),
      fltkEventCallback(&defaultFLTKEventCallback),
      fltkEventCallbackUserData((void *) 0),
      screenshotCallback((void (*)(void *, const unsigned char *, int, int)) 0),
      screenshotCallbackUserData((void *) 0),
      screenshotCallbackFlag(false)
  {
    try {
      lineBuffers = new Message_LineData*[578];
      for (size_t n = 0; n < 578; n++)
        lineBuffers[n] = (Message_LineData *) 0;
    }
    catch (...) {
      if (lineBuffers)
        delete[] lineBuffers;
      throw;
    }
  }

  FLTKDisplay_::~FLTKDisplay_()
  {
    messageQueueMutex.lock();
    exitFlag = true;
    while (freeMessageStack) {
      Message *m = freeMessageStack;
      freeMessageStack = m->nxt;
      std::free(m);
    }
    while (messageQueue) {
      Message *m = messageQueue;
      messageQueue = m->nxt;
      std::free(m);
    }
    lastMessage = (Message *) 0;
    messageQueueMutex.unlock();
    for (size_t n = 0; n < 578; n++) {
      Message *m = lineBuffers[n];
      if (m) {
        lineBuffers[n] = (Message_LineData *) 0;
        std::free(m);
      }
    }
    delete[] lineBuffers;
  }

  void FLTKDisplay_::draw()
  {
  }

  int FLTKDisplay_::handle(int event)
  {
    return fltkEventCallback(fltkEventCallbackUserData, event);
  }

  void FLTKDisplay_::setDisplayParameters(const DisplayParameters& dp)
  {
    Message_SetParameters *m = allocateMessage<Message_SetParameters>();
    m->dp = dp;
    savedDisplayParameters = dp;
    queueMessage(m);
  }

  const VideoDisplay::DisplayParameters&
      FLTKDisplay_::getDisplayParameters() const
  {
    return savedDisplayParameters;
  }

  void FLTKDisplay_::drawLine(const uint8_t *buf, size_t nBytes)
  {
    if (!skippingFrame) {
      if (curLine >= 0 && curLine < 578) {
        Message_LineData  *m = allocateMessage<Message_LineData>();
        m->lineNum = curLine;
        m->copyLine(buf, nBytes);
        queueMessage(m);
      }
    }
    if (vsyncCnt != 0) {
      curLine += 2;
      if (vsyncCnt >= (EP128EMU_VSYNC_MIN_LINES + 2 - EP128EMU_VSYNC_OFFSET) &&
          (vsyncState || vsyncCnt >= (EP128EMU_VSYNC_MAX_LINES
                                      + 2 - EP128EMU_VSYNC_OFFSET))) {
        vsyncCnt = 2 - EP128EMU_VSYNC_OFFSET;
      }
      vsyncCnt++;
    }
    else {
      curLine = (oddFrame ? -1 : 0);
      vsyncCnt++;
      oddFrame = false;
      frameDone();
    }
  }

  void FLTKDisplay_::vsyncStateChange(bool newState, unsigned int currentSlot_)
  {
    vsyncState = newState;
    if (newState &&
        vsyncCnt >= (EP128EMU_VSYNC_MIN_LINES + 2 - EP128EMU_VSYNC_OFFSET)) {
      vsyncCnt = 2 - EP128EMU_VSYNC_OFFSET;
      oddFrame = (currentSlot_ >= 20U && currentSlot_ < 48U);
    }
  }

  void FLTKDisplay_::frameDone()
  {
    messageQueueMutex.lock();
    bool    skippedFrame = skippingFrame;
    if (!skippedFrame)
      framesPending++;
    bool    overrunFlag = (framesPending > 3);  // should this be configurable ?
    skippingFrame = overrunFlag;
    if (limitFrameRateFlag) {
      if (limitFrameRateTimer.getRealTime() < 0.02)
        skippingFrame = true;
      else
        limitFrameRateTimer.reset();
    }
    messageQueueMutex.unlock();
    if (skippedFrame) {
      if (overrunFlag || !limitFrameRateFlag) {
        Fl::awake();
        threadLock.wait(1);
      }
      return;
    }
    Message *m = allocateMessage<Message_FrameDone>();
    queueMessage(m);
  }

  void FLTKDisplay_::setScreenshotCallback(void (*func)(void *,
                                                        const unsigned char *,
                                                        int, int),
                                           void *userData_)
  {
    if (!screenshotCallback || !func) {
      screenshotCallback = func;
      if (func) {
        screenshotCallbackUserData = userData_;
        screenshotCallbackFlag = true;
      }
      else {
        screenshotCallbackUserData = (void *) 0;
        screenshotCallbackFlag = false;
      }
    }
  }

  void FLTKDisplay_::setFLTKEventCallback(int (*func)(void *userData,
                                                      int event),
                                          void *userData_)
  {
    if (func)
      fltkEventCallback = func;
    else
      fltkEventCallback = &defaultFLTKEventCallback;
    fltkEventCallbackUserData = userData_;
  }

  void FLTKDisplay_::limitFrameRate(bool isEnabled)
  {
    limitFrameRateFlag = isEnabled;
  }

  void FLTKDisplay_::checkScreenshotCallback()
  {
    if (!screenshotCallbackFlag)
      return;
    screenshotCallbackFlag = false;
    void    (*func)(void *, const unsigned char *, int, int);
    void    *userData_ = screenshotCallbackUserData;
    func = screenshotCallback;
    screenshotCallback = (void (*)(void *, const unsigned char *, int, int)) 0;
    screenshotCallbackUserData = (void *) 0;
    if (!func)
      return;
    unsigned char *imageBuf_ = (unsigned char *) 0;
    try {
      imageBuf_ = new unsigned char[768 * 576 + 768];
      unsigned char *p = imageBuf_;
      for (int c = 0; c <= 255; c++) {
        float   r, g, b;
        r = float(c) / 255.0f;
        g = r;
        b = r;
        if (displayParameters.indexToRGBFunc)
          displayParameters.indexToRGBFunc(uint8_t(c), r, g, b);
        r = r * 255.0f + 0.5f;
        g = g * 255.0f + 0.5f;
        b = b * 255.0f + 0.5f;
        *(p++) = (unsigned char) (r > 0.0f ? (r < 255.5f ? r : 255.5f) : 0.0f);
        *(p++) = (unsigned char) (g > 0.0f ? (g < 255.5f ? g : 255.5f) : 0.0f);
        *(p++) = (unsigned char) (b > 0.0f ? (b < 255.5f ? b : 255.5f) : 0.0f);
      }
      unsigned char lineBuf_[768];
      for (size_t yc = 1; yc < 578; yc++) {
        if (lineBuffers[yc]) {
          const unsigned char *bufp = (unsigned char *) 0;
          size_t  nBytes = 0;
          lineBuffers[yc]->getLineData(bufp, nBytes);
          decodeLine(&(lineBuf_[0]), bufp, nBytes);
        }
        else if (yc == 1 || lineBuffers[yc - 1] == (Message_LineData *) 0)
          std::memset(&(lineBuf_[0]), 0, 768);
        if (yc > 1) {
          std::memcpy(p, &(lineBuf_[0]), 768);
          p = p + 768;
        }
      }
      func(userData_, imageBuf_, 768, 576);
    }
    catch (...) {
      if (imageBuf_)
        delete[] imageBuf_;
      imageBuf_ = (unsigned char *) 0;
    }
    if (imageBuf_)
      delete[] imageBuf_;
  }

  // --------------------------------------------------------------------------

  FLTKDisplay::Colormap::Colormap()
  {
    palette = new uint32_t[256];
    try {
      palette2 = new uint32_t[65536];
    }
    catch (...) {
      delete[] palette;
      throw;
    }
    DisplayParameters dp;
    setParams(dp);
  }

  FLTKDisplay::Colormap::~Colormap()
  {
    delete[] palette;
    delete[] palette2;
  }

  void FLTKDisplay::Colormap::setParams(const DisplayParameters& dp)
  {
    float   rTbl[256];
    float   gTbl[256];
    float   bTbl[256];
    for (size_t i = 0; i < 256; i++) {
      float   r = float(uint8_t(i)) / 255.0f;
      float   g = float(uint8_t(i)) / 255.0f;
      float   b = float(uint8_t(i)) / 255.0f;
      if (dp.indexToRGBFunc)
        dp.indexToRGBFunc(uint8_t(i), r, g, b);
      dp.applyColorCorrection(r, g, b);
      rTbl[i] = r;
      gTbl[i] = g;
      bTbl[i] = b;
    }
    for (size_t i = 0; i < 256; i++) {
      palette[i] = pixelConv(rTbl[i], gTbl[i], bTbl[i]);
    }
    double  lineShade_ = double(dp.lineShade * 0.5f);
    for (size_t i = 0; i < 256; i++) {
      for (size_t j = 0; j < 256; j++) {
        double  r = (rTbl[i] + rTbl[j]) * lineShade_;
        double  g = (gTbl[i] + gTbl[j]) * lineShade_;
        double  b = (bTbl[i] + bTbl[j]) * lineShade_;
        palette2[(i << 8) + j] = pixelConv(r, g, b);
      }
    }
  }

  uint32_t FLTKDisplay::Colormap::pixelConv(double r, double g, double b)
  {
    unsigned int  ri, gi, bi;
    ri = (r > 0.0 ? (r < 1.0 ? (unsigned int) (r * 255.0 + 0.5) : 255U) : 0U);
    gi = (g > 0.0 ? (g < 1.0 ? (unsigned int) (g * 255.0 + 0.5) : 255U) : 0U);
    bi = (b > 0.0 ? (b < 1.0 ? (unsigned int) (b * 255.0 + 0.5) : 255U) : 0U);
    return ((uint32_t(ri) << 16) + (uint32_t(gi) << 8) + uint32_t(bi));
  }

  // --------------------------------------------------------------------------

  FLTKDisplay::FLTKDisplay(int xx, int yy, int ww, int hh, const char *lbl)
    : Fl_Window(xx, yy, ww, hh, lbl),
      FLTKDisplay_(),
      colormap(),
      linesChanged((bool *) 0),
      forceUpdateLineCnt(0),
      forceUpdateLineMask(0),
      redrawFlag(false),
      prvFrameWasOdd(false),
      lastLineNum(-2)
  {
    displayParameters.displayQuality = 0;
    displayParameters.bufferingMode = 0;
    savedDisplayParameters.displayQuality = 0;
    savedDisplayParameters.bufferingMode = 0;
    try {
      linesChanged = new bool[289];
      for (size_t n = 0; n < 289; n++)
        linesChanged[n] = false;
    }
    catch (...) {
      if (linesChanged)
        delete[] linesChanged;
      throw;
    }
  }

  FLTKDisplay::~FLTKDisplay()
  {
    delete[] linesChanged;
  }

  void FLTKDisplay::displayFrame()
  {
    int     windowWidth_ = this->w();
    int     windowHeight_ = this->h();
    int     displayWidth_ = windowWidth_;
    int     displayHeight_ = windowHeight_;
    bool    halfResolutionX_ = false;
    bool    halfResolutionY_ = false;
    int     x0 = 0;
    int     y0 = 0;
    int     x1 = displayWidth_;
    int     y1 = displayHeight_;
    double  aspectScale_ = (768.0 / 576.0)
                           / ((double(windowWidth_) / double(windowHeight_))
                              * double(displayParameters.pixelAspectRatio));
    if (aspectScale_ > 1.0001) {
      displayHeight_ = int((double(windowHeight_) / aspectScale_) + 0.5);
      y0 = (windowHeight_ - displayHeight_) >> 1;
      y1 = y0 + displayHeight_;
    }
    else if (aspectScale_ < 0.9999) {
      displayWidth_ = int((double(windowWidth_) * aspectScale_) + 0.5);
      x0 = (windowWidth_ - displayWidth_) >> 1;
      x1 = x0 + displayWidth_;
    }
    if (displayWidth_ < 576)
      halfResolutionX_ = true;
    if (displayHeight_ < 432)
      halfResolutionY_ = true;

    if (x0 > 0) {
      fl_color(FL_BLACK);
      fl_rectf(0, 0, x0, windowHeight_);
    }
    if (x1 < windowWidth_) {
      fl_color(FL_BLACK);
      fl_rectf(x1, 0, (windowWidth_ - x1), windowHeight_);
    }
    if (y0 > 0) {
      fl_color(FL_BLACK);
      fl_rectf(0, 0, windowWidth_, y0);
    }
    if (y1 < windowHeight_) {
      fl_color(FL_BLACK);
      fl_rectf(0, y1, windowWidth_, (windowHeight_ - y1));
    }

    if (displayWidth_ <= 0 || displayHeight_ <= 0)
      return;

    if (forceUpdateLineMask) {
      // make sure that all lines are updated at a slow rate
      for (size_t yc = 0; yc < 289; yc++) {
        if (forceUpdateLineMask & (uint8_t(1) << uint8_t((yc >> 2) & 7)))
          linesChanged[yc] = true;
      }
      forceUpdateLineMask = 0;
    }
    unsigned char lineBuf_[768];
    unsigned char *pixelBuf_ =
        (unsigned char *) std::calloc(size_t(displayWidth_ * 4 * 3),
                                      sizeof(unsigned char));
    int   lineNumbers_[5];
    if (pixelBuf_) {
      int   curLine_ = 2;
      int   fracY_ = 0;
      bool  skippingLines_ = true;
      lineNumbers_[3] = -2;
      for (int yc = 0; yc < displayHeight_; yc++) {
        int   ycAnd3 = yc & 3;
        if (ycAnd3 == 0) {
          skippingLines_ = true;
          lineNumbers_[4] = lineNumbers_[3];
        }
        int   l0 = curLine_;
        if (lineBuffers[l0]) {
          lineNumbers_[ycAnd3] = l0;
        }
        else if (lineBuffers[l0 - 1]) {
          l0 = l0 - 1;
          lineNumbers_[ycAnd3] = l0;
        }
        else {
          lineNumbers_[ycAnd3] = -1;
        }
        if (linesChanged[l0 >> 1])
          skippingLines_ = false;
        if (ycAnd3 == 3 || yc == (displayHeight_ - 1)) {
          if (!skippingLines_) {
            int   nLines_ = 4;
            if (ycAnd3 != 3)
              nLines_ = displayHeight_ & 3;
            for (int yTmp = 0; yTmp < nLines_; yTmp++) {
              unsigned char *p = &(pixelBuf_[displayWidth_ * yTmp * 3]);
              if (yTmp == 0) {
                if (lineNumbers_[0] == lineNumbers_[4] ||
                    (lineNumbers_[0] >= 0 && lineNumbers_[4] >= 0 &&
                     *(lineBuffers[lineNumbers_[0]])
                     == *(lineBuffers[lineNumbers_[4]]))) {
                  std::memcpy(p, &(pixelBuf_[displayWidth_ * 3 * 3]),
                              size_t(displayWidth_ * 3));
                  continue;
                }
              }
              else {
                if (lineNumbers_[yTmp] == lineNumbers_[yTmp - 1] ||
                    (lineNumbers_[yTmp - 1] >= 0 && lineNumbers_[yTmp] >= 0 &&
                     *(lineBuffers[lineNumbers_[yTmp]])
                     == *(lineBuffers[lineNumbers_[yTmp - 1]]))) {
                  std::memcpy(p, &(pixelBuf_[displayWidth_ * (yTmp - 1) * 3]),
                              size_t(displayWidth_ * 3));
                  continue;
                }
              }
              if (lineNumbers_[yTmp] >= 0) {
                // decode video data
                const unsigned char *bufp = (unsigned char *) 0;
                size_t  nBytes = 0;
                lineBuffers[lineNumbers_[yTmp]]->getLineData(bufp, nBytes);
                decodeLine(&(lineBuf_[0]), bufp, nBytes);
                // convert to RGB
                bufp = &(lineBuf_[0]);
                switch (displayWidth_) {
                case 384:
                  do {
                    uint32_t  tmp = colormap(bufp[0], bufp[1]);
                    p[2] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[1] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[0] = (unsigned char) tmp & (unsigned char) 0xFF;
                    bufp = bufp + 2;
                    p = p + 3;
                  } while (bufp < &(lineBuf_[768]));
                  break;
                case 768:
                  do {
                    uint32_t  tmp = colormap(bufp[0]);
                    p[2] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[1] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[0] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = colormap(bufp[1]);
                    p[5] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[4] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[3] = (unsigned char) tmp & (unsigned char) 0xFF;
                    bufp = bufp + 2;
                    p = p + 6;
                  } while (bufp < &(lineBuf_[768]));
                  break;
                case 1152:
                  do {
                    uint32_t  tmp = colormap(bufp[0]);
                    p[2] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[1] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[0] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = colormap(bufp[0], bufp[1]);
                    p[5] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[4] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[3] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = colormap(bufp[1]);
                    p[8] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[7] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[6] = (unsigned char) tmp & (unsigned char) 0xFF;
                    bufp = bufp + 2;
                    p = p + 9;
                  } while (bufp < &(lineBuf_[768]));
                  break;
                case 1536:
                  do {
                    uint32_t  tmp = colormap(*bufp);
                    p[2] = (unsigned char) tmp & (unsigned char) 0xFF;
                    p[5] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[1] = (unsigned char) tmp & (unsigned char) 0xFF;
                    p[4] = (unsigned char) tmp & (unsigned char) 0xFF;
                    tmp = tmp >> 8;
                    p[0] = (unsigned char) tmp & (unsigned char) 0xFF;
                    p[3] = (unsigned char) tmp & (unsigned char) 0xFF;
                    bufp++;
                    p = p + 6;
                  } while (bufp < &(lineBuf_[768]));
                  break;
                default:
                  {
                    int       fracX_ = displayWidth_;
                    uint32_t  c = 0U;
                    if (!halfResolutionX_) {
                      fracX_ = (fracX_ >= 768 ? fracX_ : 768);
                      while (true) {
                        if (fracX_ >= displayWidth_) {
                          if (bufp >= &(lineBuf_[768]))
                            break;
                          do {
                            c = colormap(*bufp);
                            fracX_ -= displayWidth_;
                            bufp++;
                          } while (fracX_ >= displayWidth_);
                        }
                        {
                          uint32_t  tmp = c;
                          p[2] = (unsigned char) tmp & (unsigned char) 0xFF;
                          tmp = tmp >> 8;
                          p[1] = (unsigned char) tmp & (unsigned char) 0xFF;
                          tmp = tmp >> 8;
                          p[0] = (unsigned char) tmp & (unsigned char) 0xFF;
                        }
                        fracX_ += 768;
                        p += 3;
                      }
                    }
                    else {
                      fracX_ = (fracX_ >= 384 ? fracX_ : 384);
                      while (true) {
                        if (fracX_ >= displayWidth_) {
                          if (bufp >= &(lineBuf_[768]))
                            break;
                          do {
                            c = colormap(bufp[0], bufp[1]);
                            fracX_ -= displayWidth_;
                            bufp += 2;
                          } while (fracX_ >= displayWidth_);
                        }
                        {
                          uint32_t  tmp = c;
                          p[2] = (unsigned char) tmp & (unsigned char) 0xFF;
                          tmp = tmp >> 8;
                          p[1] = (unsigned char) tmp & (unsigned char) 0xFF;
                          tmp = tmp >> 8;
                          p[0] = (unsigned char) tmp & (unsigned char) 0xFF;
                        }
                        fracX_ += 384;
                        p += 3;
                      }
                    }
                  }
                }
              }
              else {
                uint32_t  c = colormap(0x00);
                for (int xc = 0; xc < displayWidth_; xc++) {
                  p[0] = (unsigned char) ((c >> 16) & 0xFF);
                  p[1] = (unsigned char) ((c >> 8) & 0xFF);
                  p[2] = (unsigned char) (c & 0xFF);
                  p = p + 3;
                }
              }
            }
            fl_draw_image(pixelBuf_, x0, y0 + (yc & (~(int(3)))),
                          displayWidth_, nLines_);
          }
          else
            lineNumbers_[3] = -2;
        }
        if (!halfResolutionY_) {
          fracY_ += 576;
          while (fracY_ >= displayHeight_) {
            fracY_ -= displayHeight_;
            curLine_ = (curLine_ < 577 ? (curLine_ + 1) : curLine_);
          }
        }
        else {
          fracY_ += 288;
          while (fracY_ >= displayHeight_) {
            fracY_ -= displayHeight_;
            curLine_ = (curLine_ < 575 ? (curLine_ + 2) : curLine_);
          }
        }
      }
      std::free(pixelBuf_);
      for (size_t yc = 0; yc < 289; yc++)
        linesChanged[yc] = false;
    }
  }

  void FLTKDisplay::draw()
  {
    if (this->damage() & FL_DAMAGE_EXPOSE) {
      forceUpdateLineMask = 0xFF;
      forceUpdateLineCnt = 0;
      forceUpdateTimer.reset();
      redrawFlag = true;
    }
    if (redrawFlag) {
      redrawFlag = false;
      displayFrame();
    }
  }

  bool FLTKDisplay::checkEvents()
  {
    threadLock.notify();
    while (true) {
      messageQueueMutex.lock();
      Message *m = messageQueue;
      if (m) {
        messageQueue = m->nxt;
        if (messageQueue) {
          if (!messageQueue->nxt)
            lastMessage = messageQueue;
        }
        else
          lastMessage = (Message *) 0;
      }
      messageQueueMutex.unlock();
      if (!m)
        break;
      if (EP128EMU_EXPECT(m->msgType == Message::MsgType_LineData)) {
        Message_LineData  *msg;
        msg = static_cast<Message_LineData *>(m);
        int     lineNum = msg->lineNum;
        if (lineNum >= 0 && lineNum < 578) {
          lastLineNum = lineNum;
          if ((lineNum & 1) == int(prvFrameWasOdd) &&
              lineBuffers[lineNum ^ 1] != (Message_LineData *) 0) {
            // non-interlaced mode: clear any old lines in the other field
            linesChanged[lineNum >> 1] = true;
            deleteMessage(lineBuffers[lineNum ^ 1]);
            lineBuffers[lineNum ^ 1] = (Message_LineData *) 0;
          }
          // check if this line has changed
          if (lineBuffers[lineNum]) {
            if (*(lineBuffers[lineNum]) == *msg) {
              deleteMessage(m);
              continue;
            }
          }
          linesChanged[lineNum >> 1] = true;
          if (lineBuffers[lineNum])
            deleteMessage(lineBuffers[lineNum]);
          lineBuffers[lineNum] = msg;
          continue;
        }
      }
      else if (m->msgType == Message::MsgType_FrameDone) {
        // need to update display
        messageQueueMutex.lock();
        framesPending = (framesPending > 0 ? (framesPending - 1) : 0);
        framesPendingFlag = (framesPending > 0);
        messageQueueMutex.unlock();
        redrawFlag = true;
        deleteMessage(m);
        int     n = lastLineNum;
        prvFrameWasOdd = bool(n & 1);
        lastLineNum = (n & 1) - 2;
        if (n < 576) {
          // clear any remaining lines
          n = n | 1;
          do {
            n++;
            if (lineBuffers[n]) {
              linesChanged[n >> 1] = true;
              deleteMessage(lineBuffers[n]);
              lineBuffers[n] = (Message_LineData *) 0;
            }
          } while (n < 577);
        }
        noInputTimer.reset();
        if (screenshotCallbackFlag)
          checkScreenshotCallback();
        break;
      }
      else if (m->msgType == Message::MsgType_SetParameters) {
        Message_SetParameters *msg;
        msg = static_cast<Message_SetParameters *>(m);
        displayParameters = msg->dp;
        DisplayParameters tmp_dp(displayParameters);
        tmp_dp.lineShade = 1.0f;
        colormap.setParams(tmp_dp);
        for (size_t n = 0; n < 289; n++)
          linesChanged[n] = true;
      }
      deleteMessage(m);
    }
    if (noInputTimer.getRealTime() > 0.5) {
      noInputTimer.reset(0.25);
      redrawFlag = true;
      if (screenshotCallbackFlag)
        checkScreenshotCallback();
    }
    if (forceUpdateTimer.getRealTime() >= 0.15) {
      forceUpdateLineMask |= (uint8_t(1) << forceUpdateLineCnt);
      forceUpdateLineCnt++;
      forceUpdateLineCnt &= uint8_t(7);
      forceUpdateTimer.reset();
    }
    return redrawFlag;
  }

  int FLTKDisplay::handle(int event)
  {
    return fltkEventCallback(fltkEventCallbackUserData, event);
  }

  void FLTKDisplay::setDisplayParameters(const DisplayParameters& dp)
  {
    DisplayParameters dp_(dp);
    dp_.displayQuality = 0;
    dp_.bufferingMode = 0;
    FLTKDisplay_::setDisplayParameters(dp_);
  }

}       // namespace Ep128Emu

