
// ep128emu -- portable Enterprise 128 emulator
// Copyright (C) 2003-2016 Istvan Varga <istvanv@users.sourceforge.net>
// https://github.com/istvan-v/ep128emu/
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

#include "gui.hpp"
#include "debuglib.hpp"
#include "monitor.hpp"

#include <typeinfo>
#include <vector>

#define MONITOR_MAX_LINES   (160)

using Ep128Emu::parseHexNumberEx;

Ep128EmuGUIMonitor::Ep128EmuGUIMonitor(int xx, int yy, int ww, int hh,
                                       const char *ll)
  : Fl_Text_Editor(xx, yy, ww, hh, ll),
    buf_((Fl_Text_Buffer *) 0),
    debugWindow((Ep128EmuGUI_DebugWindow *) 0),
    gui((Ep128EmuGUI *) 0),
    assembleOffset(int32_t(0)),
    disassembleAddress(0U),
    disassembleOffset(int32_t(0)),
    memoryDumpAddress(0U),
    addressMask(0xFFFFU),
    cpuAddressMode(true),
    traceFlags(0x00),
    traceFile((std::FILE *) 0),
    traceInstructionsRemaining(0)
{
  buf_ = new Fl_Text_Buffer();
  buffer(buf_);
  add_key_binding(FL_Enter, FL_TEXT_EDITOR_ANY_STATE, &enterKeyCallback);
  insert_mode(0);
  scrollbar_align(FL_ALIGN_RIGHT);
}

Ep128EmuGUIMonitor::~Ep128EmuGUIMonitor()
{
  closeTraceFile();
  buffer((Fl_Text_Buffer *) 0);
  delete buf_;
}

void Ep128EmuGUIMonitor::command_assemble(const std::vector<std::string>& args)
{
  if (args.size() == 2) {
    (void) parseHexNumberEx(args[1].c_str());
    return;
  }
  uint32_t  addr = Ep128::Z80Disassembler::assembleInstruction(
                       args, gui->vm, cpuAddressMode, assembleOffset);
  this->move_up();
  disassembleOffset = -assembleOffset;
  disassembleAddress = addr;
  disassembleInstruction(true);
  addr = uint32_t((int32_t(disassembleAddress) + disassembleOffset)
                  & int32_t(addressMask));
  char    tmpBuf[16];
  if (cpuAddressMode)
    std::sprintf(&(tmpBuf[0]), "A   %04X  ", (unsigned int) addr);
  else
    std::sprintf(&(tmpBuf[0]), "A %06X  ", (unsigned int) addr);
  this->overstrike(&(tmpBuf[0]));
  show_insert_position();
}

void Ep128EmuGUIMonitor::command_disassemble(const std::vector<std::string>&
                                                 args)
{
  size_t    argOffs = 1;
  std::FILE *f = (std::FILE *) 0;
  if (args.size() > 1) {
    if (args[1].length() >= 1) {
      if (args[1][0] == '"') {
        argOffs++;
        std::string fileName(args[1].c_str() + 1);
        int     err = gui->vm.openFileInWorkingDirectory(f, fileName, "w");
        if (err != 0) {
          printMessage(gui->vm.getFileOpenErrorMessage(err));
          return;
        }
      }
    }
  }
  try {
    if (args.size() > (argOffs + 3))
      throw Ep128Emu::Exception("invalid number of disassemble arguments");
    uint32_t  startAddr = disassembleAddress & addressMask;
    if (args.size() > argOffs)
      startAddr = parseHexNumberEx(args[argOffs].c_str(), addressMask);
    disassembleAddress = startAddr;
    uint32_t  endAddr = (startAddr + 20U) & addressMask;
    if (args.size() > (argOffs + 1))
      endAddr = parseHexNumberEx(args[argOffs + 1].c_str(), addressMask);
    if (args.size() > (argOffs + 2)) {
      uint32_t  tmp = parseHexNumberEx(args[argOffs + 2].c_str(), addressMask);
      disassembleOffset = int32_t(tmp) - int32_t(startAddr);
      if (disassembleOffset > int32_t(addressMask >> 1))
        disassembleOffset -= int32_t(addressMask + 1U);
      else if (disassembleOffset < -(int32_t((addressMask >> 1) + 1U)))
        disassembleOffset += int32_t(addressMask + 1U);
    }
    std::string tmpBuf;
    if (!f) {
      // disassemble to screen
      while (((endAddr - disassembleAddress) & addressMask)
             > (MONITOR_MAX_LINES * 4U)) {
        uint32_t  nextAddr = gui->vm.disassembleInstruction(tmpBuf,
                                                            disassembleAddress,
                                                            cpuAddressMode,
                                                            disassembleOffset);
        disassembleAddress = nextAddr & addressMask;
      }
      while (true) {
        uint32_t  prvAddr = disassembleAddress;
        disassembleInstruction();
        while (prvAddr != disassembleAddress) {
          if (prvAddr == endAddr)
            return;
          prvAddr = (prvAddr + 1U) & addressMask;
        }
      }
    }
    else {
      // disassemble to file
      while (true) {
        uint32_t  prvAddr = disassembleAddress;
        uint32_t  nextAddr = gui->vm.disassembleInstruction(tmpBuf,
                                                            disassembleAddress,
                                                            cpuAddressMode,
                                                            disassembleOffset);
        int       n = std::fprintf(f, ". %s\n", tmpBuf.c_str());
        if (size_t(n) != (tmpBuf.length() + 3)) {
          printMessage("Error writing file");
          break;
        }
        disassembleAddress = nextAddr & addressMask;
        while (prvAddr != disassembleAddress) {
          if (prvAddr == endAddr) {
            int     err = std::fflush(f);
            std::fclose(f);
            f = (std::FILE *) 0;
            if (err != 0)
              printMessage("Error writing file");
            return;
          }
          prvAddr = (prvAddr + 1U) & addressMask;
        }
      }
    }
  }
  catch (...) {
    if (f)
      std::fclose(f);
    throw;
  }
  if (f)
    std::fclose(f);
}

void Ep128EmuGUIMonitor::command_memoryDump(const std::vector<std::string>&
                                                args)
{
  if (args.size() > 3)
    throw Ep128Emu::Exception("invalid number of memory dump arguments");
  uint32_t  startAddr = memoryDumpAddress & addressMask;
  if (args.size() > 1)
    startAddr = parseHexNumberEx(args[1].c_str(), addressMask);
  memoryDumpAddress = startAddr;
  uint32_t  endAddr = (startAddr + 95U) & addressMask;
  if (args.size() > 2)
    endAddr = parseHexNumberEx(args[2].c_str(), addressMask);
  while (((endAddr - memoryDumpAddress) & addressMask)
         > (MONITOR_MAX_LINES * 8U)) {
    memoryDumpAddress = (memoryDumpAddress + 8U) & addressMask;
  }
  while (true) {
    uint32_t  prvAddr = memoryDumpAddress;
    memoryDump();
    while (prvAddr != memoryDumpAddress) {
      if (prvAddr == endAddr)
        return;
      prvAddr = (prvAddr + 1U) & addressMask;
    }
  }
}

void Ep128EmuGUIMonitor::command_memoryModify(const std::vector<std::string>&
                                                  args)
{
  if (args.size() < 2)
    throw Ep128Emu::Exception("insufficient arguments for memory modify");
  uint32_t  addr = parseHexNumberEx(args[1].c_str(), addressMask);
  memoryDumpAddress = addr;
  for (size_t i = 2; i < args.size(); i++) {
    if (args[i] == ":")
      break;
    uint32_t  value = parseHexNumberEx(args[i].c_str());
    if (value >= 0x0100U)
      throw Ep128Emu::Exception("byte value is out of range");
    gui->vm.writeMemory(addr, uint8_t(value), cpuAddressMode);
    addr = (addr + 1U) & addressMask;
  }
  this->move_up();
  memoryDump();
}

void Ep128EmuGUIMonitor::command_ioDump(const std::vector<std::string>& args)
{
  if (args.size() < 2 || args.size() > 3)
    throw Ep128Emu::Exception("invalid number of I/O dump arguments");
  uint32_t  startAddr = parseHexNumberEx(args[1].c_str(), 0xFFFFU);
  uint32_t  endAddr = (startAddr + 95U) & 0xFFFFU;
  if (args.size() > 2)
    endAddr = parseHexNumberEx(args[2].c_str(), 0xFFFFU);
  while (((endAddr - startAddr) & 0xFFFFU) > (MONITOR_MAX_LINES * 8U))
    startAddr = (startAddr + 8U) & 0xFFFFU;
  bool    doneFlag = false;
  do {
    uint32_t  addr = startAddr;
    char      tmpBuf[64];
    uint8_t   dataBuf[8];
    for (int i = 0; i < 8; i++) {
      dataBuf[i] = gui->vm.readIOPort(uint16_t(addr)) & 0xFF;
      if (addr == endAddr)
        doneFlag = true;
      addr = (addr + 1U) & 0xFFFFU;
    }
    std::sprintf(&(tmpBuf[0]),
                 "O %04X  %02X %02X %02X %02X %02X %02X %02X %02X",
                 (unsigned int) startAddr,
                 (unsigned int) dataBuf[0], (unsigned int) dataBuf[1],
                 (unsigned int) dataBuf[2], (unsigned int) dataBuf[3],
                 (unsigned int) dataBuf[4], (unsigned int) dataBuf[5],
                 (unsigned int) dataBuf[6], (unsigned int) dataBuf[7]);
    printMessage(&(tmpBuf[0]));
    startAddr = addr;
  } while (!doneFlag);
}

void Ep128EmuGUIMonitor::command_ioModify(const std::vector<std::string>& args)
{
  if (args.size() < 2)
    throw Ep128Emu::Exception("insufficient arguments for I/O modify");
  size_t  addr = size_t(parseHexNumberEx(args[1].c_str(), 0xFFFFU));
  for (size_t i = 2; i < args.size(); i++) {
    if (args[i] == ":")
      break;
    uint32_t  value = parseHexNumberEx(args[i].c_str());
    if (value >= 0x0100U)
      throw Ep128Emu::Exception("byte value is out of range");
    gui->vm.writeIOPort(uint16_t((addr + (i - 2)) & 0xFFFF), uint8_t(value));
  }
  this->move_up();
  char    tmpBuf[64];
  uint8_t dataBuf[8];
  for (int i = 0; i < 8; i++)
    dataBuf[i] = gui->vm.readIOPort(uint16_t((addr + i) & 0xFFFF)) & 0xFF;
  std::sprintf(&(tmpBuf[0]), "O %04X  %02X %02X %02X %02X %02X %02X %02X %02X",
               (unsigned int) addr,
               (unsigned int) dataBuf[0], (unsigned int) dataBuf[1],
               (unsigned int) dataBuf[2], (unsigned int) dataBuf[3],
               (unsigned int) dataBuf[4], (unsigned int) dataBuf[5],
               (unsigned int) dataBuf[6], (unsigned int) dataBuf[7]);
  printMessage(&(tmpBuf[0]));
}

void Ep128EmuGUIMonitor::command_printRegisters(const std::vector<std::string>&
                                                    args)
{
  if (args.size() != 1)
    throw Ep128Emu::Exception("too many arguments");
  printMessage("  PC   AF   BC   DE   HL   SP   IX   IY");
  printCPURegisters1();
  printMessage("       AF'  BC'  DE'  HL'  IM   I    R");
  printCPURegisters2();
}

void Ep128EmuGUIMonitor::command_setRegisters(const std::vector<std::string>&
                                                  args)
{
  if ((args.size() < 2 || args.size() > 9) ||
      (args[1] == ";" && args.size() == 2)) {
    throw Ep128Emu::Exception("invalid number of arguments");
  }
  uint32_t  regValues[8];
  size_t    offs = 1;
  if (args[1] == ";")
    offs++;
  size_t    nValues = args.size() - offs;
  for (size_t i = 0; i < nValues; i++) {
    regValues[i] = parseHexNumberEx(args[i + offs].c_str());
    if (offs == 1 && i == 0) {
      if (regValues[i] > 0xFFFFU)
        throw Ep128Emu::Exception("address is out of range");
    }
    else if (offs == 2 && i >= 5) {
      if (regValues[i] > 0xFFU)
        throw Ep128Emu::Exception("byte value is out of range");
    }
    else if (offs == 2 && i == 4) {
      if (regValues[i] > 0x02U)
        throw Ep128Emu::Exception("interrupt mode is out of range");
    }
    else {
      if (regValues[i] > 0xFFFFU)
        throw Ep128Emu::Exception("16 bit register value is out of range");
    }
  }
  Ep128::Z80_REGISTERS& r = gui->vm.getZ80Registers();
  if (offs == 1) {
    gui->vm.setProgramCounter(uint16_t(regValues[0]));
    if (nValues > 1)
      r.AF.W = Ep128::Z80_WORD(regValues[1]);
    if (nValues > 2)
      r.BC.W = Ep128::Z80_WORD(regValues[2]);
    if (nValues > 3)
      r.DE.W = Ep128::Z80_WORD(regValues[3]);
    if (nValues > 4)
      r.HL.W = Ep128::Z80_WORD(regValues[4]);
    if (nValues > 5)
      r.SP.W = Ep128::Z80_WORD(regValues[5]);
    if (nValues > 6)
      r.IX.W = Ep128::Z80_WORD(regValues[6]);
    if (nValues > 7)
      r.IY.W = Ep128::Z80_WORD(regValues[7]);
    this->move_up();
    printCPURegisters1();
  }
  else {
    r.altAF.W = Ep128::Z80_WORD(regValues[0]);
    if (nValues > 1)
      r.altBC.W = Ep128::Z80_WORD(regValues[1]);
    if (nValues > 2)
      r.altDE.W = Ep128::Z80_WORD(regValues[2]);
    if (nValues > 3)
      r.altHL.W = Ep128::Z80_WORD(regValues[3]);
    if (nValues > 4)
      r.IM = Ep128::Z80_BYTE(regValues[4]);
    if (nValues > 5)
      r.I = Ep128::Z80_BYTE(regValues[5]);
    if (nValues > 6) {
      r.R = Ep128::Z80_BYTE(regValues[6] & 0x7FU);
      r.RBit7 = Ep128::Z80_BYTE(regValues[6] & 0x80U);
    }
    this->move_up();
    printCPURegisters2();
  }
}

void Ep128EmuGUIMonitor::command_go(const std::vector<std::string>& args)
{
  if (args.size() > 2)
    throw Ep128Emu::Exception("too many arguments");
  if (args.size() > 1) {
    uint32_t  tmp = parseHexNumberEx(args[1].c_str());
    if (tmp > 0xFFFFU)
      throw Ep128Emu::Exception("address is out of range");
    gui->vm.setProgramCounter(uint16_t(tmp));
  }
  debugWindow->focusWidget = this;
  gui->vm.setSingleStepMode(0);
  debugWindow->deactivate();
}

void Ep128EmuGUIMonitor::command_searchPattern(const std::vector<std::string>&
                                                   args)
{
  if (args.size() < 4)
    throw Ep128Emu::Exception("insufficient arguments for search");
  uint32_t  startAddr = parseHexNumberEx(args[1].c_str());
  uint32_t  endAddr = parseHexNumberEx(args[2].c_str());
  if ((startAddr | endAddr) > addressMask)
    throw Ep128Emu::Exception("address is out of range");
  size_t    matchCnt = 0;
  uint32_t  matchAddrs[64];
  while (true) {
    int32_t   nextAddr = searchPattern(args, 3, args.size() - 3,
                                       startAddr, endAddr, cpuAddressMode);
    if (nextAddr < 0) {
      startAddr = endAddr;
      break;
    }
    startAddr = uint32_t(nextAddr) & addressMask;
    matchAddrs[matchCnt++] = startAddr;
    if (matchCnt >= 64 || startAddr == endAddr)
      break;
    startAddr = (startAddr + 1U) & addressMask;
  }
  char    tmpBuf[64];
  int     bufPos = 0;
  for (size_t i = 0; i < matchCnt; i++) {
    int     n = std::sprintf(&(tmpBuf[bufPos]), " %0*X",
                             int(cpuAddressMode ? 4 : 6),
                             (unsigned int) matchAddrs[i]);
    bufPos += n;
    if (((i & 3) == 3 && bufPos >= 28) || i == (matchCnt - 1)) {
      printMessage(&(tmpBuf[0]));
      bufPos = 0;
    }
  }
  if (startAddr != endAddr) {
    std::sprintf(&(tmpBuf[0]), "H %X %X",
                 (unsigned int) ((startAddr + 1U) & addressMask),
                 (unsigned int) endAddr);
    std::string newCmd(&(tmpBuf[0]));
    for (size_t i = 3; i < args.size(); i++) {
      newCmd += ' ';
      newCmd += args[i];
      if (args[i].length() > 0) {
        if (args[i][0] == '"')
          newCmd += '"';
      }
    }
    printMessage(newCmd.c_str());
    this->move_up();
  }
}

void Ep128EmuGUIMonitor::command_searchAndReplace(
    const std::vector<std::string>& args)
{
  size_t  searchArgOffs = 3;
  size_t  searchArgCnt = 0;
  for (size_t i = searchArgOffs; i < args.size(); i++) {
    if (args[i] == ",")
      break;
    searchArgCnt++;
  }
  size_t  replaceArgOffs = searchArgCnt + 4;
  size_t  replaceArgCnt = 0;
  for (size_t i = replaceArgOffs; i < args.size(); i++) {
    replaceArgCnt++;
  }
  if (searchArgCnt < 1 || replaceArgCnt < 1)
    throw Ep128Emu::Exception("insufficient arguments for search/replace");
  std::vector<uint8_t>  searchString_;
  std::vector<uint8_t>  searchMask_;
  parseSearchPattern(searchString_, searchMask_,
                     args, searchArgOffs, searchArgCnt);
  std::vector<uint8_t>  replaceString_;
  std::vector<uint8_t>  replaceMask_;
  parseSearchPattern(replaceString_, replaceMask_,
                     args, replaceArgOffs, replaceArgCnt);
  if (replaceString_.size() < 1)
    throw Ep128Emu::Exception("insufficient arguments for search/replace");
  uint32_t  startAddr = parseHexNumberEx(args[1].c_str());
  uint32_t  endAddr = parseHexNumberEx(args[2].c_str());
  if ((startAddr | endAddr) > addressMask)
    throw Ep128Emu::Exception("address is out of range");
  size_t    replaceCnt = 0;
  while (true) {
    int32_t   nextAddr = searchPattern(searchString_, searchMask_,
                                       startAddr, endAddr, cpuAddressMode);
    if (nextAddr < 0)
      break;
    replaceCnt++;
    startAddr = uint32_t(nextAddr) & addressMask;
    size_t  j = 0;
    do {
      uint8_t c = gui->vm.readMemory(startAddr, cpuAddressMode);
      c &= (replaceMask_[j] ^ uint8_t(0xFF));
      c |= (replaceString_[j] & replaceMask_[j]);
      gui->vm.writeMemory(startAddr, c, cpuAddressMode);
      if (startAddr == endAddr)
        break;
      startAddr = (startAddr + 1U) & addressMask;
    } while (++j < replaceString_.size());
    if (j < replaceString_.size())
      break;
  }
  char    tmpBuf[64];
  std::sprintf(&(tmpBuf[0]), "Replaced %lu matches",
               (unsigned long) replaceCnt);
  printMessage(&(tmpBuf[0]));
}

void Ep128EmuGUIMonitor::command_memoryCopy(const std::vector<std::string>&
                                                args)
{
  if (args.size() != 4)
    throw Ep128Emu::Exception("invalid number of memory copy arguments");
  uint32_t  srcStartAddr = parseHexNumberEx(args[1].c_str());
  uint32_t  srcEndAddr = parseHexNumberEx(args[2].c_str());
  uint32_t  dstStartAddr = parseHexNumberEx(args[3].c_str());
  if ((srcStartAddr | srcEndAddr | dstStartAddr) > addressMask)
    throw Ep128Emu::Exception("address is out of range");
  if (srcStartAddr >= dstStartAddr) {
    while (true) {
      uint8_t c = gui->vm.readMemory(srcStartAddr, cpuAddressMode);
      gui->vm.writeMemory(dstStartAddr, c, cpuAddressMode);
      if (srcStartAddr == srcEndAddr)
        break;
      srcStartAddr = (srcStartAddr + 1U) & addressMask;
      dstStartAddr = (dstStartAddr + 1U) & addressMask;
    }
  }
  else {
    uint32_t  dstEndAddr =
        (dstStartAddr + (srcEndAddr - srcStartAddr)) & addressMask;
    while (true) {
      uint8_t c = gui->vm.readMemory(srcEndAddr, cpuAddressMode);
      gui->vm.writeMemory(dstEndAddr, c, cpuAddressMode);
      if (srcStartAddr == srcEndAddr)
        break;
      srcEndAddr = (srcEndAddr - 1U) & addressMask;
      dstEndAddr = (dstEndAddr - 1U) & addressMask;
    }
  }
}

void Ep128EmuGUIMonitor::command_memoryFill(const std::vector<std::string>&
                                                args)
{
  if (args.size() < 4)
    throw Ep128Emu::Exception("insufficient arguments for memory fill");
  uint32_t  startAddr = parseHexNumberEx(args[1].c_str());
  uint32_t  endAddr = parseHexNumberEx(args[2].c_str());
  if ((startAddr | endAddr) > addressMask)
    throw Ep128Emu::Exception("address is out of range");
  std::vector<uint8_t>  pattern_;
  for (size_t i = 3; i < args.size(); i++) {
    uint32_t  c = parseHexNumberEx(args[i].c_str());
    if (c > 0xFFU)
      throw Ep128Emu::Exception("byte value is out of range");
    pattern_.push_back(uint8_t(c));
  }
  size_t  patternPos = 0;
  while (true) {
    gui->vm.writeMemory(startAddr, pattern_[patternPos], cpuAddressMode);
    if (startAddr == endAddr)
      break;
    startAddr = (startAddr + 1U) & addressMask;
    if (++patternPos >= pattern_.size())
      patternPos = 0;
  }
}

void Ep128EmuGUIMonitor::command_memoryCompare(const std::vector<std::string>&
                                                   args)
{
  if (args.size() != 4)
    throw Ep128Emu::Exception("invalid number of memory compare arguments");
  uint32_t  startAddr1 = parseHexNumberEx(args[1].c_str());
  uint32_t  endAddr1 = parseHexNumberEx(args[2].c_str());
  uint32_t  startAddr2 = parseHexNumberEx(args[3].c_str());
  if ((startAddr1 | endAddr1 | startAddr2) > addressMask)
    throw Ep128Emu::Exception("address is out of range");
  size_t    diffCnt = 0;
  uint32_t  diffAddrs[64];
  size_t    bytesRemaining = size_t((endAddr1 - startAddr1) & addressMask) + 1;
  do {
    uint8_t c1 = gui->vm.readMemory(startAddr1, cpuAddressMode);
    uint8_t c2 = gui->vm.readMemory(startAddr2, cpuAddressMode);
    if (c1 != c2)
      diffAddrs[diffCnt++] = startAddr1;
    startAddr1 = (startAddr1 + 1U) & addressMask;
    startAddr2 = (startAddr2 + 1U) & addressMask;
    bytesRemaining--;
  } while (diffCnt < 64 && bytesRemaining > 0);
  char    tmpBuf[64];
  int     bufPos = 0;
  for (size_t i = 0; i < diffCnt; i++) {
    int     n = std::sprintf(&(tmpBuf[bufPos]), " %0*X",
                             int(cpuAddressMode ? 4 : 6),
                             (unsigned int) diffAddrs[i]);
    bufPos += n;
    if (((i & 3) == 3 && bufPos >= 28) || i == (diffCnt - 1)) {
      printMessage(&(tmpBuf[0]));
      bufPos = 0;
    }
  }
  if (bytesRemaining > 0) {
    std::sprintf(&(tmpBuf[0]), "C %X %X %X",
                 (unsigned int) startAddr1, (unsigned int) endAddr1,
                 (unsigned int) startAddr2);
    printMessage(&(tmpBuf[0]));
    this->move_up();
  }
}

void Ep128EmuGUIMonitor::command_assemblerOffset(
    const std::vector<std::string>& args)
{
  if (args.size() <= 1) {
    assembleOffset = int32_t(0);
  }
  else {
    bool    negativeFlag = false;
    size_t  argOffs = 1;
    if (args[1] == "-") {
      negativeFlag = true;
      argOffs++;
    }
    else if (args[1] == "+") {
      argOffs++;
    }
    if (args.size() != (argOffs + 1))
      throw Ep128Emu::Exception("invalid number of assemble offset arguments");
    assembleOffset =
        int32_t(parseHexNumberEx(args[argOffs].c_str(), addressMask));
    if (assembleOffset > int32_t(addressMask >> 1))
      assembleOffset -= int32_t(addressMask + 1U);
    if (negativeFlag)
      assembleOffset = -assembleOffset;
  }
  disassembleOffset = -assembleOffset;
  char    tmpBuf[128];
  if (assembleOffset == 0) {
    std::sprintf(&(tmpBuf[0]), "Assemble offset set to 0\n"
                               "Disassemble offset set to 0");
  }
  else {
    int     n = (cpuAddressMode ? 4 : 6);
    if (assembleOffset > 0) {
      std::sprintf(&(tmpBuf[0]), "Assemble offset set to +%0*X\n"
                                 "Disassemble offset set to -%0*X",
                   n, (unsigned int) assembleOffset,
                   n, (unsigned int) assembleOffset);
    }
    else {
      std::sprintf(&(tmpBuf[0]), "Assemble offset set to -%0*X\n"
                                 "Disassemble offset set to +%0*X",
                   n, (unsigned int) disassembleOffset,
                   n, (unsigned int) disassembleOffset);
    }
  }
  printMessage(&(tmpBuf[0]));
}

void Ep128EmuGUIMonitor::command_printInfo(
    const std::vector<std::string>& args)
{
  if (args.size() > 1)
    throw Ep128Emu::Exception("too many arguments");
  if (cpuAddressMode)
    printMessage("Address mode:        CPU (16 bit)");
  else
    printMessage("Address mode:        physical (22 bit)");
  char    tmpBuf[128];
  int     n = (cpuAddressMode ? 4 : 6);
  std::sprintf(&(tmpBuf[0]), "Assemble offset:    %c%0*X",
               int(assembleOffset == 0 ?
                   ' ' : (assembleOffset > 0 ? '+' : '-')),
               n,
               (unsigned int) (assembleOffset >= 0 ?
                               assembleOffset : (-assembleOffset)));
  printMessage(&(tmpBuf[0]));
  std::sprintf(&(tmpBuf[0]), "Disassemble offset: %c%0*X",
               int(disassembleOffset == 0 ?
                   ' ' : (disassembleOffset > 0 ? '+' : '-')),
               n,
               (unsigned int) (disassembleOffset >= 0 ?
                               disassembleOffset : (-disassembleOffset)));
  printMessage(&(tmpBuf[0]));
  std::sprintf(&(tmpBuf[0]), "Memory paging:       %02X %02X %02X %02X",
               (unsigned int) gui->vm.getMemoryPage(0),
               (unsigned int) gui->vm.getMemoryPage(1),
               (unsigned int) gui->vm.getMemoryPage(2),
               (unsigned int) gui->vm.getMemoryPage(3));
  printMessage(&(tmpBuf[0]));
}

void Ep128EmuGUIMonitor::command_continue(const std::vector<std::string>& args)
{
  if (args.size() > 1)
    throw Ep128Emu::Exception("too many arguments");
  debugWindow->focusWidget = this;
  gui->vm.setSingleStepMode(0);
  debugWindow->deactivate();
}

void Ep128EmuGUIMonitor::command_step(const std::vector<std::string>& args)
{
  if (args.size() > 1)
    throw Ep128Emu::Exception("too many arguments");
  debugWindow->focusWidget = this;
  gui->vm.setSingleStepMode(1);
  debugWindow->deactivate();
}

void Ep128EmuGUIMonitor::command_stepOver(const std::vector<std::string>& args)
{
  if (args.size() > 1)
    throw Ep128Emu::Exception("too many arguments");
  debugWindow->focusWidget = this;
  gui->vm.setSingleStepMode(2);
  debugWindow->deactivate();
}

void Ep128EmuGUIMonitor::command_trace(const std::vector<std::string>& args)
{
  closeTraceFile();
  if (args.size() < 2 || args.size() > 5)
    throw Ep128Emu::Exception("invalid number of arguments");
  if (args[1].length() < 1 || args[1][0] != '"')
    throw Ep128Emu::Exception("file name is not a string");
  uint32_t  maxInsns = 0U;
  int32_t   startAddr = int32_t(-1);
  if (args.size() > 2) {
    maxInsns = parseHexNumberEx(args[2].c_str());
    if (maxInsns > 0x01000000U)
      throw Ep128Emu::Exception("invalid instruction count");
  }
  if (!maxInsns)
    maxInsns = 65536U;
  if (args.size() > 3) {
    if (args[3] != "*") {
      uint32_t  n = parseHexNumberEx(args[3].c_str());
      if (n > 0xFFFFU)
        throw Ep128Emu::Exception("address is out of range");
      startAddr = int32_t(n);
    }
  }
  if (args.size() > 4)
    traceFlags = uint8_t(parseHexNumberEx(args[4].c_str(), 0xFFU));
  else
    traceFlags = 0x03;
  std::string fileName(args[1].c_str() + 1);
  std::FILE *f = (std::FILE *) 0;
  int       err = gui->vm.openFileInWorkingDirectory(f, fileName, "w");
  if (err != 0) {
    printMessage(gui->vm.getFileOpenErrorMessage(err));
    return;
  }
  traceFile = f;
  traceInstructionsRemaining = size_t(maxInsns);
  if (startAddr >= 0)
    gui->vm.setProgramCounter(uint16_t(startAddr));
  debugWindow->focusWidget = this;
  gui->vm.setSingleStepMode(3);
  debugWindow->deactivate();
}

void Ep128EmuGUIMonitor::command_load(const std::vector<std::string>& args,
                                      bool verifyMode)
{
  if (args.size() < 4 || args.size() > 5)
    throw Ep128Emu::Exception("invalid number of arguments");
  if (args[1].length() < 1 || args[1][0] != '"')
    throw Ep128Emu::Exception("file name is not a string");
  uint32_t  asciiMode = parseHexNumberEx(args[2].c_str());
  uint32_t  startAddr = parseHexNumberEx(args[3].c_str());
  uint32_t  endAddr = 0U;
  bool      haveEndAddr = (args.size() > 4);
  if (haveEndAddr)
    endAddr = parseHexNumberEx(args[4].c_str());
  if (asciiMode > 1U)
    throw Ep128Emu::Exception("ASCII mode flag must be 0 or 1");
  if ((startAddr | endAddr) > addressMask)
    throw Ep128Emu::Exception("address is out of range");
  if (!haveEndAddr)
    endAddr = 0xFFFFFFFFU;
  try {
    size_t  nBytes =
        gui->vm.loadMemory(args[1].c_str() + 1,
                           verifyMode, bool(asciiMode), cpuAddressMode,
                           startAddr, endAddr);
    char    tmpBuf[64];
    if (!verifyMode) {
      if (nBytes > 0) {
        endAddr = (startAddr + uint32_t(nBytes) - 1U) & addressMask;
        int     n = (cpuAddressMode ? 4 : 6);
        std::sprintf(&(tmpBuf[0]), "Loaded file to %0*X-%0*X",
                     n, (unsigned int) startAddr, n, (unsigned int) endAddr);
        printMessage(&(tmpBuf[0]));
      }
    }
    else {
      std::sprintf(&(tmpBuf[0]), "%lu difference(s)", (unsigned long) nBytes);
      printMessage(&(tmpBuf[0]));
    }
  }
  catch (std::exception& e) {
    printMessage(e.what());
    return;
  }
}

void Ep128EmuGUIMonitor::command_save(const std::vector<std::string>& args)
{
  if (args.size() != 5)
    throw Ep128Emu::Exception("invalid number of arguments");
  if (args[1].length() < 1 || args[1][0] != '"')
    throw Ep128Emu::Exception("file name is not a string");
  uint32_t  asciiMode = parseHexNumberEx(args[2].c_str());
  uint32_t  startAddr = parseHexNumberEx(args[3].c_str());
  uint32_t  endAddr = parseHexNumberEx(args[4].c_str());
  if (asciiMode > 1U)
    throw Ep128Emu::Exception("ASCII mode flag must be 0 or 1");
  if ((startAddr | endAddr) > addressMask)
    throw Ep128Emu::Exception("address is out of range");
  try {
    gui->vm.saveMemory(args[1].c_str() + 1, bool(asciiMode), cpuAddressMode,
                       startAddr, endAddr);
  }
  catch (std::exception& e) {
    printMessage(e.what());
    return;
  }
}

void Ep128EmuGUIMonitor::command_toggleCPUAddressMode(
    const std::vector<std::string>& args)
{
  if (args.size() > 2)
    throw Ep128Emu::Exception("too many arguments for address mode");
  bool    oldCPUAddressMode = cpuAddressMode;
  if (args.size() > 1) {
    uint32_t  n = parseHexNumberEx(args[1].c_str());
    cpuAddressMode = (n != 0U);
  }
  else
    cpuAddressMode = !cpuAddressMode;
  addressMask = (cpuAddressMode ? 0x0000FFFFU : 0x003FFFFFU);
  if (cpuAddressMode != oldCPUAddressMode) {
    assembleOffset = int32_t(0);
    disassembleOffset = int32_t(0);
    if (cpuAddressMode)
      printMessage("CPU address mode");
    else
      printMessage("Physical address mode");
  }
}

void Ep128EmuGUIMonitor::command_help(const std::vector<std::string>& args)
{
  if (!(args.size() == 2 ||
        (args.size() == 3 && args[1] == ";" && args[2] == ";"))) {
    printMessage(".       assemble");
    printMessage(";       set PC, AF, BC, DE, HL, SP, IX, IY");
    printMessage(";;      set AF', BC', DE', HL', IM, I, R");
    printMessage(">       modify memory");
    printMessage("?       print help");
    printMessage("A       assemble");
    printMessage("AM      set/toggle CPU/physical address mode");
    printMessage("AO      set assemble and disassemble offset");
    printMessage("C       compare memory");
    printMessage("D       disassemble");
    printMessage("F       fill memory with pattern");
    printMessage("G       continue or go to address");
    printMessage("H       search for pattern in memory");
    printMessage("I       print current settings");
    printMessage("IO      dump I/O registers");
    printMessage("L       load binary or ASCII file to memory");
    printMessage("M       dump memory");
    printMessage("O       modify I/O registers");
    printMessage("R       print CPU registers");
    printMessage("S       save memory to binary or ASCII file");
    printMessage("SR      search and replace pattern in memory");
    printMessage("T       copy memory");
    printMessage("TR      trace (log instructions to file)");
    printMessage("V       verify (compare memory and file)");
    printMessage("X       continue");
    printMessage("Y       step over");
    printMessage("Z       step");
  }
  else if (args[1] == "." || args[1] == "A") {
    printMessage("A <address> ...");
  }
  else if (args[1] == ";") {
    if (args.size() == 2)
      printMessage(";<pc> [af [bc [de [hl [sp [ix [iy]]]]]]]");
    else
      printMessage(";;<af'> [bc' [de' [hl' [im [i [r]]]]]]");
  }
  else if (args[1] == ">") {
    printMessage("><address> [value1 [value2 [...]]]");
  }
  else if (args[1] == "?") {
    printMessage("?       print short help for all commands");
    printMessage("? <cmd> print detailed help for a command");
  }
  else if (args[1] == "AM") {
    printMessage("AM      toggle CPU/physical address mode");
    printMessage("AM 0    set physical (22 bit) address mode");
    printMessage("AM 1    set CPU (16 bit) address mode");
    printMessage("Changing the address mode resets assemble");
    printMessage("and disassemble offset to zero");
  }
  else if (args[1] == "AO") {
    printMessage("AO      reset assemble/disassemble offset");
    printMessage("AO <n>  set assemble offset to +n bytes");
    printMessage("AO +<n> set assemble offset to +n bytes");
    printMessage("AO -<n> set assemble offset to -n bytes");
    printMessage("Disassemble offset is set to -(asm offset)");
  }
  else if (args[1] == "C") {
    printMessage("C <start1> <end1> <start2>");
  }
  else if (args[1] == "D") {
    printMessage("D [\"filename\"] [start [end [runtimeAddr]]]");
  }
  else if (args[1] == "F") {
    printMessage("F <start> <end> <value1> [value2 [...]]");
  }
  else if (args[1] == "G") {
    printMessage("G       continue emulation");
    printMessage("G<addr> go to address and continue emulation");
  }
  else if (args[1] == "H") {
    printMessage("H <start> <end> <pattern>");
    printMessage("Pattern can include any of the following:");
    printMessage("    NN  search for exact byte value NN");
    printMessage("  MMNN  search for byte NN with bit mask MM");
    printMessage("     *  matches any single byte");
    printMessage(" \"str\"  search for string (bit mask = 7F)");
  }
  else if (args[1] == "I") {
    printMessage("I       print current monitor settings");
  }
  else if (args[1] == "IO") {
    printMessage("IO <start> [end]");
  }
  else if (args[1] == "L") {
    printMessage("L <\"filename\"> <asciiMode> <start> [end]");
    printMessage("'asciiMode' is 0 for binary, and 1 for text");
  }
  else if (args[1] == "M") {
    printMessage("M [start [end]]");
  }
  else if (args[1] == "O") {
    printMessage("O<address> [value1 [value2 [...]]]");
  }
  else if (args[1] == "R") {
    printMessage("R       print CPU registers");
  }
  else if (args[1] == "S") {
    printMessage("S <\"filename\"> <asciiMode> <start> <end>");
    printMessage("'asciiMode' is 0 for binary, and 1 for text");
  }
  else if (args[1] == "SR") {
    printMessage("SR <start> <end> <searchPat>, <replacePat>");
    printMessage("Patterns can include any of the following:");
    printMessage("    NN  exact byte value NN");
    printMessage("  MMNN  byte NN with bit mask MM");
    printMessage("     *  match any single byte / no change");
    printMessage(" \"str\"  string (bit mask = 7F)");
  }
  else if (args[1] == "T") {
    printMessage("T <srcStart> <srcEnd> <dstStart>");
  }
  else if (args[1] == "TR") {
    printMessage("TR <\"filename\"> [maxInsns [addr [flags]]]");
    printMessage("maxInsns=0 (default) is interpreted as 65536L");
    printMessage("addr can be * to continue from the current PC");
    printMessage("flags is an 8-bit value that enables the printing");
    printMessage("of [X,Y], AF, BC, DE, HL, SP, segment, and opcode");
  }
  else if (args[1] == "V") {
    printMessage("V <\"filename\"> <asciiMode> <start> [end]");
    printMessage("'asciiMode' is 0 for binary, and 1 for text");
  }
  else if (args[1] == "X") {
    printMessage("X       continue emulation");
  }
  else if (args[1] == "Y") {
    printMessage("Y       step one instruction with step over");
  }
  else if (args[1] == "Z") {
    printMessage("Z       step one CPU instruction");
  }
  else {
    printMessage("Unknown command name");
  }
}

int Ep128EmuGUIMonitor::enterKeyCallback(int c, Fl_Text_Editor *e_)
{
  (void) c;
  Ep128EmuGUIMonitor& e = *(reinterpret_cast<Ep128EmuGUIMonitor *>(e_));
  const char  *s = e.buf_->line_text(e.insert_position());
  e.moveDown();
  if (s) {
    if (s[0] != '\0' && s[0] != '\n') {
      try {
        e.parseCommand(s);
      }
      catch (...) {
        e.move_up();
        e.insert_position(e.buf_->line_end(e.insert_position()) - 1);
        e.overstrike("?");
        e.moveDown();
      }
    }
    std::free(const_cast<char *>(s));
  }
  return 1;
}

void Ep128EmuGUIMonitor::moveDown()
{
  try {
    insert_position(buf_->line_end(insert_position()));
    if (insert_position() >= buf_->length()) {
      int     n = buf_->count_lines(0, buf_->length());
      while (n >= MONITOR_MAX_LINES) {
        buf_->remove(0, buf_->line_end(0) + 1);
        n--;
      }
      insert_position(buf_->length());
      this->insert("\n");
    }
    else {
      move_down();
      insert_position(buf_->line_start(insert_position()));
    }
    show_insert_position();
  }
  catch (...) {
  }
}

void Ep128EmuGUIMonitor::parseCommand(const char *s)
{
  std::vector<std::string>  args;
  Ep128Emu::tokenizeString(args, s);
  if (args.size() == 0)
    return;
  if (args[0] == ";")
    command_setRegisters(args);
  else if (args[0] == ">")
    command_memoryModify(args);
  else if (args[0] == "?")
    command_help(args);
  else if (args[0] == "A" || args[0] == ".")
    command_assemble(args);
  else if (args[0] == "AM")
    command_toggleCPUAddressMode(args);
  else if (args[0] == "AO")
    command_assemblerOffset(args);
  else if (args[0] == "C")
    command_memoryCompare(args);
  else if (args[0] == "D")
    command_disassemble(args);
  else if (args[0] == "F")
    command_memoryFill(args);
  else if (args[0] == "G")
    command_go(args);
  else if (args[0] == "H")
    command_searchPattern(args);
  else if (args[0] == "I")
    command_printInfo(args);
  else if (args[0] == "IO")
    command_ioDump(args);
  else if (args[0] == "L")
    command_load(args, false);
  else if (args[0] == "M")
    command_memoryDump(args);
  else if (args[0] == "O")
    command_ioModify(args);
  else if (args[0] == "R")
    command_printRegisters(args);
  else if (args[0] == "S")
    command_save(args);
  else if (args[0] == "SR")
    command_searchAndReplace(args);
  else if (args[0] == "T")
    command_memoryCopy(args);
  else if (args[0] == "TR")
    command_trace(args);
  else if (args[0] == "V")
    command_load(args, true);
  else if (args[0] == "X")
    command_continue(args);
  else if (args[0] == "Y")
    command_stepOver(args);
  else if (args[0] == "Z")
    command_step(args);
  else
    throw Ep128Emu::Exception("invalid monitor command");
}

void Ep128EmuGUIMonitor::printMessage(const char *s)
{
  if (!s)
    return;
  try {
    std::string tmpBuf;
    while (true) {
      tmpBuf = "";
      while (*s != '\0' && *s != '\n') {
        tmpBuf += (*s);
        s++;
      }
      insert_position(buf_->line_start(insert_position()));
      overstrike(tmpBuf.c_str());
      if (insert_position() < buf_->line_end(insert_position()))
        buf_->remove(insert_position(), buf_->line_end(insert_position()));
      moveDown();
      if (*s == '\0')
        break;
      s++;
    }
  }
  catch (...) {
  }
}

void Ep128EmuGUIMonitor::disassembleInstruction(bool assembleMode)
{
  disassembleAddress = disassembleAddress & addressMask;
  std::string tmpBuf;
  uint32_t  nextAddr = gui->vm.disassembleInstruction(tmpBuf,
                                                      disassembleAddress,
                                                      cpuAddressMode,
                                                      disassembleOffset);
  disassembleAddress = nextAddr & addressMask;
  if (!assembleMode)
    tmpBuf = std::string(". ") + tmpBuf;
  else
    tmpBuf = std::string("A ") + tmpBuf;
  printMessage(tmpBuf.c_str());
}

void Ep128EmuGUIMonitor::memoryDump()
{
  char      tmpBuf[64];
  uint8_t   dataBuf[8];
  uint32_t  startAddr = memoryDumpAddress & addressMask;
  memoryDumpAddress = startAddr;
  for (int i = 0; i < 8; i++) {
    dataBuf[i] = gui->vm.readMemory(memoryDumpAddress, cpuAddressMode) & 0xFF;
    memoryDumpAddress = (memoryDumpAddress + 1U) & addressMask;
  }
  char    *bufp = &(tmpBuf[0]);
  int     n = std::sprintf(bufp, ">%0*X",
                           int(cpuAddressMode ? 4 : 6),
                           (unsigned int) startAddr);
  bufp = bufp + n;
  n = std::sprintf(bufp, "  %02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned int) dataBuf[0], (unsigned int) dataBuf[1],
                   (unsigned int) dataBuf[2], (unsigned int) dataBuf[3],
                   (unsigned int) dataBuf[4], (unsigned int) dataBuf[5],
                   (unsigned int) dataBuf[6], (unsigned int) dataBuf[7]);
  bufp = bufp + n;
  for (int i = 0; i < 8; i++) {
    dataBuf[i] &= uint8_t(0x7F);
    if (dataBuf[i] < uint8_t(' ') || dataBuf[i] == uint8_t(0x7F))
      dataBuf[i] = uint8_t('.');
  }
  std::sprintf(bufp, "  :%c%c%c%c%c%c%c%c",
               int(dataBuf[0]), int(dataBuf[1]), int(dataBuf[2]),
               int(dataBuf[3]), int(dataBuf[4]), int(dataBuf[5]),
               int(dataBuf[6]), int(dataBuf[7]));
  printMessage(&(tmpBuf[0]));
}

void Ep128EmuGUIMonitor::printCPURegisters1()
{
  const Ep128::Z80_REGISTERS& r =
      ((const Ep128Emu::VirtualMachine *) &(gui->vm))->getZ80Registers();
  char    tmpBuf[128];
  std::sprintf(&(tmpBuf[0]), ";%04X %04X %04X %04X %04X %04X %04X %04X",
               (unsigned int) gui->vm.getProgramCounter(),
               (unsigned int) r.AF.W, (unsigned int) r.BC.W,
               (unsigned int) r.DE.W, (unsigned int) r.HL.W,
               (unsigned int) r.SP.W,
               (unsigned int) r.IX.W, (unsigned int) r.IY.W);
  printMessage(&(tmpBuf[0]));
}

void Ep128EmuGUIMonitor::printCPURegisters2()
{
  const Ep128::Z80_REGISTERS& r =
      ((const Ep128Emu::VirtualMachine *) &(gui->vm))->getZ80Registers();
  char    tmpBuf[128];
  std::sprintf(&(tmpBuf[0]), ";;    %04X %04X %04X %04X  %02X   %02X   %02X",
               (unsigned int) r.altAF.W, (unsigned int) r.altBC.W,
               (unsigned int) r.altDE.W, (unsigned int) r.altHL.W,
               (unsigned int) r.IM, (unsigned int) r.I,
               (unsigned int) (r.RBit7 | (r.R & 0x7F)));
  printMessage(&(tmpBuf[0]));
}

void Ep128EmuGUIMonitor::breakMessage(const char *s)
{
  try {
    if (s == (char *) 0 || s[0] == '\0')
      s = "BREAK";
    printMessage(s);
    printMessage("  PC   AF   BC   DE   HL   SP   IX   IY");
    printCPURegisters1();
    printMessage("       AF'  BC'  DE'  HL'  IM   I    R");
    printCPURegisters2();
  }
  catch (...) {
  }
}

void Ep128EmuGUIMonitor::parseSearchPattern(
    std::vector<uint8_t>& searchString_, std::vector<uint8_t>& searchMask_,
    const std::vector<std::string>& args, size_t argOffs, size_t argCnt)
{
  for (size_t i = argOffs; i < args.size() && i < (argOffs + argCnt); i++) {
    if (args[i].length() >= 1) {
      if (args[i][0] == '"') {
        for (size_t j = 1; j < args[i].length(); j++) {
          searchString_.push_back(uint8_t(args[i][j] & 0x7F));
          searchMask_.push_back(uint8_t(0x7F));
        }
      }
      else if (args[i] == "*") {
        // match any byte
        searchString_.push_back(uint8_t(0x00));
        searchMask_.push_back(uint8_t(0x00));
      }
      else {
        uint32_t  n = parseHexNumberEx(args[i].c_str());
        if (n > 0xFFFFU)
          throw Ep128Emu::Exception("search value is out of range");
        uint32_t  m = 0xFFU;
        // use upper 8 bits as AND mask
        if (n > 0xFFU)
          m = n >> 8;
        searchString_.push_back(uint8_t(n & m));
        searchMask_.push_back(m);
      }
    }
  }
}

int32_t Ep128EmuGUIMonitor::searchPattern(
    const std::vector<uint8_t>& searchString_,
    const std::vector<uint8_t>& searchMask_,
    uint32_t startAddr, uint32_t endAddr, bool cpuAddressMode_)
{
  uint32_t  addrMask_ = (cpuAddressMode_ ? 0x0000FFFFU : 0x003FFFFFU);
  uint32_t  i = startAddr & addrMask_;
  if (searchString_.size() < 1)
    return int32_t(i);                  // empty string
  uint32_t  j = i;
  size_t    l = size_t((endAddr - startAddr) & addrMask_) + 1;
  if (l < searchString_.size())
    return int32_t(-1);
  l = l - searchString_.size();
  std::vector<uint8_t>  tmpBuf;
  tmpBuf.resize(searchString_.size());
  for (size_t k = 0; k < searchString_.size(); k++) {
    tmpBuf[k] = gui->vm.readMemory(j, cpuAddressMode_);
    j = (j + 1U) & addrMask_;
  }
  size_t  bufPos = 0;
  while (true) {
    size_t  p = bufPos;
    size_t  k = 0;
    while (true) {
      if ((tmpBuf[p] & searchMask_[k]) != searchString_[k])
        break;
      if (++k >= searchString_.size())
        return int32_t(i);              // found a match, return start address
      if (++p >= tmpBuf.size())
        p = 0;
    }
    if (!l)
      break;
    l--;
    tmpBuf[bufPos] = gui->vm.readMemory(j, cpuAddressMode_);
    i = (i + 1U) & addrMask_;
    j = (j + 1U) & addrMask_;
    if (++bufPos >= tmpBuf.size())
      bufPos = 0;
  }
  // not found
  return int32_t(-1);
}

int32_t Ep128EmuGUIMonitor::searchPattern(const std::vector<std::string>& args,
                                          size_t argOffs, size_t argCnt,
                                          uint32_t startAddr, uint32_t endAddr,
                                          bool cpuAddressMode_)
{
  std::vector<uint8_t>  searchString_;
  std::vector<uint8_t>  searchMask_;
  parseSearchPattern(searchString_, searchMask_, args, argOffs, argCnt);
  return searchPattern(searchString_, searchMask_,
                       startAddr, endAddr, cpuAddressMode_);
}

void Ep128EmuGUIMonitor::writeTraceFile(uint16_t addr)
{
  if (!traceInstructionsRemaining) {
    closeTraceFile();
    return;
  }
  traceInstructionsRemaining--;
  char    tmpBuf[96];
  char    *bufp = &(tmpBuf[0]);
  if (traceFlags & 0x80) {
    int     xPos = 0;
    int     yPos = 0;
    gui->vm.getVideoPosition(xPos, yPos);
    int     n = std::sprintf(bufp,
                             (typeid(gui->vm) == typeid(Ep128::Ep128VM) ?
                              "[%2u,%05X] " : "[%3u,%3u] "),
                             (unsigned int) xPos, (unsigned int) yPos);
    bufp = bufp + n;
  }
  if (traceFlags & 0x7C) {
    const Ep128::Z80_REGISTERS& r =
        ((const Ep128Emu::VirtualMachine *) &(gui->vm))->getZ80Registers();
    if (traceFlags & 0x40) {
      int     n = std::sprintf(bufp, "AF=%04X ", (unsigned int) r.AF.W);
      bufp = bufp + n;
    }
    if (traceFlags & 0x20) {
      int     n = std::sprintf(bufp, "BC=%04X ", (unsigned int) r.BC.W);
      bufp = bufp + n;
    }
    if (traceFlags & 0x10) {
      int     n = std::sprintf(bufp, "DE=%04X ", (unsigned int) r.DE.W);
      bufp = bufp + n;
    }
    if (traceFlags & 0x08) {
      int     n = std::sprintf(bufp, "HL=%04X ", (unsigned int) r.HL.W);
      bufp = bufp + n;
    }
    if (traceFlags & 0x04) {
      int     n = std::sprintf(bufp, "SP=%04X ", (unsigned int) r.SP.W);
      bufp = bufp + n;
    }
  }
  if (traceFlags & 0x02) {
    bufp = Ep128Emu::printHexNumber(bufp, gui->vm.getMemoryPage(addr >> 14),
                                    0, 2, 0);
    *(bufp++) = ':';
  }
  bufp = Ep128Emu::printHexNumber(bufp, addr, 0, 4, 0);
  if (traceFlags & 0x01) {
    try {
      std::string tmpBuf2;
      tmpBuf2.reserve(48);
      gui->vm.disassembleInstruction(tmpBuf2, addr, true);
      if (tmpBuf2.length() > 21 && tmpBuf2.length() <= 40) {
        int     n = std::sprintf(bufp, "  %s", tmpBuf2.c_str() + 21);
        bufp = bufp + n;
      }
    }
    catch (...) {
    }
  }
  *(bufp++) = '\n';
  size_t  len = size_t(bufp - &(tmpBuf[0]));
  if (std::fwrite(&(tmpBuf[0]), sizeof(char), len, traceFile) != len)
    closeTraceFile();           // error writing file, disk may be full
  if (!traceInstructionsRemaining)
    closeTraceFile();
}

void Ep128EmuGUIMonitor::closeTraceFile()
{
  traceInstructionsRemaining = 0;
  std::FILE *f = traceFile;
  if (f) {
    traceFile = (std::FILE *) 0;
    std::fclose(f);
  }
}

