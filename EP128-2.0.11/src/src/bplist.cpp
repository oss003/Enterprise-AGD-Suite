
// ep128emu -- portable Enterprise 128 emulator
// Copyright (C) 2003-2011 Istvan Varga <istvanv@users.sourceforge.net>
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
#include "bplist.hpp"

#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Ep128Emu {

  BreakPoint::BreakPoint(bool isIO_, bool haveSegment_,
                         bool r_, bool w_, bool x_, bool ignoreFlag_,
                         uint8_t segment_, uint16_t addr_, int priority_)
  {
    n_ = (r_ ? 0x01000000U : 0x00000000U)
         | (w_ ? 0x02000000U : 0x00000000U)
         | (x_ ? 0x04000000U : 0x00000000U)
         | (ignoreFlag_ ? 0x20000000U : 0x00000000U);
    n_ = (n_ != 0U ? n_ : 0x07000000U)
         | (priority_ > 0 ?
            (priority_ < 3 ? (uint32_t(priority_) << 22) : 0x00C00000U)
            : 0x00000000U);
    if (isIO_) {
      n_ = (n_ & 0xDBFFFFFFU) | 0x10000000U
           | ((n_ & 0x03000000U) != 0U ? 0x00000000U : 0x03000000U)
           | uint32_t(addr_ & 0xFFFF);
    }
    else if (haveSegment_) {
      n_ = n_ | 0x08000000U
           | (uint32_t(segment_ & 0xFF) << 14) | uint32_t(addr_ & 0x3FFF);
    }
    else {
      n_ = n_ | uint32_t(addr_ & 0xFFFF);
    }
  }

  BreakPointList::BreakPointList(const std::string& lst)
  {
    std::map<uint32_t, BreakPoint>  bpList;
    std::string curToken = "";

    for (size_t i = 0; i < lst.length(); i++) {
      {
        char    ch = lst[i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
          if (curToken.length() < 1)
            continue;
        }
        else {
          curToken += ch;
          if ((i + 1) < lst.length())
            continue;
        }
      }
      size_t    j;
      uint16_t  addr = 0, lastAddr;
      uint8_t   segment = 0;
      bool      isIO = false, haveSegment = false;
      bool      isRead = false, isWrite = false, isExecute = false;
      bool      isIgnore = false;
      int       priority = -1;
      uint32_t  n;

      n = 0;
      for (j = 0; j < curToken.length(); j++) {
        char    c = curToken[j];
        if (c >= '0' && c <= '9')
          n = (n << 4) + uint32_t(c - '0');
        else if (c >= 'A' && c <= 'F')
          n = (n << 4) + uint32_t(c - 'A') + 10;
        else if (c >= 'a' && c <= 'f')
          n = (n << 4) + uint32_t(c - 'a') + 10;
        else
          break;
      }
      if (j != 2 && j != 4)
        throw Exception("syntax error in breakpoint list");
      if (j == 2 && j < curToken.length() && curToken[j] == ':') {
        j++;
        haveSegment = true;
        segment = uint8_t(n);
        n = 0;
        for ( ; j < curToken.length(); j++) {
          char    c = curToken[j];
          if (c >= '0' && c <= '9')
            n = (n << 4) + uint32_t(c - '0');
          else if (c >= 'A' && c <= 'F')
            n = (n << 4) + uint32_t(c - 'A') + 10;
          else if (c >= 'a' && c <= 'f')
            n = (n << 4) + uint32_t(c - 'a') + 10;
          else
            break;
        }
        if (j != 7)
          throw Exception("syntax error in breakpoint list");
      }
      else if (j == 2)
        isIO = true;
      addr = uint16_t(n);
      lastAddr = addr;
      if (j < curToken.length() && curToken[j] == '-') {
        size_t  len = 0;
        n = 0;
        j++;
        for ( ; j < curToken.length(); j++, len++) {
          char    c = curToken[j];
          if (c >= '0' && c <= '9')
            n = (n << 4) + uint32_t(c - '0');
          else if (c >= 'A' && c <= 'F')
            n = (n << 4) + uint32_t(c - 'A') + 10;
          else if (c >= 'a' && c <= 'f')
            n = (n << 4) + uint32_t(c - 'a') + 10;
          else
            break;
        }
        if (isIO) {
          if (len != 2)
            throw Exception("syntax error in breakpoint list");
        }
        else if (len != 4 || (haveSegment && ((n ^ addr) & 0xC000U) != 0U))
          throw Exception("syntax error in breakpoint list");
        if (n < addr)
          throw Exception("syntax error in breakpoint list");
        lastAddr = n;
      }
      if (haveSegment) {
        addr = addr & 0x3FFF;
        lastAddr = lastAddr & 0x3FFF;
      }
      for ( ; j < curToken.length(); j++) {
        switch (curToken[j]) {
        case 'r':
          isRead = true;
          break;
        case 'w':
          isWrite = true;
          break;
        case 'x':
          isExecute = true;
          break;
        case 'i':
          isIgnore = true;
          break;
        case 'p':
          if (++j >= curToken.length())
            throw Exception("syntax error in breakpoint list");
          if (curToken[j] < '0' || curToken[j] > '3')
            throw Exception("syntax error in breakpoint list");
          priority = int(curToken[j] - '0');
          break;
        default:
          throw Exception("syntax error in breakpoint list");
        }
      }
      if (isIgnore) {
        if (isIO)
          throw Exception("ignore flag is not allowed for I/O breakpoints");
        if (isRead || isWrite || isExecute || priority >= 0)
          throw Exception("read/write flags and priority are not allowed "
                          "for ignore breakpoints");
        priority = 0;
      }
      else if (isIO && isExecute) {
        throw Exception("execute flag is not allowed for I/O breakpoints");
      }
      else if (!isRead && !isWrite && !isExecute) {
        isRead = true;
        isWrite = true;
        isExecute = !isIO;
      }
      priority = (priority >= 0 ? priority : 2);
      uint32_t  addr_ = addr;
      if (isIO)
        addr_ |= uint32_t(0x80000000UL);
      else if (haveSegment)
        addr_ |= (uint32_t(0x40000000UL) | (uint32_t(segment) << 14));
      BreakPoint  bp(isIO, haveSegment, isRead, isWrite, isExecute, isIgnore,
                     segment, addr, priority);
      while (true) {
        std::map<uint32_t, BreakPoint>::iterator  i_ = bpList.find(addr_);
        if (i_ == bpList.end()) {
          // add new breakpoint
          bpList.insert(std::pair<uint32_t, BreakPoint>(addr_, bp));
        }
        else {
          // update existing breakpoint
          BreakPoint  *bpp = &((*i_).second);
          if (isIgnore) {
            (*bpp) = bp;
          }
          else {
            // isIO, haveSegment, priority, segment, address
            // (only the priorities may differ)
            uint32_t  tmp1 = bpp->n_ & 0x18FFFFFFU;
            uint32_t  tmp2 = bp.n_ & 0x18FFFFFFU;
            // combine read, write, execute, and ignore flags
            bpp->n_ = ((bpp->n_ | bp.n_) & 0x27000000U)
                      | (tmp1 >= tmp2 ? tmp1 : tmp2);
          }
        }
        if (addr >= lastAddr)
          break;
        addr++;
        addr_++;
        bp.n_++;
      }
      curToken.clear();
    }

    if (bpList.size() > 0) {
      lst_.reserve(bpList.size());
      std::map<uint32_t, BreakPoint>::iterator  i_;
      for (i_ = bpList.begin(); i_ != bpList.end(); i_++)
        lst_.push_back((*i_).second);
    }
  }

  void BreakPointList::addMemoryBreakPoint(uint8_t segment, uint16_t addr,
                                           bool r, bool w, bool x,
                                           bool ignoreFlag, int priority)
  {
    lst_.push_back(BreakPoint(false, true, r, w, x, ignoreFlag,
                              segment, addr, priority));
  }

  void BreakPointList::addMemoryBreakPoint(uint16_t addr,
                                           bool r, bool w, bool x,
                                           bool ignoreFlag, int priority)
  {
    lst_.push_back(BreakPoint(false, false, r, w, x, ignoreFlag,
                              0, addr, priority));
  }

  void BreakPointList::addIOBreakPoint(uint16_t addr,
                                       bool r, bool w, int priority)
  {
    lst_.push_back(BreakPoint(true, false, r, w, false, false,
                              0, addr & 0xFF, priority));
  }

  std::string BreakPointList::getBreakPointList()
  {
    std::ostringstream  lst;
    lst << std::hex << std::uppercase << std::right << std::setfill('0');
    if (lst_.size() > 0) {
      std::stable_sort(lst_.begin(), lst_.end());
      BreakPoint  prv_bp(lst_[0]);
      uint16_t    firstAddr = prv_bp.addr();
      for (size_t i = 1; i < lst_.size(); i++) {
        BreakPoint  bp(lst_[i]);
        uint16_t    lastAddr = prv_bp.addr();
        if (i >= (lst_.size() - 1)) {
          lastAddr = bp.addr();
        }
        else {
          if ((bp.addr() == (lastAddr + 1U) || bp.addr() == lastAddr) &&
              bp.isIO() == prv_bp.isIO() &&
              bp.haveSegment() == prv_bp.haveSegment() &&
              bp.isRead() == prv_bp.isRead() &&
              bp.isWrite() == prv_bp.isWrite() &&
              bp.isExecute() == prv_bp.isExecute() &&
              bp.isIgnore() == prv_bp.isIgnore() &&
              bp.priority() == prv_bp.priority() &&
              bp.segment() == prv_bp.segment()) {
            prv_bp = bp;
            continue;
          }
        }
        if (prv_bp.haveSegment())
          lst << std::setw(2) << uint32_t(prv_bp.segment()) << ":";
        // bit 0 = read, bit 1 = write, bit 2 = execute
        uint8_t rwxFlags = uint8_t((prv_bp.n_ & 0x07000000U) >> 24);
        if (prv_bp.isIO()) {
          rwxFlags = rwxFlags & 3;
          rwxFlags = (rwxFlags != 0 ? rwxFlags : uint8_t(3));
        }
        else {
          if (prv_bp.isIgnore()) {
            lst << std::setw(4) << firstAddr;
            if (lastAddr != firstAddr)
              lst << "-" << std::setw(4) << lastAddr;
            lst << "i\n";
            if (rwxFlags != 0 && prv_bp.haveSegment())
              lst << std::setw(2) << uint32_t(prv_bp.segment()) << ":";
          }
          else {
            rwxFlags = (rwxFlags != 0 ? rwxFlags : uint8_t(7));
          }
        }
        if (rwxFlags) {
          lst << std::setw(prv_bp.isIO() ? 2 : 4) << firstAddr;
          if (lastAddr != firstAddr)
            lst << "-" << std::setw(prv_bp.isIO() ? 2 : 4) << lastAddr;
          if ((rwxFlags | (uint8_t(prv_bp.isIO()) << 2)) != 7) {
            if (rwxFlags & 1)
              lst << "r";
            if (rwxFlags & 2)
              lst << "w";
            if (rwxFlags & 4)
              lst << "x";
          }
          if (prv_bp.priority() != 2)
            lst << "p" << std::setw(1) << prv_bp.priority();
          lst << "\n";
        }
        prv_bp = bp;
        firstAddr = bp.addr();
      }
    }
    lst << "\n";
    return lst.str();
  }

  // --------------------------------------------------------------------------

  class ChunkType_BPList : public File::ChunkTypeHandler {
   private:
    BreakPointList& ref;
   public:
    ChunkType_BPList(BreakPointList& ref_)
      : File::ChunkTypeHandler(),
        ref(ref_)
    {
    }
    virtual ~ChunkType_BPList()
    {
    }
    virtual File::ChunkType getChunkType() const
    {
      return File::EP128EMU_CHUNKTYPE_BREAKPOINTS;
    }
    virtual void processChunk(File::Buffer& buf)
    {
      ref.loadState(buf);
    }
  };

  void BreakPointList::saveState(File::Buffer& buf)
  {
    buf.setPosition(0);
    buf.writeUInt32(0x01000002);        // version number
    for (size_t i = 0; i < lst_.size(); i++) {
      buf.writeBoolean(lst_[i].isIO());
      buf.writeBoolean(lst_[i].haveSegment());
      buf.writeBoolean(lst_[i].isRead());
      buf.writeBoolean(lst_[i].isWrite());
      buf.writeBoolean(lst_[i].isExecute());
      buf.writeBoolean(lst_[i].isIgnore());
      buf.writeByte(lst_[i].segment());
      buf.writeUInt32(lst_[i].addr());
      buf.writeByte(uint8_t(lst_[i].priority()));
    }
  }

  void BreakPointList::saveState(File& f)
  {
    File::Buffer  buf;
    this->saveState(buf);
    f.addChunk(File::EP128EMU_CHUNKTYPE_BREAKPOINTS, buf);
  }

  void BreakPointList::loadState(File::Buffer& buf)
  {
    buf.setPosition(0);
    // check version number
    unsigned int  version = buf.readUInt32();
    if (!(version >= 0x01000001 && version <= 0x01000002)) {
      buf.setPosition(buf.getDataSize());
      throw Exception("incompatible breakpoint list format");
    }
    // reset breakpoint list
    lst_.clear();
    // load saved state
    while (buf.getPosition() < buf.getDataSize()) {
      bool      isIO = buf.readBoolean();
      bool      haveSegment = buf.readBoolean();
      bool      isRead = buf.readBoolean();
      bool      isWrite = buf.readBoolean();
      bool      isExecute = isRead;
      if (version >= 0x01000002)
        isExecute = buf.readBoolean();
      bool      isIgnore = buf.readBoolean();
      uint8_t   segment = buf.readByte();
      uint16_t  addr = uint16_t(buf.readUInt32());
      int       priority = buf.readByte();
      BreakPoint  bp(isIO, haveSegment, isRead, isWrite, isExecute, isIgnore,
                     segment, addr, priority);
      lst_.push_back(bp);
    }
  }

  void BreakPointList::registerChunkType(File& f)
  {
    ChunkType_BPList  *p;
    p = new ChunkType_BPList(*this);
    try {
      f.registerChunkType(p);
    }
    catch (...) {
      delete p;
      throw;
    }
  }

}       // namespace Ep128Emu

