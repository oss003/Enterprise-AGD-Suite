
// ep128emu -- portable Enterprise 128 emulator
// Copyright (C) 2003-2010 Istvan Varga <istvanv@users.sourceforge.net>
// http://sourceforge.net/projects/ep128emu/
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
#include "cpcio.hpp"

static uint8_t dummyReadCallback(void *userData, uint16_t addr)
{
    (void) userData; (void) addr;
    return 0xFF;
}

static void dummyWriteCallback(void *userData, uint16_t addr, uint8_t value)
{
    (void) userData; (void) addr; (void) value;
}

namespace CPC464 {

  IOPorts::IOPorts()
  {
    callbackUserData = (void *) 0;
    readCallback = &dummyReadCallback;
    writeCallback = &dummyWriteCallback;
    debugReadCallback = (uint8_t (*)(void *, uint16_t)) 0;
    breakPointTable = (uint8_t *) 0;
    breakPointCnt = 0;
    breakPointPriorityThreshold = 0;
    try {
      breakPointTable = new uint8_t[256];
      for (int i = 0; i < 256; i++)
        breakPointTable[i] = 0;
    }
    catch (...) {
      if (breakPointTable) {
        delete[] breakPointTable;
        breakPointTable = (uint8_t *) 0;
      }
      throw;
    }
  }

  IOPorts::~IOPorts()
  {
    delete[] breakPointTable;
  }

  void IOPorts::setBreakPoint(uint16_t addr, int priority, bool r, bool w)
  {
    uint8_t mode = (r ? 1 : 0) + (w ? 2 : 0);
    if (mode) {
      // create new breakpoint, or change existing one
      mode += (uint8_t) ((priority > 0 ? (priority < 3 ? priority : 3) : 0)
                         << 2);
      if (!breakPointTable) {
        breakPointTable = new uint8_t[256];
        for (int i = 0; i < 256; i++)
          breakPointTable[i] = 0;
      }
      uint8_t&  bp = breakPointTable[addr & 0xFF];
      if (!bp)
        breakPointCnt++;
      if (bp > mode)
        mode = (bp & 12) + (mode & 3);
      mode |= (bp & 3);
      bp = mode;
    }
    else if (breakPointTable) {
      if (breakPointTable[addr & 0xFF]) {
        // remove a previously existing breakpoint
        breakPointTable[addr & 0xFF] = 0;
        breakPointCnt--;
        if (!breakPointCnt) {
          delete[] breakPointTable;
          breakPointTable = (uint8_t*) 0;
        }
      }
    }
  }

  void IOPorts::clearBreakPoints()
  {
    for (unsigned int addr = 0; addr < 256; addr++)
      setBreakPoint((uint16_t) addr, 0, false, false);
  }

  void IOPorts::breakPointCallback(bool isWrite, uint16_t addr, uint8_t value)
  {
    (void) isWrite;
    (void) addr;
    (void) value;
  }

  void IOPorts::setBreakPointPriorityThreshold(int n)
  {
    breakPointPriorityThreshold =
        (uint8_t) ((n > 0 ? (n < 4 ? n : 4) : 0) << 2);
  }

  int IOPorts::getBreakPointPriorityThreshold()
  {
    return ((int) breakPointPriorityThreshold >> 2);
  }

  Ep128Emu::BreakPointList IOPorts::getBreakPointList()
  {
    Ep128Emu::BreakPointList  bplst;
    if (breakPointTable) {
      for (size_t i = 0; i < 256; i++) {
        uint8_t bp = breakPointTable[i];
        if (bp)
          bplst.addIOBreakPoint(uint16_t(i), !!(bp & 1), !!(bp & 2), bp >> 2);
      }
    }
    return bplst;
  }

  uint8_t IOPorts::readDebug(uint16_t addr) const
  {
    if (debugReadCallback)
      return debugReadCallback(callbackUserData, addr);
    return 0xFF;
  }

  void IOPorts::writeDebug(uint16_t addr, uint8_t value)
  {
    writeCallback(callbackUserData, addr, value);
  }

  void IOPorts::setCallbackUserData(void *userData)
  {
    callbackUserData = userData;
  }

  void IOPorts::setReadCallback(uint8_t (*func)(void *userData, uint16_t addr))
  {
    if (func)
      readCallback = func;
    else
      readCallback = dummyReadCallback;
  }

  void IOPorts::setDebugReadCallback(uint8_t (*func)(void *userData,
                                                     uint16_t addr))
  {
    debugReadCallback = func;
  }

  void IOPorts::setWriteCallback(void (*func)(void *userData,
                                              uint16_t addr, uint8_t value))
  {
    if (func)
      writeCallback = func;
    else
      writeCallback = dummyWriteCallback;
  }

}       // namespace CPC464

