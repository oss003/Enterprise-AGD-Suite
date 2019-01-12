
// compressor utility for Enterprise 128 programs
// Copyright (C) 2007-2018 Istvan Varga <istvanv@users.sourceforge.net>
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

#ifndef EPCOMPRESS_DECOMPRESS4_HPP
#define EPCOMPRESS_DECOMPRESS4_HPP

#include "ep128emu.hpp"
#include "compress.hpp"
#include <vector>

namespace Ep128Compress {

  class Decompressor_M4 : public Decompressor {
   private:
    unsigned int  lengthDecodeTable[8 * 2];
    unsigned int  offs1DecodeTable[4 * 2];
    unsigned int  offs2DecodeTable[8 * 2];
    unsigned int  offs3DecodeTable[32 * 2];
    size_t        offs3PrefixSize;
    unsigned char shiftRegister;
    int           shiftRegisterCnt;
    const unsigned char *inputBuffer;
    size_t        inputBufferSize;
    size_t        inputBufferPosition;
    // --------
    unsigned int readBits(size_t nBits);
    unsigned char readLiteralByte();
    // returns LZ match length (1..65535), or zero for literal byte,
    // or length + 0x80000000 for literal sequence
    unsigned int readMatchLength();
    unsigned int readLZMatchParameter(unsigned char slotNum,
                                      const unsigned int *decodeTable);
    void readDecodeTables();
    bool decompressDataBlock(std::vector< unsigned char >& buf);
   public:
    Decompressor_M4();
    virtual ~Decompressor_M4();
    // both functions assume "raw" compressed data block with no start address
    virtual void decompressData(
        std::vector< std::vector< unsigned char > >& outBuf,
        const std::vector< unsigned char >& inBuf);
    virtual void decompressData(
        std::vector< unsigned char >& outBuf,
        const std::vector< unsigned char >& inBuf);
  };

}       // namespace Ep128Compress

#endif  // EPCOMPRESS_DECOMPRESS4_HPP

