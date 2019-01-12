
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

#include "ep128emu.hpp"
#include "vm.hpp"
#include "ep128vm.hpp"
#include "bplist.hpp"
#include "script.hpp"
#include "debuglib.hpp"

#ifdef HAVE_LUA_H
extern "C" {
#  include "lua.h"
#  include "lauxlib.h"
#  include "lualib.h"
}
#  ifdef USING_OLD_LUA_API
// if using old Lua 5.0.x API:
typedef intptr_t lua_Integer;
static inline lua_Integer lua_tointeger(lua_State *L, int idx)
{
  return lua_Integer(lua_tonumber(L, idx));
}
static inline void lua_pushinteger(lua_State *L, lua_Integer n)
{
  lua_pushnumber(L, lua_Number(n));
}
static inline const char *lua_tolstring(lua_State *L, int idx, size_t *len)
{
  const char  *s = lua_tostring(L, idx);
  if (len) {
    if (s)
      (*len) = std::strlen(s);
    else
      (*len) = 0;
  }
  return s;
}
static inline void lua_setfield(lua_State *L, int idx, const char *k)
{
  lua_pushstring(L, k);
  lua_insert(L, -2);
  lua_settable(L, idx);
}
static inline void lua_getfield(lua_State *L, int idx, const char *k)
{
  lua_pushstring(L, k);
  lua_gettable(L, idx);
}
static inline void luaL_openlibs(lua_State *L)
{
  luaopen_base(L);
  luaopen_table(L);
  luaopen_io(L);
  luaopen_string(L);
  luaopen_math(L);
  luaopen_debug(L);
  luaopen_loadlib(L);
}
#  endif    // USING_OLD_LUA_API
#endif      // HAVE_LUA_H

namespace Ep128Emu {

#ifdef HAVE_LUA_H

  int LuaScript::luaFunc_AND(lua_State *lst)
  {
    lua_Integer result = (~(lua_Integer(0)));
    int     argCnt = lua_gettop(lst);
    for (int i = 1; i <= argCnt; i++) {
      if (!lua_isnumber(lst, i)) {
        LuaScript&  this_ = *(reinterpret_cast<LuaScript *>(
                                  lua_touserdata(lst, lua_upvalueindex(1))));
        this_.luaError("invalid argument type for AND()");
        return 0;
      }
      result = result & lua_Integer(lua_tointeger(lst, i));
    }
    lua_pushinteger(lst, result);
    return 1;
  }

  int LuaScript::luaFunc_OR(lua_State *lst)
  {
    lua_Integer result = 0;
    int     argCnt = lua_gettop(lst);
    for (int i = 1; i <= argCnt; i++) {
      if (!lua_isnumber(lst, i)) {
        LuaScript&  this_ = *(reinterpret_cast<LuaScript *>(
                                  lua_touserdata(lst, lua_upvalueindex(1))));
        this_.luaError("invalid argument type for OR()");
        return 0;
      }
      result = result | lua_Integer(lua_tointeger(lst, i));
    }
    lua_pushinteger(lst, result);
    return 1;
  }

  int LuaScript::luaFunc_XOR(lua_State *lst)
  {
    lua_Integer result = 0;
    int     argCnt = lua_gettop(lst);
    for (int i = 1; i <= argCnt; i++) {
      if (!lua_isnumber(lst, i)) {
        LuaScript&  this_ = *(reinterpret_cast<LuaScript *>(
                                  lua_touserdata(lst, lua_upvalueindex(1))));
        this_.luaError("invalid argument type for XOR()");
        return 0;
      }
      result = result ^ lua_Integer(lua_tointeger(lst, i));
    }
    lua_pushinteger(lst, result);
    return 1;
  }

  int LuaScript::luaFunc_SHL(lua_State *lst)
  {
    if (lua_gettop(lst) != 2) {
      LuaScript&  this_ = *(reinterpret_cast<LuaScript *>(
                                lua_touserdata(lst, lua_upvalueindex(1))));
      this_.luaError("invalid number of arguments for SHL()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      LuaScript&  this_ = *(reinterpret_cast<LuaScript *>(
                                lua_touserdata(lst, lua_upvalueindex(1))));
      this_.luaError("invalid argument type for SHL()");
      return 0;
    }
    lua_Integer result = lua_tointeger(lst, 1);
    lua_Integer n = lua_tointeger(lst, 2);
    if (n > 0) {
      if (n >= lua_Integer(sizeof(lua_Integer) * 8))
        result = 0;
      else
        result = result << int(n);
    }
    else if (n < 0) {
      n = -n;
      if (n >= lua_Integer(sizeof(lua_Integer) * 8))
        result = 0;
      else
        result = result >> int(n);
    }
    lua_pushinteger(lst, result);
    return 1;
  }

  int LuaScript::luaFunc_SHR(lua_State *lst)
  {
    if (lua_gettop(lst) != 2) {
      LuaScript&  this_ = *(reinterpret_cast<LuaScript *>(
                                lua_touserdata(lst, lua_upvalueindex(1))));
      this_.luaError("invalid number of arguments for SHR()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      LuaScript&  this_ = *(reinterpret_cast<LuaScript *>(
                                lua_touserdata(lst, lua_upvalueindex(1))));
      this_.luaError("invalid argument type for SHR()");
      return 0;
    }
    lua_Integer result = lua_tointeger(lst, 1);
    lua_Integer n = lua_tointeger(lst, 2);
    if (n > 0) {
      if (n >= lua_Integer(sizeof(lua_Integer) * 8))
        result = 0;
      else
        result = result >> int(n);
    }
    else if (n < 0) {
      n = -n;
      if (n >= lua_Integer(sizeof(lua_Integer) * 8))
        result = 0;
      else
        result = result << int(n);
    }
    lua_pushinteger(lst, result);
    return 1;
  }

  int LuaScript::luaFunc_setBreakPoint(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 3) {
      this_.luaError("invalid number of arguments for setBreakPoint()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) &&
          lua_isnumber(lst, 2) &&
          lua_isnumber(lst, 3))) {
      this_.luaError("invalid argument type for setBreakPoint()");
      return 0;
    }
    try {
      int       bpType = int(lua_tointeger(lst, 1));
      uint32_t  bpAddr = uint32_t(lua_tointeger(lst, 2));
      int       bpPriority = int(lua_tointeger(lst, 3));
      bool      ioFlag = ((unsigned int) (bpType & (~(int(0x18)))) >= 5U);
      bool      addr22BitFlag = bool((uint32_t(bpType) & 8U)
                                     | (bpAddr & 0xFFFF0000U));
      uint8_t   bpSegment = 0x00;
      if (ioFlag) {
        if (bpType & (~(int(7))))
          throw Exception("invalid breakpoint type");
        bpAddr = bpAddr & 0xFFU;
      }
      else {
        if ((bpType & 0x17) == 0x00 || (bpType & 0x17) == 0x03)
          bpType = bpType | 0x07;
        if (addr22BitFlag) {
          bpSegment = uint8_t((bpAddr >> 14) & 0xFFU);
          bpAddr = bpAddr & 0x3FFFU;
        }
      }
      BreakPoint  bp(ioFlag, addr22BitFlag, bool(bpType & 1), bool(bpType & 2),
                     bool(bpType & 4), bool(bpType & 16),
                     bpSegment, uint16_t(bpAddr), bpPriority);
      this_.vm.setBreakPoint(bp, (bpPriority >= 0));
    }
    catch (std::exception& e) {
      this_.luaError(e.what());
      return 0;
    }
    return 0;
  }

  int LuaScript::luaFunc_clearBreakPoints(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for clearBreakPoints()");
      return 0;
    }
    this_.vm.clearBreakPoints();
    return 0;
  }

  int LuaScript::luaFunc_getMemoryPage(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for getMemoryPage()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for getMemoryPage()");
      return 0;
    }
    int     n = this_.vm.getMemoryPage(int(lua_tointeger(lst, 1) & 3));
    lua_pushinteger(lst, lua_Integer(n));
    return 1;
  }

  int LuaScript::luaFunc_readMemory(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for readMemory()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for readMemory()");
      return 0;
    }
    uint8_t n =
        this_.vm.readMemory(uint32_t(lua_tointeger(lst, 1) & 0xFFFF), true);
    lua_pushinteger(lst, lua_Integer(n));
    return 1;
  }

  int LuaScript::luaFunc_writeMemory(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 2) {
      this_.luaError("invalid number of arguments for writeMemory()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      this_.luaError("invalid argument type for writeMemory()");
      return 0;
    }
    this_.vm.writeMemory(uint32_t(lua_tointeger(lst, 1) & 0xFFFF),
                         uint8_t(lua_tointeger(lst, 2) & 0xFF), true);
    return 0;
  }

  int LuaScript::luaFunc_readMemoryRaw(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for readMemoryRaw()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for readMemoryRaw()");
      return 0;
    }
    uint8_t n =
        this_.vm.readMemory(uint32_t(lua_tointeger(lst, 1) & 0x3FFFFF), false);
    lua_pushinteger(lst, lua_Integer(n));
    return 1;
  }

  int LuaScript::luaFunc_writeMemoryRaw(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 2) {
      this_.luaError("invalid number of arguments for writeMemoryRaw()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      this_.luaError("invalid argument type for writeMemoryRaw()");
      return 0;
    }
    this_.vm.writeMemory(uint32_t(lua_tointeger(lst, 1) & 0x3FFFFF),
                         uint8_t(lua_tointeger(lst, 2) & 0xFF), false);
    return 0;
  }

  int LuaScript::luaFunc_readWord(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for readWord()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for readWord()");
      return 0;
    }
    uint32_t  addr = uint32_t(lua_tointeger(lst, 1) & 0xFFFF);
    uint16_t  n = this_.vm.readMemory(addr, true);
    n = n | (uint16_t(this_.vm.readMemory(addr + 1U, true)) << 8);
    lua_pushinteger(lst, lua_Integer(n));
    return 1;
  }

  int LuaScript::luaFunc_writeWord(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 2) {
      this_.luaError("invalid number of arguments for writeWord()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      this_.luaError("invalid argument type for writeWord()");
      return 0;
    }
    uint32_t  addr = uint32_t(lua_tointeger(lst, 1) & 0xFFFF);
    uint16_t  n = uint16_t(lua_tointeger(lst, 2) & 0xFFFF);
    this_.vm.writeMemory(addr, uint8_t(n & 0xFF), true);
    this_.vm.writeMemory(addr + 1U, uint8_t(n >> 8), true);
    return 0;
  }

  int LuaScript::luaFunc_readWordRaw(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for readWordRaw()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for readWordRaw()");
      return 0;
    }
    uint32_t  addr = uint32_t(lua_tointeger(lst, 1) & 0x3FFFFF);
    uint16_t  n = this_.vm.readMemory(addr, false);
    n = n | (uint16_t(this_.vm.readMemory(addr + 1U, false)) << 8);
    lua_pushinteger(lst, lua_Integer(n));
    return 1;
  }

  int LuaScript::luaFunc_writeWordRaw(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 2) {
      this_.luaError("invalid number of arguments for writeWordRaw()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      this_.luaError("invalid argument type for writeWordRaw()");
      return 0;
    }
    uint32_t  addr = uint32_t(lua_tointeger(lst, 1) & 0x3FFFFF);
    uint16_t  n = uint16_t(lua_tointeger(lst, 2) & 0xFFFF);
    this_.vm.writeMemory(addr, uint8_t(n & 0xFF), false);
    this_.vm.writeMemory(addr + 1U, uint8_t(n >> 8), false);
    return 0;
  }

  int LuaScript::luaFunc_writeROM(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 2) {
      this_.luaError("invalid number of arguments for writeROM()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      this_.luaError("invalid argument type for writeROM()");
      return 0;
    }
    this_.vm.writeROM(uint32_t(lua_tointeger(lst, 1) & 0x3FFFFF),
                      uint8_t(lua_tointeger(lst, 2) & 0xFF));
    return 0;
  }

  int LuaScript::luaFunc_writeWordROM(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 2) {
      this_.luaError("invalid number of arguments for writeWordROM()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      this_.luaError("invalid argument type for writeWordROM()");
      return 0;
    }
    uint32_t  addr = uint32_t(lua_tointeger(lst, 1) & 0x3FFFFF);
    uint16_t  n = uint16_t(lua_tointeger(lst, 2) & 0xFFFF);
    this_.vm.writeROM(addr, uint8_t(n & 0xFF));
    this_.vm.writeROM(addr + 1U, uint8_t(n >> 8));
    return 0;
  }

  int LuaScript::luaFunc_readIOPort(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for readIOPort()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for readIOPort()");
      return 0;
    }
    uint8_t n = this_.vm.readIOPort(uint16_t(lua_tointeger(lst, 1) & 0xFFFF));
    lua_pushinteger(lst, lua_Integer(n));
    return 1;
  }

  int LuaScript::luaFunc_writeIOPort(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 2) {
      this_.luaError("invalid number of arguments for writeIOPort()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
      this_.luaError("invalid argument type for writeIOPort()");
      return 0;
    }
    this_.vm.writeIOPort(uint16_t(lua_tointeger(lst, 1) & 0xFFFF),
                         uint8_t(lua_tointeger(lst, 2) & 0xFF));
    return 0;
  }

  int LuaScript::luaFunc_getPC(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getPC()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.vm.getProgramCounter()));
    return 1;
  }

  int LuaScript::luaFunc_getA(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getA()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.AF.B.h));
    return 1;
  }

  int LuaScript::luaFunc_getF(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getF()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.AF.B.l));
    return 1;
  }

  int LuaScript::luaFunc_getAF(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getAF()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.AF.W));
    return 1;
  }

  int LuaScript::luaFunc_getB(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getB()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.BC.B.h));
    return 1;
  }

  int LuaScript::luaFunc_getC(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getC()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.BC.B.l));
    return 1;
  }

  int LuaScript::luaFunc_getBC(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getBC()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.BC.W));
    return 1;
  }

  int LuaScript::luaFunc_getD(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getD()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.DE.B.h));
    return 1;
  }

  int LuaScript::luaFunc_getE(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getE()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.DE.B.l));
    return 1;
  }

  int LuaScript::luaFunc_getDE(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getDE()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.DE.W));
    return 1;
  }

  int LuaScript::luaFunc_getH(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getH()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.HL.B.h));
    return 1;
  }

  int LuaScript::luaFunc_getL(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getL()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.HL.B.l));
    return 1;
  }

  int LuaScript::luaFunc_getHL(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getHL()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.HL.W));
    return 1;
  }

  int LuaScript::luaFunc_getAF_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getAF_()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.altAF.W));
    return 1;
  }

  int LuaScript::luaFunc_getBC_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getBC_()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.altBC.W));
    return 1;
  }

  int LuaScript::luaFunc_getDE_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getDE_()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.altDE.W));
    return 1;
  }

  int LuaScript::luaFunc_getHL_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getHL_()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.altHL.W));
    return 1;
  }

  int LuaScript::luaFunc_getSP(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getSP()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.SP.W));
    return 1;
  }

  int LuaScript::luaFunc_getIX(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getIX()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.IX.W));
    return 1;
  }

  int LuaScript::luaFunc_getIY(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getIY()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.IY.W));
    return 1;
  }

  int LuaScript::luaFunc_getIM(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getIM()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.IM));
    return 1;
  }

  int LuaScript::luaFunc_getI(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getI()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.I));
    return 1;
  }

  int LuaScript::luaFunc_getR(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getR()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(this_.z80Registers.RBit7
                                     | (this_.z80Registers.R & 0x7F)));
    return 1;
  }

  int LuaScript::luaFunc_getIFF1(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getIFF1()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(bool(this_.z80Registers.IFF1)));
    return 1;
  }

  int LuaScript::luaFunc_getIFF2(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getIFF2()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(bool(this_.z80Registers.IFF2)));
    return 1;
  }

  int LuaScript::luaFunc_setPC(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setPC()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setPC()");
      return 0;
    }
    this_.vm.setProgramCounter(uint16_t(lua_tointeger(lst, 1) & 0xFFFF));
    return 0;
  }

  int LuaScript::luaFunc_setA(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setA()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setA()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.AF.B.h = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setF(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setF()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setF()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.AF.B.l = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setAF(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setAF()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setAF()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.AF.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setB(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setB()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setB()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.BC.B.h = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setC(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setC()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setC()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.BC.B.l = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setBC(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setBC()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setBC()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.BC.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setD(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setD()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setD()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.DE.B.h = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setE(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setE()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setE()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.DE.B.l = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setDE(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setDE()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setDE()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.DE.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setH(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setH()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setH()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.HL.B.h = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setL(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setL()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setL()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.HL.B.l = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setHL(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setHL()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setHL()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.HL.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setAF_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setAF_()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setAF_()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.altAF.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setBC_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setBC_()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setBC_()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.altBC.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setDE_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setDE_()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setDE_()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.altDE.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setHL_(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setHL_()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setHL_()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.altHL.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setSP(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setSP()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setSP()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.SP.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setIX(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setIX()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setIX()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.IX.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setIY(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setIY()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setIY()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.IY.W = Ep128::Z80_WORD(lua_tointeger(lst, 1) & 0xFFFF);
    return 0;
  }

  int LuaScript::luaFunc_setIM(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setIM()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setIM()");
      return 0;
    }
    lua_Integer tmp = lua_Integer(lua_tointeger(lst, 1));
    if (tmp < 0 || tmp > 2) {
      this_.luaError("invalid interrupt mode value for setIM()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.IM = Ep128::Z80_BYTE(tmp);
    return 0;
  }

  int LuaScript::luaFunc_setI(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setI()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setI()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.I = Ep128::Z80_BYTE(lua_tointeger(lst, 1) & 0xFF);
    return 0;
  }

  int LuaScript::luaFunc_setR(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setR()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setR()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    uint8_t tmp = uint8_t(lua_tointeger(lst, 1) & 0xFF);
    r.RBit7 = tmp & 0x80;
    r.R = tmp & 0x7F;
    return 0;
  }

  int LuaScript::luaFunc_setIFF1(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setIFF1()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setIFF1()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.IFF1 = Ep128::Z80_BYTE(bool(lua_tointeger(lst, 1)));
    return 0;
  }

  int LuaScript::luaFunc_setIFF2(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 1) {
      this_.luaError("invalid number of arguments for setIFF2()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for setIFF2()");
      return 0;
    }
    Ep128::Z80_REGISTERS& r = this_.vm.getZ80Registers();
    r.IFF2 = Ep128::Z80_BYTE(bool(lua_tointeger(lst, 1)));
    return 0;
  }

  int LuaScript::luaFunc_getNextOpcodeAddr(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    int     argCnt = lua_gettop(lst);
    if (argCnt != 1 && argCnt != 2) {
      this_.luaError("invalid number of arguments for getNextOpcodeAddr()");
      return 0;
    }
    if (!lua_isnumber(lst, 1)) {
      this_.luaError("invalid argument type for getNextOpcodeAddr()");
      return 0;
    }
    uint32_t  addr = uint32_t(lua_tointeger(lst, 1) & 0x003FFFFF);
    bool      cpuAddressMode = true;
    if (argCnt > 1) {
      if (!lua_isboolean(lst, 2)) {
        this_.luaError("invalid argument type for getNextOpcodeAddr()");
        return 0;
      }
      cpuAddressMode = bool(lua_toboolean(lst, 2));
    }
    uint32_t  nxtAddr = Ep128::Z80Disassembler::getNextInstructionAddr(
                            this_.vm, addr, cpuAddressMode);
    lua_pushinteger(lst, lua_Integer(nxtAddr));
    return 1;
  }

  int LuaScript::luaFunc_getVideoPosition(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 0) {
      this_.luaError("invalid number of arguments for getVideoPosition()");
      return 0;
    }
    int     tmpX = 0;
    int     tmpY = 0;
    this_.vm.getVideoPosition(tmpX, tmpY);
    lua_pushinteger(lst, lua_Integer(tmpX));
    lua_pushinteger(lst, lua_Integer(tmpY));
    return 2;
  }

  int LuaScript::luaFunc_getRawAddress(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    int       argCnt = lua_gettop(lst);
    uint32_t  addr = 0U;
    if (argCnt == 1) {
      if (!lua_isnumber(lst, 1)) {
        this_.luaError("invalid argument type for getRawAddress()");
        return 0;
      }
      addr = uint32_t(lua_tointeger(lst, 1) & 0xFFFF);
      addr = (addr & 0x3FFFU)
             | (uint32_t(this_.vm.getMemoryPage(int(addr >> 14))) << 14);
    }
    else if (argCnt == 2) {
      if (!(lua_isnumber(lst, 1) && lua_isnumber(lst, 2))) {
        this_.luaError("invalid argument type for getRawAddress()");
        return 0;
      }
      addr = (uint32_t(lua_tointeger(lst, 1) & 0xFF) << 14)
             | uint32_t(lua_tointeger(lst, 2) & 0x3FFF);
    }
    else {
      this_.luaError("invalid number of arguments for getRawAddress()");
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(addr));
    return 1;
  }

  int LuaScript::luaFunc_loadMemory(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    int     argCnt = lua_gettop(lst);
    if (argCnt != 4 && argCnt != 5) {
      this_.luaError("invalid number of arguments for loadMemory()");
      return 0;
    }
    if (!(lua_isstring(lst, 1) && lua_isboolean(lst, 2) &&
          lua_isboolean(lst, 3) && lua_isnumber(lst, 4))) {
      this_.luaError("invalid argument type for loadMemory()");
      return 0;
    }
    uint32_t  endAddr = 0xFFFFFFFFU;
    if (argCnt > 4) {
      if (!lua_isnumber(lst, 5)) {
        this_.luaError("invalid argument type for loadMemory()");
        return 0;
      }
      endAddr = uint32_t(lua_tointeger(lst, 5) & 0x003FFFFF);
    }
    size_t  nBytes = 0;
    try {
      nBytes = this_.vm.loadMemory(lua_tolstring(lst, 1, (size_t *) 0),
                                   false,
                                   bool(lua_toboolean(lst, 2)),
                                   bool(lua_toboolean(lst, 3)),
                                   uint32_t(lua_tointeger(lst, 4) & 0x003FFFFF),
                                   endAddr);
    }
    catch (std::exception& e) {
      this_.luaError(e.what());
      return 0;
    }
    lua_pushinteger(lst, lua_Integer(nBytes));
    return 1;
  }

  int LuaScript::luaFunc_saveMemory(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 5) {
      this_.luaError("invalid number of arguments for saveMemory()");
      return 0;
    }
    if (!(lua_isstring(lst, 1) && lua_isboolean(lst, 2) &&
          lua_isboolean(lst, 3) && lua_isnumber(lst, 4) &&
          lua_isnumber(lst, 5))) {
      this_.luaError("invalid argument type for saveMemory()");
      return 0;
    }
    try {
      this_.vm.saveMemory(lua_tolstring(lst, 1, (size_t *) 0),
                          bool(lua_toboolean(lst, 2)),
                          bool(lua_toboolean(lst, 3)),
                          uint32_t(lua_tointeger(lst, 4) & 0x003FFFFF),
                          uint32_t(lua_tointeger(lst, 5) & 0x003FFFFF));
    }
    catch (std::exception& e) {
      this_.luaError(e.what());
      return 0;
    }
    return 0;
  }

  int LuaScript::luaFunc_loadROMSegment(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    if (lua_gettop(lst) != 3) {
      this_.luaError("invalid number of arguments for loadROMSegment()");
      return 0;
    }
    if (!(lua_isnumber(lst, 1) && lua_isstring(lst, 2) &&
          lua_isnumber(lst, 3))) {
      this_.luaError("invalid argument type for loadROMSegment()");
      return 0;
    }
    try {
      this_.vm.loadROMSegment(uint8_t(lua_tointeger(lst, 1) & 0xFF),
                              lua_tolstring(lst, 2, (size_t *) 0),
                              uint32_t(lua_tointeger(lst, 3) & 0x7FFFFFFF));
    }
    catch (std::exception& e) {
      this_.luaError(e.what());
      return 0;
    }
    return 0;
  }

  int LuaScript::luaFunc_mprint(lua_State *lst)
  {
    LuaScript&  this_ =
        *(reinterpret_cast<LuaScript *>(lua_touserdata(lst,
                                                       lua_upvalueindex(1))));
    try {
      std::string buf;
      int     argCnt = lua_gettop(lst);
      for (int i = 1; i <= argCnt; i++) {
        if (!lua_isstring(lst, i))
          throw Exception("invalid argument type for mprint()");
        const char  *s = lua_tolstring(lst, i, (size_t *) 0);
        if (s)
          buf += s;
      }
      this_.messageCallback(buf.c_str());
    }
    catch (std::exception& e) {
      this_.luaError(e.what());
      return 0;
    }
    return 0;
  }

#endif  // HAVE_LUA_H

  // --------------------------------------------------------------------------

  LuaScript::LuaScript(VirtualMachine& vm_)
    : vm(vm_),
      luaState((lua_State *) 0),
      z80Registers(((const VirtualMachine *) &vm_)->getZ80Registers()),
      errorMessage((char *) 0),
      haveBreakPointCallback(false)
  {
  }

  LuaScript::~LuaScript()
  {
#ifdef HAVE_LUA_H
    if (luaState)
      lua_close(luaState);
#endif  // HAVE_LUA_H
  }

#ifdef HAVE_LUA_H

  void LuaScript::registerLuaFunction(lua_CFunction f, const char *name)
  {
    lua_pushlightuserdata(luaState, (void *) this);
    lua_pushcclosure(luaState, f, 1);
    lua_setglobal(luaState, name);
  }

  bool LuaScript::runBreakPointCallback_(int type, uint16_t addr, uint8_t value)
  {
    lua_pushvalue(luaState, -1);
    lua_pushinteger(luaState, lua_Integer(type));
    lua_pushinteger(luaState, lua_Integer(addr));
    lua_pushinteger(luaState, lua_Integer(value));
    int     err = lua_pcall(luaState, 3, 1, 0);
    if (err != 0) {
      messageCallback(lua_tolstring(luaState, -1, (size_t *) 0));
      closeScript();
      if (errorMessage) {
        const char  *msg = errorMessage;
        errorMessage = (char *) 0;
        errorCallback(msg);
      }
      else if (err == LUA_ERRRUN)
        errorCallback("runtime error while running Lua script");
      else if (err == LUA_ERRMEM)
        errorCallback("memory allocation failure while running Lua script");
      else if (err == LUA_ERRERR)
        errorCallback("error while running Lua error handler");
      else
        errorCallback("error running Lua script");
      return true;
    }
    if (!lua_isboolean(luaState, -1)) {
      closeScript();
      errorCallback("Lua breakpoint function should return a boolean");
      return true;
    }
    bool    retval = bool(lua_toboolean(luaState, -1));
    lua_pop(luaState, 1);
    return retval;
  }

  void LuaScript::luaError(const char *msg)
  {
    if (!msg)
      msg = "unknown error in Lua script";
    errorMessage = msg;
    (void) luaL_error(luaState, "%s", msg);
  }

#endif  // HAVE_LUA_H

  void LuaScript::loadScript(const char *s)
  {
    closeScript();
    if (s == (char *) 0 || s[0] == '\0')
      return;
#ifdef HAVE_LUA_H
#  ifdef USING_OLD_LUA_API
    luaState = lua_open();
#  else
    luaState = luaL_newstate();
#  endif
    if (!luaState) {
      errorCallback("error allocating Lua state");
      return;
    }
    luaL_openlibs(luaState);
    int     err = luaL_loadbuffer(luaState, s, std::strlen(s), "");
    if (err != 0) {
      messageCallback(lua_tolstring(luaState, -1, (size_t *) 0));
      closeScript();
      if (err == LUA_ERRSYNTAX)
        errorCallback("syntax error in Lua script");
      else if (err == LUA_ERRMEM)
        errorCallback("memory allocation failure while loading Lua script");
      else
        errorCallback("error loading Lua script");
      return;
    }
    registerLuaFunction(&luaFunc_AND, "AND");
    registerLuaFunction(&luaFunc_OR, "OR");
    registerLuaFunction(&luaFunc_XOR, "XOR");
    registerLuaFunction(&luaFunc_SHL, "SHL");
    registerLuaFunction(&luaFunc_SHR, "SHR");
    registerLuaFunction(&luaFunc_setBreakPoint, "setBreakPoint");
    registerLuaFunction(&luaFunc_clearBreakPoints, "clearBreakPoints");
    registerLuaFunction(&luaFunc_getMemoryPage, "getMemoryPage");
    registerLuaFunction(&luaFunc_readMemory, "readMemory");
    registerLuaFunction(&luaFunc_writeMemory, "writeMemory");
    registerLuaFunction(&luaFunc_readMemoryRaw, "readMemoryRaw");
    registerLuaFunction(&luaFunc_writeMemoryRaw, "writeMemoryRaw");
    registerLuaFunction(&luaFunc_readWord, "readWord");
    registerLuaFunction(&luaFunc_writeWord, "writeWord");
    registerLuaFunction(&luaFunc_readWordRaw, "readWordRaw");
    registerLuaFunction(&luaFunc_writeWordRaw, "writeWordRaw");
    registerLuaFunction(&luaFunc_writeROM, "writeROM");
    registerLuaFunction(&luaFunc_writeWordROM, "writeWordROM");
    registerLuaFunction(&luaFunc_readIOPort, "readIOPort");
    registerLuaFunction(&luaFunc_writeIOPort, "writeIOPort");
    registerLuaFunction(&luaFunc_getPC, "getPC");
    registerLuaFunction(&luaFunc_getA, "getA");
    registerLuaFunction(&luaFunc_getF, "getF");
    registerLuaFunction(&luaFunc_getAF, "getAF");
    registerLuaFunction(&luaFunc_getB, "getB");
    registerLuaFunction(&luaFunc_getC, "getC");
    registerLuaFunction(&luaFunc_getBC, "getBC");
    registerLuaFunction(&luaFunc_getD, "getD");
    registerLuaFunction(&luaFunc_getE, "getE");
    registerLuaFunction(&luaFunc_getDE, "getDE");
    registerLuaFunction(&luaFunc_getH, "getH");
    registerLuaFunction(&luaFunc_getL, "getL");
    registerLuaFunction(&luaFunc_getHL, "getHL");
    registerLuaFunction(&luaFunc_getAF_, "getAF_");
    registerLuaFunction(&luaFunc_getBC_, "getBC_");
    registerLuaFunction(&luaFunc_getDE_, "getDE_");
    registerLuaFunction(&luaFunc_getHL_, "getHL_");
    registerLuaFunction(&luaFunc_getSP, "getSP");
    registerLuaFunction(&luaFunc_getIX, "getIX");
    registerLuaFunction(&luaFunc_getIY, "getIY");
    registerLuaFunction(&luaFunc_getIM, "getIM");
    registerLuaFunction(&luaFunc_getI, "getI");
    registerLuaFunction(&luaFunc_getR, "getR");
    registerLuaFunction(&luaFunc_getIFF1, "getIFF1");
    registerLuaFunction(&luaFunc_getIFF2, "getIFF2");
    registerLuaFunction(&luaFunc_setPC, "setPC");
    registerLuaFunction(&luaFunc_setA, "setA");
    registerLuaFunction(&luaFunc_setF, "setF");
    registerLuaFunction(&luaFunc_setAF, "setAF");
    registerLuaFunction(&luaFunc_setB, "setB");
    registerLuaFunction(&luaFunc_setC, "setC");
    registerLuaFunction(&luaFunc_setBC, "setBC");
    registerLuaFunction(&luaFunc_setD, "setD");
    registerLuaFunction(&luaFunc_setE, "setE");
    registerLuaFunction(&luaFunc_setDE, "setDE");
    registerLuaFunction(&luaFunc_setH, "setH");
    registerLuaFunction(&luaFunc_setL, "setL");
    registerLuaFunction(&luaFunc_setHL, "setHL");
    registerLuaFunction(&luaFunc_setAF_, "setAF_");
    registerLuaFunction(&luaFunc_setBC_, "setBC_");
    registerLuaFunction(&luaFunc_setDE_, "setDE_");
    registerLuaFunction(&luaFunc_setHL_, "setHL_");
    registerLuaFunction(&luaFunc_setSP, "setSP");
    registerLuaFunction(&luaFunc_setIX, "setIX");
    registerLuaFunction(&luaFunc_setIY, "setIY");
    registerLuaFunction(&luaFunc_setIM, "setIM");
    registerLuaFunction(&luaFunc_setI, "setI");
    registerLuaFunction(&luaFunc_setR, "setR");
    registerLuaFunction(&luaFunc_setIFF1, "setIFF1");
    registerLuaFunction(&luaFunc_setIFF2, "setIFF2");
    registerLuaFunction(&luaFunc_getNextOpcodeAddr, "getNextOpcodeAddr");
    registerLuaFunction(&luaFunc_getVideoPosition, "getVideoPosition");
    registerLuaFunction(&luaFunc_getRawAddress, "getRawAddress");
    registerLuaFunction(&luaFunc_loadMemory, "loadMemory");
    registerLuaFunction(&luaFunc_saveMemory, "saveMemory");
    registerLuaFunction(&luaFunc_loadROMSegment, "loadROMSegment");
    registerLuaFunction(&luaFunc_mprint, "mprint");
    err = lua_pcall(luaState, 0, 0, 0);
    if (err != 0) {
      messageCallback(lua_tolstring(luaState, -1, (size_t *) 0));
      closeScript();
      if (errorMessage) {
        const char  *msg = errorMessage;
        errorMessage = (char *) 0;
        errorCallback(msg);
      }
      else if (err == LUA_ERRRUN)
        errorCallback("runtime error while running Lua script");
      else if (err == LUA_ERRMEM)
        errorCallback("memory allocation failure while running Lua script");
      else if (err == LUA_ERRERR)
        errorCallback("error while running Lua error handler");
      else
        errorCallback("error running Lua script");
      return;
    }
    lua_getglobal(luaState, "breakPointCallback");
    if (!lua_isfunction(luaState, -1))
      lua_pop(luaState, 1);
    else
      haveBreakPointCallback = true;
#else
    errorCallback("Lua scripting is not supported by this build of ep128emu");
#endif  // HAVE_LUA_H
  }

  void LuaScript::closeScript()
  {
    haveBreakPointCallback = false;
#ifdef HAVE_LUA_H
    if (luaState) {
      lua_close(luaState);
      luaState = (lua_State *) 0;
    }
#endif  // HAVE_LUA_H
  }

  void LuaScript::errorCallback(const char *msg)
  {
    if (msg == (char *) 0 || msg[0] == '\0')
      msg = "Lua script error";
    throw Exception(msg);
  }

  void LuaScript::messageCallback(const char *msg)
  {
    if (!msg)
      msg = "";
    std::printf("%s\n", msg);
  }

}       // namespace Ep128Emu

