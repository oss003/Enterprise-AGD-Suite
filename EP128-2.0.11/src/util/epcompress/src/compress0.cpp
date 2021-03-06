
// compressor utility for Enterprise 128 programs
// Copyright (C) 2007-2017 Istvan Varga <istvanv@users.sourceforge.net>
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
#include "compress.hpp"
#include "comprlib.hpp"
#include "compress0.hpp"

#include <list>
#include <map>

namespace Ep128Compress {

  Compressor_M0::DSearchTable::DSearchTable(size_t minLength, size_t maxLength,
                                            size_t maxOffs)
    : LZSearchTable(minLength, maxLength, maxLength, 0, maxOffs, maxOffs)
  {
  }

  Compressor_M0::DSearchTable::~DSearchTable()
  {
  }

  void Compressor_M0::DSearchTable::findMatches(const unsigned char *buf,
                                                size_t bufSize)
  {
    if (bufSize < 1) {
      throw Ep128Emu::Exception("Compressor_M0::DSearchTable::DSearchTable(): "
                                "zero input buffer size");
    }
    // find matches with delta value (offset range: 1..4, delta range: -64..64)
    seqDiffTable.clear();
    maxSeqLenTable.clear();
    seqDiffTable.resize(4);
    maxSeqLenTable.resize(4);
    for (size_t i = 0; i < 4; i++) {
      seqDiffTable[i].resize(bufSize, 0x00);
      maxSeqLenTable[i].resize(bufSize, 0);
    }
    for (size_t i = Compressor_M0::minRepeatDist;
         (i + Compressor_M0::minRepeatLen) <= bufSize;
         i++) {
      for (size_t j = 1; j <= 4; j++) {
        if (i >= j) {
          unsigned char d = (buf[i] - buf[i - j]) & 0xFF;
          if (d != 0x00 && (d >= 0xC0 || d <= 0x40)) {
            size_t  k = i;
            size_t  l = i - j;
            while (k < bufSize && (k - i) < Compressor_M0::maxRepeatLen &&
                   buf[k] == ((buf[l] + d) & 0xFF)) {
              k++;
              l++;
            }
            k = k - i;
            if (k >= Compressor_M0::minRepeatLen) {
              seqDiffTable[j - 1][i] = d;
              maxSeqLenTable[j - 1][i] = (unsigned short) k;
            }
          }
        }
      }
    }
    LZSearchTable::findMatches(buf, 0, bufSize);
  }

  // --------------------------------------------------------------------------

  void Compressor_M0::huffmanCompatibilityHack(
      unsigned int *encodeTable, const unsigned int *symbolCnts,
      size_t nSymbols, bool reverseBits)
  {
    unsigned int  sizeCounts[20];
    unsigned int  sizeCodes[20];
    for (size_t i = 0; i <= 16; i++)
      sizeCounts[i] = 0U;
    for (size_t i = 0; i < nSymbols; i++) {
      unsigned int  symLen = encodeTable[i] >> 24;
      encodeTable[i] = symLen;
      sizeCounts[symLen] = sizeCounts[symLen] + 1U;
    }
    sizeCounts[0] = 0U;
    sizeCodes[0] = 0U;
    for (unsigned int i = 1U; i <= 15U; i++) {
      sizeCodes[i] = (sizeCodes[i - 1] + sizeCounts[i - 1]) << 1;
      while (EP128EMU_UNLIKELY(sizeCounts[i] > 255U)) {
        // FIXME: this code is inefficient, but it runs rarely
        unsigned int  minCnt = 0xFFFFFFFFU;
        unsigned int  c = 0U;
        for (size_t j = 0; j < nSymbols; j++) {
          if (encodeTable[j] == i) {
            if (symbolCnts[j] < minCnt) {
              minCnt = symbolCnts[j];
              c = (unsigned int) j;
            }
          }
        }
        encodeTable[c] = i + 1U;
        sizeCounts[i + 1] = sizeCounts[i + 1] + 1U;
        sizeCounts[i] = sizeCounts[i] - 1U;
      }
    }
    for (size_t i = 0; i < nSymbols; i++) {
      unsigned int  nBits = encodeTable[i];
      if (nBits) {
        unsigned int  huffCode = sizeCodes[nBits];
        sizeCodes[nBits]++;
        if (reverseBits) {
          // Deflate format stores Huffman codes in most significant bit first
          // order, but everything else is least significant bit first
          huffCode = ((huffCode & 0x00FFU) << 8) | ((huffCode & 0xFF00U) >> 8);
          huffCode = ((huffCode & 0x0F0FU) << 4) | ((huffCode & 0xF0F0U) >> 4);
          huffCode = ((huffCode & 0x3333U) << 2) | ((huffCode & 0xCCCCU) >> 2);
          huffCode = ((huffCode & 0x5555U) << 1) | ((huffCode & 0xAAAAU) >> 1);
          huffCode = huffCode >> (16U - nBits);
        }
        encodeTable[i] = (nBits << 24) | huffCode;
      }
    }
  }

  static void writeGammaCode(std::vector< unsigned int >& outBuf, size_t n,
                             unsigned int flagBits = 0U)
  {
    unsigned int  c = flagBits;
    unsigned char nBits = 1;
    while (n > 1) {
      c = c | ((2U | ((unsigned int) n & 1U)) << nBits);
      n = n >> 1;
      nBits = nBits + 2;
    }
    outBuf.push_back(((unsigned int) nBits << 24) | c);
  }

  void Compressor_M0::calculateHuffmanEncoding(
      std::vector< unsigned int >& ioBuf)
  {
    size_t  maxLen = 16;
    for (int h = 0; h < 2; h++) {
      HuffmanEncoder& huffmanEncoder =
          (h == 0 ? huffmanEncoder1 : huffmanEncoder2);
      unsigned int  *symbolCnts = (h == 0 ? symbolCntTable1 : symbolCntTable2);
      unsigned int  *encodeTable = (h == 0 ? encodeTable1 : encodeTable2);
      size_t  nSymbols = (h == 0 ? 324 : 28);
      huffmanEncoder.clear();
      for (size_t i = 0; i < nSymbols; i++) {
        for (unsigned int j = symbolCnts[i]; j-- > 0U; )
          huffmanEncoder.addSymbol((unsigned int) i);
      }
      // create optimal encoding table
      huffmanEncoder.updateTables(false, maxLen);
      for (unsigned int c = 0U; c < (unsigned int) nSymbols; c++) {
        encodeTable[c] = 0U;
        if (huffmanEncoder.getSymbolSize(c) <= maxLen)
          encodeTable[c] = huffmanEncoder.encodeSymbol(c);
      }
      // b31..b29:
      //   000: not Huffman encoded symbol
      //   100: Huffman decode table 1
      //   101: Huffman decode table 2
      //   110: Huffman encoded symbol 0x000..0x0FF
      //   111: Huffman encoded symbol 0x100..0x1FF
      // b23..b16:
      //   lower 8 bits of unencoded symbol value
      unsigned int  hdrFlagBits = (h == 0 ? 0x80000000U : 0xA0000000U);
      // create decode table to be written to the output buffer
      size_t  savedBufSize = ioBuf.size();
      ioBuf.push_back(0x01000001U);
      for (unsigned int l = 1U; l <= 16U; l++) {
        size_t  sizeCnt = 0;
        for (unsigned int c = 0U; c < (unsigned int) nSymbols; c++)
          sizeCnt += size_t((encodeTable[c] >> 24) == l);
        if (EP128EMU_UNLIKELY(sizeCnt >= 256)) {
          // compatibility hack for new FAST_HUFFMAN decompressor code
          if (EP128EMU_UNLIKELY(l >= 16U)) {
            ioBuf.resize(savedBufSize);
            maxLen--;
            h--;
            break;
          }
          huffmanCompatibilityHack(encodeTable, symbolCnts, nSymbols, false);
          sizeCnt = 255;
        }
        writeGammaCode(ioBuf, sizeCnt + 1, hdrFlagBits);
        unsigned int  prvCode = 0U - 1U;
        for (unsigned int c = 0U; c < (unsigned int) nSymbols; c++) {
          if ((encodeTable[c] >> 24) == l) {
            size_t  d = size_t(c - prvCode);
            prvCode = c;
            writeGammaCode(ioBuf, d, hdrFlagBits);
          }
        }
      }
      if (EP128EMU_UNLIKELY(h < 0))
        continue;                       // try again with shorter maximum length
      maxLen = 16;
      // update symbol lengths and add flag bits to encode table
      for (size_t i = 0; i < nSymbols; i++) {
        symbolCnts[i] = 0U;
        unsigned int  c = (unsigned int) (h == 0 ? i : (i + 0x0180));
        tmpCharBitsTable[c] = 16383;
        if (encodeTable[i]) {
          tmpCharBitsTable[c] = size_t(encodeTable[i] >> 24);
          c = 0xC0000000U | ((c & 0xFFU) << 16) | ((c & 0x0100U) << 21);
          encodeTable[i] = encodeTable[i] | c;
        }
      }
    }
  }

  void Compressor_M0::huffmanEncodeBlock(std::vector< unsigned int >& ioBuf,
                                         const unsigned char *inBuf,
                                         size_t uncompressedBytes)
  {
    if (ioBuf.size() < 4 || uncompressedBytes < 1)
      return;
    size_t  uncompressedSize = uncompressedBytes << 3;
    size_t  compressedSize = 0;
    for (int h = 0; h < 2; h++) {
      size_t  huffTableOffs = 0;
      size_t  huffTableEnd = 0;
      long    huffSizeDiff = 0;
      // calculate compressed size with and without Huffman encoding
      for (size_t i = 4; i < ioBuf.size(); i++) {
        unsigned int  c = ioBuf[i];
        if (!(c & 0x80000000U)) {
          if (!h)
            compressedSize += size_t(c >> 24);
        }
        else if (!(c & 0x40000000U)) {
          if (int((c >> 29) & 1U) == h) {
            huffSizeDiff += long((c >> 24) & 0x1FU);
            if (!huffTableOffs)
              huffTableOffs = i;
            huffTableEnd = i + 1;
          }
        }
        else if (int(!(~c & 0x20800000U)) == h) {
          size_t  n = (h == 0 ? 9 : 5);
          huffSizeDiff += (long((c >> 24) & 0x1FU) - long(n));
          compressedSize += n;
        }
      }
      // if Huffman coding does not reduce the data size, use fixed length codes
      if (huffSizeDiff >= 0L && huffTableOffs) {
        ioBuf[huffTableOffs - 1] = 0x01000000U;
        ioBuf.erase(ioBuf.begin() + huffTableOffs,
                    ioBuf.begin() + huffTableEnd);
        for (size_t i = 4; i < ioBuf.size(); i++) {
          unsigned int  c = ioBuf[i];
          if (c >= 0xC0000000U) {
            unsigned int  c_ = ((c >> 16) & 0xFFU) | ((c >> 21) & 0x0100U);
            c_ = (h == 0 ? c_ : (c_ - 0x0180U));
            if (c_ < 324U)
              ioBuf[i] = (h == 0 ? 0x09000000U : 0x05000000U) | c_;
          }
        }
      }
      else {
        compressedSize = size_t(compressedSize + huffSizeDiff);
      }
    }
    // if the size cannot be reduced, store the data without compression
    if (compressedSize >= uncompressedSize) {
      ioBuf.resize(uncompressedBytes + 4);
      ioBuf[3] = 0x01000000U;
      for (size_t i = 0; i < uncompressedBytes; i++)
        ioBuf[i + 4] = 0x08000000U | (unsigned int) inBuf[i];
    }
    else {
      // clear temporary flag bits from Huffman codes and table data
      for (size_t i = 4; i < ioBuf.size(); i++) {
        ioBuf[i] =
            ioBuf[i] & ((ioBuf[i] & 0x40000000U) ? 0x1F00FFFFU : 0x1FFFFFFFU);
      }
    }
  }

  // --------------------------------------------------------------------------

  void Compressor_M0::initializeLengthCodeTables()
  {
    for (size_t i = minRepeatDist; i <= maxRepeatDist; i++) {
      size_t  lCode = 0;
      size_t  lBits = 0;
      size_t  lValue = i - minRepeatDist;
      if (lValue <= 7) {
        lCode = lValue;
        lValue = 0;
      }
      else {
        while (lValue >= (size_t(1) << lBits))
          lBits++;
        lCode = ((lValue >> (lBits - 3)) & 3) | ((lBits - 2) << 2);
        lBits = lBits - 3;
        lValue = lValue & ((size_t(1) << lBits) - 1);
      }
      size_t  j = (i - minRepeatDist) + minRepeatLen;
      if (j >= minRepeatLen && j <= maxRepeatLen) {
        lengthCodeTable[j] = (unsigned short) (lCode | 0x0180);
        lengthBitsTable[j] = (unsigned char) lBits;
        lengthValueTable[j] = (unsigned int) (lValue | (lBits << 24));
      }
      distanceCodeTable[i] = (unsigned short) (lCode | 0x0100);
      distanceBitsTable[i] = (unsigned char) lBits;
      distanceValueTable[i] = (unsigned int) (lValue | (lBits << 24));
    }
  }

  EP128EMU_INLINE void Compressor_M0::encodeSymbol(
      std::vector< unsigned int >& buf, unsigned int c)
  {
    if (c < 0x0180U) {
      symbolCntTable1[c] = symbolCntTable1[c] + 1U;
      buf.push_back(encodeTable1[c]);
    }
    else {
      c = c & 0x7FU;
      symbolCntTable2[c] = symbolCntTable2[c] + 1U;
      buf.push_back(encodeTable2[c]);
    }
  }

  void Compressor_M0::writeRepeatCode(std::vector< unsigned int >& buf,
                                      size_t d, size_t n)
  {
    if (d > 8) {
      for (size_t i = 0; i < 4; i++) {
        if (d == prvDistances[i]) {
          unsigned int  c = 0x0140U | (unsigned int) i;
          if (tmpCharBitsTable[c] >= (tmpCharBitsTable[distanceCodeTable[d]]
                                      + size_t(distanceBitsTable[d]))) {
            break;
          }
          encodeSymbol(buf, c);
          c = (unsigned int) lengthCodeTable[n];
          encodeSymbol(buf, c);
          if (lengthBitsTable[n] > 0)
            buf.push_back(lengthValueTable[n]);
          return;
        }
      }
      for (size_t i = 3; i > 0; i--)
        prvDistances[i] = prvDistances[i - 1];
      prvDistances[0] = d;
    }
    unsigned int  c = (unsigned int) distanceCodeTable[d];
    encodeSymbol(buf, c);
    if (distanceBitsTable[d] > 0)
      buf.push_back(distanceValueTable[d]);
    c = (unsigned int) lengthCodeTable[n];
    encodeSymbol(buf, c);
    if (lengthBitsTable[n] > 0)
      buf.push_back(lengthValueTable[n]);
  }

  void Compressor_M0::writeSequenceCode(std::vector< unsigned int >& buf,
                                        unsigned char seqDiff,
                                        size_t d, size_t n)
  {
    unsigned int  c = (unsigned int) distanceCodeTable[d] | 0x003CU;
    encodeSymbol(buf, c);
    seqDiff = (seqDiff + 0x40) & 0xFF;
    if (seqDiff > 0x40)
      seqDiff--;
    buf.push_back(0x07000000U | (unsigned int) seqDiff);
    c = (unsigned int) lengthCodeTable[n];
    encodeSymbol(buf, c);
    if (lengthBitsTable[n] > 0)
      buf.push_back(lengthValueTable[n]);
  }

  EP128EMU_INLINE long Compressor_M0::rndBit()
  {
    unsigned int  b = ((lfsrState >> 30) ^ (lfsrState >> 27)) & 1U;
    lfsrState = (lfsrState << 1) | b;
    return long(b);
  }

  void Compressor_M0::optimizeMatches_RND(
      LZMatchParameters *matchTable, BitCountTableEntry *bitCountTable,
      const size_t *lengthBitsTable_, const unsigned char *inBuf,
      size_t offs, size_t nBytes)
  {
    for (size_t i = nBytes; i-- > 0; ) {
      // check literal byte
      long    bestSize = long(tmpCharBitsTable[inBuf[offs + i]])
                         + bitCountTable[i + 1].totalBits;
      size_t  bestLen = 1;
      size_t  bestOffs = 0;
      bool    bestPrvDistFlag = false;
      unsigned char bestSeqDiff = 0x00;
      size_t  minLen =
          (config.minLength > minRepeatLen ? config.minLength : minRepeatLen);
      size_t  maxLen = nBytes - i;
      // check LZ77 matches
      const unsigned int  *matchPtr = searchTable->getMatches(offs + i);
      while (size_t(*matchPtr & 0x03FFU) >= minLen) {
        size_t  len = *matchPtr & 0x03FFU;
        size_t  d = *(matchPtr++) >> 10;
        len = (len < maxLen ? len : maxLen);
        size_t  offsBits = tmpCharBitsTable[distanceCodeTable[d]];
        if (d > 8) {
          // long offset: need to search the previous offsets table
          offsBits += size_t(distanceBitsTable[d]);
          for ( ; len >= minLen; len--) {
            const BitCountTableEntry& nxtMatch = bitCountTable[i + len];
            long    nBits = long(lengthBitsTable_[len]) + nxtMatch.totalBits;
            size_t  offsBits_ = offsBits;
            bool    prvDistFlag = false;
            if (size_t(nxtMatch.prvDistances[0]) == d) {
              if (tmpCharBitsTable[0x0140] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0140];
                prvDistFlag = true;
              }
            }
            else if (size_t(nxtMatch.prvDistances[1]) == d) {
              if (tmpCharBitsTable[0x0141] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0141];
                prvDistFlag = true;
              }
            }
            else if (size_t(nxtMatch.prvDistances[2]) == d) {
              if (tmpCharBitsTable[0x0142] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0142];
                prvDistFlag = true;
              }
            }
            else if (size_t(nxtMatch.prvDistances[3]) == d) {
              if (tmpCharBitsTable[0x0143] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0143];
                prvDistFlag = true;
              }
            }
            nBits += long(offsBits_);
            if ((nBits + rndBit()) <= bestSize) {
              bestSize = nBits;
              bestLen = len;
              bestOffs = d;
              bestPrvDistFlag = prvDistFlag;
            }
          }
        }
        else {
          // short offset
          for ( ; len >= minLen; len--) {
            const BitCountTableEntry& nxtMatch = bitCountTable[i + len];
            long    nBits = long(lengthBitsTable_[len] + offsBits)
                            + nxtMatch.totalBits;
            if ((nBits + rndBit()) <= bestSize) {
              bestSize = nBits;
              bestLen = len;
              bestOffs = d;
            }
          }
        }
      }
      // check matches with delta value
      for (size_t d = 1; d <= 4; d++) {
        size_t  len = searchTable->getSequenceLength(offs + i, d);
        if (len < minLen)
          continue;
        if (d > config.maxOffset)
          break;
        unsigned char seqDiff = searchTable->getSequenceDeltaValue(offs + i, d);
        size_t  offsBits = tmpCharBitsTable[0x013B + d] + 7;
        len = (len < maxLen ? len : maxLen);
        for ( ; len >= minLen; len--) {
          const BitCountTableEntry& nxtMatch = bitCountTable[i + len];
          long    nBits = long(lengthBitsTable_[len] + offsBits)
                          + nxtMatch.totalBits;
          if ((nBits + rndBit()) <= bestSize) {
            bestSize = nBits;
            bestLen = len;
            bestOffs = d;
            bestSeqDiff = seqDiff;
          }
        }
      }
      matchTable[i].d = (unsigned int) bestOffs;
      matchTable[i].len = (unsigned short) bestLen;
      matchTable[i].seqDiff = bestSeqDiff;
      bitCountTable[i] = bitCountTable[i + bestLen];
      bitCountTable[i].totalBits = bestSize;
      if (bestOffs > 8 && !bestPrvDistFlag) {
        for (int nn = 3; nn > 0; nn--) {
          bitCountTable[i].prvDistances[nn] =
              bitCountTable[i].prvDistances[nn - 1];
        }
        bitCountTable[i].prvDistances[0] = (unsigned int) bestOffs;
      }
    }
  }

  void Compressor_M0::optimizeMatches(
      LZMatchParameters *matchTable, BitCountTableEntry *bitCountTable,
      const size_t *lengthBitsTable_, const unsigned char *inBuf,
      size_t offs, size_t nBytes)
  {
    for (size_t i = nBytes; i-- > 0; ) {
      // check literal byte
      long    bestSize = long(tmpCharBitsTable[inBuf[offs + i]])
                         + bitCountTable[i + 1].totalBits;
      size_t  bestLen = 1;
      size_t  bestOffs = 0;
      bool    bestPrvDistFlag = false;
      unsigned char bestSeqDiff = 0x00;
      size_t  minLen =
          (config.minLength > minRepeatLen ? config.minLength : minRepeatLen);
      size_t  maxLen = nBytes - i;
      // check LZ77 matches
      const unsigned int  *matchPtr = searchTable->getMatches(offs + i);
      while (size_t(*matchPtr & 0x03FFU) >= minLen) {
        size_t  len = *matchPtr & 0x03FFU;
        size_t  d = *(matchPtr++) >> 10;
        len = (len < maxLen ? len : maxLen);
        size_t  offsBits = tmpCharBitsTable[distanceCodeTable[d]];
        if (d > 8) {
          // long offset: need to search the previous offsets table
          offsBits += size_t(distanceBitsTable[d]);
          for ( ; len >= minLen; len--) {
            const BitCountTableEntry& nxtMatch = bitCountTable[i + len];
            long    nBits = long(lengthBitsTable_[len]) + nxtMatch.totalBits;
            size_t  offsBits_ = offsBits;
            bool    prvDistFlag = false;
            if (size_t(nxtMatch.prvDistances[0]) == d) {
              if (tmpCharBitsTable[0x0140] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0140];
                prvDistFlag = true;
              }
            }
            else if (size_t(nxtMatch.prvDistances[1]) == d) {
              if (tmpCharBitsTable[0x0141] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0141];
                prvDistFlag = true;
              }
            }
            else if (size_t(nxtMatch.prvDistances[2]) == d) {
              if (tmpCharBitsTable[0x0142] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0142];
                prvDistFlag = true;
              }
            }
            else if (size_t(nxtMatch.prvDistances[3]) == d) {
              if (tmpCharBitsTable[0x0143] < offsBits_) {
                offsBits_ = tmpCharBitsTable[0x0143];
                prvDistFlag = true;
              }
            }
            nBits += long(offsBits_);
            if ((nBits + long(d >= bestOffs)) <= bestSize) {
              bestSize = nBits;
              bestLen = len;
              bestOffs = d;
              bestPrvDistFlag = prvDistFlag;
            }
          }
        }
        else {
          // short offset
          for ( ; len >= minLen; len--) {
            const BitCountTableEntry& nxtMatch = bitCountTable[i + len];
            long    nBits = long(lengthBitsTable_[len] + offsBits)
                            + nxtMatch.totalBits;
            if ((nBits + long(d >= bestOffs)) <= bestSize) {
              bestSize = nBits;
              bestLen = len;
              bestOffs = d;
            }
          }
        }
      }
      // check matches with delta value
      for (size_t d = 1; d <= 4; d++) {
        size_t  len = searchTable->getSequenceLength(offs + i, d);
        if (len < minLen)
          continue;
        if (d > config.maxOffset)
          break;
        unsigned char seqDiff = searchTable->getSequenceDeltaValue(offs + i, d);
        size_t  offsBits = tmpCharBitsTable[0x013B + d] + 7;
        len = (len < maxLen ? len : maxLen);
        for ( ; len >= minLen; len--) {
          const BitCountTableEntry& nxtMatch = bitCountTable[i + len];
          long    nBits = long(lengthBitsTable_[len] + offsBits)
                          + nxtMatch.totalBits;
          if ((nBits + long(d >= bestOffs)) <= bestSize) {
            bestSize = nBits;
            bestLen = len;
            bestOffs = d;
            bestSeqDiff = seqDiff;
          }
        }
      }
      matchTable[i].d = (unsigned int) bestOffs;
      matchTable[i].len = (unsigned short) bestLen;
      matchTable[i].seqDiff = bestSeqDiff;
      bitCountTable[i] = bitCountTable[i + bestLen];
      bitCountTable[i].totalBits = bestSize;
      if (bestOffs > 8 && !bestPrvDistFlag) {
        for (int nn = 3; nn > 0; nn--) {
          bitCountTable[i].prvDistances[nn] =
              bitCountTable[i].prvDistances[nn - 1];
        }
        bitCountTable[i].prvDistances[0] = (unsigned int) bestOffs;
      }
    }
  }

  void Compressor_M0::compressData_(std::vector< unsigned int >& tmpOutBuf,
                                    const std::vector< unsigned char >& inBuf,
                                    size_t offs, size_t nBytes)
  {
    size_t  endPos = offs + nBytes;
    // compress data by searching for repeated byte sequences,
    // and replacing them with length/distance codes
    for (size_t i = 0; i < 4; i++)
      prvDistances[i] = 0;
    std::vector< LZMatchParameters >  matchTable(nBytes);
    {
      std::vector< BitCountTableEntry > bitCountTable(nBytes + 1);
      std::vector< size_t > lengthBitsTable_(maxRepeatLen + 1, 0x7FFF);
      for (size_t i = minRepeatLen; i <= maxRepeatLen; i++) {
        lengthBitsTable_[i] = tmpCharBitsTable[lengthCodeTable[i]]
                              + size_t(lengthBitsTable[i]);
      }
      bitCountTable[nBytes].totalBits = 0L;
      for (size_t i = 0; i < 4; i++)
        bitCountTable[nBytes].prvDistances[i] = 0;
      if (config.splitOptimizationDepth >= 9) {
        optimizeMatches_RND(&(matchTable.front()), &(bitCountTable.front()),
                            &(lengthBitsTable_.front()), &(inBuf.front()),
                            offs, nBytes);
      }
      else {
        optimizeMatches(&(matchTable.front()), &(bitCountTable.front()),
                        &(lengthBitsTable_.front()), &(inBuf.front()),
                        offs, nBytes);
      }
    }
    for (size_t i = offs; i < endPos; ) {
      LZMatchParameters&  tmp = matchTable[i - offs];
      if (tmp.len >= minRepeatLen) {
        if (tmp.seqDiff)
          writeSequenceCode(tmpOutBuf, tmp.seqDiff, tmp.d, tmp.len);
        else
          writeRepeatCode(tmpOutBuf, tmp.d, tmp.len);
        i = i + tmp.len;
      }
      else {
        encodeSymbol(tmpOutBuf, (unsigned int) inBuf[i]);
        i++;
      }
    }
  }

  bool Compressor_M0::compressData(std::vector< unsigned int >& tmpOutBuf,
                                   const std::vector< unsigned char >& inBuf,
                                   unsigned int startAddr, bool isLastBlock,
                                   size_t offs, size_t nBytes)
  {
    // the 'offs' and 'nBytes' parameters allow compressing a buffer
    // as multiple chunks for possibly improved statistical compression
    if (nBytes < 1 || offs >= inBuf.size())
      return true;
    if (nBytes > (inBuf.size() - offs))
      nBytes = inBuf.size() - offs;
    for (unsigned int i = 0U; i < 324U; i++) {
      tmpCharBitsTable[i] = 9;
      symbolCntTable1[i] = 0U;
      encodeTable1[i] = 0x09000000U | i;
    }
    for (unsigned int i = 0U; i < 28U; i++) {
      tmpCharBitsTable[i + 0x0180] = 5;
      symbolCntTable2[i] = 0U;
      encodeTable2[i] = 0x05000000U | i;
    }
    std::vector< uint64_t >     hashTable;
    std::vector< unsigned int > bestBuf;
    std::vector< unsigned int > tmpBuf;
    size_t  bestSize = 0x7FFFFFFF;
    bool    doneFlag = false;
    for (size_t i = 0; i < config.optimizeIterations; i++) {
      if (progressDisplayEnabled) {
        if (!setProgressPercentage(int(progressCnt * uint64_t(100)
                                       / progressMax))) {
          return false;
        }
        progressCnt += nBytes;
      }
      if (doneFlag)     // if the compression cannot be optimized further,
        continue;       // quit the loop earlier
      // write data header (start address, 2's complement of the number
      // of bytes, last block flag and compression enabled flag)
      tmpBuf.clear();
      if (startAddr < 0x80000000U) {
        tmpBuf.push_back(0x10000000U | (unsigned int) (startAddr + offs));
        tmpBuf.push_back(0x10000000U | (unsigned int) (65536 - nBytes));
      }
      else {
        // hack to keep the header size constant
        tmpBuf.push_back(0x08000000U | (unsigned int) ((65536 - nBytes) >> 8));
        tmpBuf.push_back(0x08000000U
                            | (unsigned int) ((65536 - nBytes) & 0xFF));
      }
      tmpBuf.push_back(isLastBlock ? 0x01000001U : 0x01000000U);
      tmpBuf.push_back(0x01000001U);
      if (!i) {
        // Huffman coding is disabled on first pass
        tmpBuf.push_back(0x01000000U);
        tmpBuf.push_back(0x01000000U);
      }
      else {
        // apply statistical compression
        calculateHuffmanEncoding(tmpBuf);
      }
      compressData_(tmpBuf, inBuf, offs, nBytes);
      // calculate compressed size and hash value
      size_t    compressedSize = 0;
      uint64_t  h = 1UL;
      for (size_t j = 0; j < tmpBuf.size(); j++) {
        compressedSize += size_t((tmpBuf[j] >> 24) & 0x1FU);
        h = h ^ uint64_t(tmpBuf[j]);
        h = uint32_t(h) * uint64_t(0xC2B0C3CCUL);
        h = (h ^ (h >> 32)) & 0xFFFFFFFFUL;
      }
      h = h | (uint64_t(compressedSize) << 32);
      if (compressedSize < bestSize) {
        // found a better compression, so save it
        bestSize = compressedSize;
        bestBuf.resize(tmpBuf.size());
        std::memcpy(&(bestBuf.front()), &(tmpBuf.front()),
                    tmpBuf.size() * sizeof(unsigned int));
      }
      for (size_t j = 0; j < hashTable.size(); j++) {
        if (hashTable[j] == h) {
          // if the exact same compressed data was already generated earlier,
          // the remaining optimize iterations can be skipped
          doneFlag = true;
          break;
        }
      }
      if (!doneFlag)
        hashTable.push_back(h);         // save hash value
    }
    // append compressed data to output buffer
    for (size_t i = 0; i < bestBuf.size(); i++)
      tmpOutBuf.push_back(bestBuf[i]);
    return true;
  }

  bool Compressor_M0::compressBlock(SplitOptimizationBlock& tmpBlock,
                                    const std::vector< unsigned char >& inBuf,
                                    unsigned int startAddr, size_t startPos,
                                    size_t nBytes, bool isLastBlock)
  {
    tmpBlock.buf.clear();
    tmpBlock.startPos = startPos;
    tmpBlock.nBytes = nBytes;
    tmpBlock.compressedSize = 0;
    tmpBlock.isLastBlock = ((startPos + nBytes) >= inBuf.size());
    if (tmpBlock.isLastBlock)
      tmpBlock.nBytes = inBuf.size() - startPos;
    if (!compressData(tmpBlock.buf, inBuf, startAddr,
                      (tmpBlock.isLastBlock && isLastBlock),
                      tmpBlock.startPos, tmpBlock.nBytes)) {
      delete searchTable;
      searchTable = (DSearchTable *) 0;
      if (progressDisplayEnabled)
        progressMessage("");
      return false;
    }
    // calculate compressed size
    for (size_t j = 0; j < tmpBlock.buf.size(); j++)
      tmpBlock.compressedSize += size_t((tmpBlock.buf[j] >> 24) & 0x1FU);
    return true;
  }

  void Compressor_M0::packOutputData(const std::vector< unsigned int >& tmpBuf,
                                     bool isLastBlock)
  {
    if (outBuf.size() == 0)
      outBuf.push_back((unsigned char) 0x00);   // reserve space for checksum
    for (size_t i = 0; i < tmpBuf.size(); i++) {
      unsigned int  c = tmpBuf[i];
      unsigned int  nBits = c >> 24;
      c = c & 0x00FFFFFFU;
      for (unsigned int j = nBits; j-- > 0U; ) {
        unsigned int  b = (unsigned int) (bool(c & (1U << j)));
        outputShiftReg = ((outputShiftReg & 0x7F) << 1) | (unsigned char) b;
        if (++outputBitCnt >= 8) {
          outBuf.push_back(outputShiftReg);
          outputShiftReg = 0x00;
          outputBitCnt = 0;
        }
      }
    }
    if (isLastBlock) {
      while (outputBitCnt != 0) {
        outputShiftReg = ((outputShiftReg & 0x7F) << 1);
        if (++outputBitCnt >= 8) {
          outBuf.push_back(outputShiftReg);
          outputShiftReg = 0x00;
          outputBitCnt = 0;
        }
      }
      // calculate checksum
      unsigned char chkSum = 0x00;
      for (size_t i = outBuf.size() - 1; i > 0; i--) {
        unsigned int  tmp = (unsigned int) chkSum ^ (unsigned int) outBuf[i];
        tmp = ((tmp << 1) + ((tmp & 0x80U) >> 7) + 0xC4U) & 0xFFU;
        chkSum = (unsigned char) tmp;
      }
      chkSum = (unsigned char) ((0x180 - 0xC4) >> 1) ^ chkSum;
      outBuf[0] = chkSum;
      outputBitCnt = -1;        // set output buffer closed flag
    }
  }

  Compressor_M0::Compressor_M0(std::vector< unsigned char >& outBuf_,
                               size_t huff1Size, size_t huff1MinCnt,
                               size_t huff2Size, size_t huff2MinCnt)
    : Compressor(outBuf_),
      lengthCodeTable((unsigned short *) 0),
      lengthBitsTable((unsigned char *) 0),
      lengthValueTable((unsigned int *) 0),
      distanceCodeTable((unsigned short *) 0),
      distanceBitsTable((unsigned char *) 0),
      distanceValueTable((unsigned int *) 0),
      tmpCharBitsTable((size_t *) 0),
      searchTable((DSearchTable *) 0),
      outputShiftReg(0x00),
      outputBitCnt(0),
      lfsrState(0x12345678U),
      huffmanEncoder1(huff1Size, huff1MinCnt),
      huffmanEncoder2(huff2Size, huff2MinCnt),
      symbolCntTable1((unsigned int *) 0),
      symbolCntTable2((unsigned int *) 0),
      encodeTable1((unsigned int *) 0),
      encodeTable2((unsigned int *) 0)
  {
  }

  // --------------------------------------------------------------------------

  Compressor_M0::Compressor_M0(std::vector< unsigned char >& outBuf_)
    : Compressor(outBuf_),
      lengthCodeTable((unsigned short *) 0),
      lengthBitsTable((unsigned char *) 0),
      lengthValueTable((unsigned int *) 0),
      distanceCodeTable((unsigned short *) 0),
      distanceBitsTable((unsigned char *) 0),
      distanceValueTable((unsigned int *) 0),
      tmpCharBitsTable((size_t *) 0),
      searchTable((DSearchTable *) 0),
      outputShiftReg(0x00),
      outputBitCnt(0),
      lfsrState(0x12345678U),
      huffmanEncoder1(324, 0),
      huffmanEncoder2(28, 0),
      symbolCntTable1((unsigned int *) 0),
      symbolCntTable2((unsigned int *) 0),
      encodeTable1((unsigned int *) 0),
      encodeTable2((unsigned int *) 0)
  {
    try {
      lengthCodeTable = new unsigned short[maxRepeatLen + maxRepeatDist + 2];
      lengthBitsTable = new unsigned char[maxRepeatLen + maxRepeatDist + 2];
      lengthValueTable = new unsigned int[maxRepeatLen + maxRepeatDist + 706];
      distanceCodeTable = lengthCodeTable + (maxRepeatLen + 1);
      distanceBitsTable = lengthBitsTable + (maxRepeatLen + 1);
      distanceValueTable = lengthValueTable + (maxRepeatLen + 1);
      symbolCntTable1 = distanceValueTable + (maxRepeatDist + 1);
      symbolCntTable2 = symbolCntTable1 + 324;
      encodeTable1 = symbolCntTable2 + 28;
      encodeTable2 = encodeTable1 + 324;
      tmpCharBitsTable = new size_t[384 + 28];
      initializeLengthCodeTables();
      for (size_t i = 0; i < (384 + 28); i++)
        tmpCharBitsTable[i] = 16383;
      for (size_t i = 0; i < 4; i++)
        prvDistances[i] = 0;
    }
    catch (...) {
      if (lengthCodeTable)
        delete[] lengthCodeTable;
      if (lengthBitsTable)
        delete[] lengthBitsTable;
      if (lengthValueTable)
        delete[] lengthValueTable;
      if (tmpCharBitsTable)
        delete[] tmpCharBitsTable;
      throw;
    }
  }

  Compressor_M0::~Compressor_M0()
  {
    delete[] lengthCodeTable;
    delete[] lengthBitsTable;
    delete[] lengthValueTable;
    delete[] tmpCharBitsTable;
    if (searchTable)
      delete searchTable;
  }

  bool Compressor_M0::compressData(const std::vector< unsigned char >& inBuf,
                                   unsigned int startAddr, bool isLastBlock,
                                   bool enableProgressDisplay)
  {
    if (outputBitCnt < 0)
      throw Ep128Emu::Exception("internal error: compressing to closed buffer");
    if (inBuf.size() < 1)
      return true;
    // 0: epcompress -m0, 1: ZLib, 2: ZLib with non-standard extensions
    unsigned char compType = 0;
    if (lengthCodeTable[3] == 257)
      compType = (config.maxOffset <= 32768 ? 1 : 2);
    progressDisplayEnabled = enableProgressDisplay;
    try {
      if (enableProgressDisplay) {
        progressMessage("Compressing data");
        setProgressPercentage(0);
      }
      if (searchTable) {
        delete searchTable;
        searchTable = (DSearchTable *) 0;
      }
      {
        size_t  minLen = (compType == 1 ? 3 : 2);
        minLen = (minLen > config.minLength ? minLen : config.minLength);
        size_t  maxLen = (compType != 0 ? 258 : 256);
        size_t  maxOffs = config.maxOffset;
        maxOffs = (maxOffs < inBuf.size() ? maxOffs : inBuf.size());
        searchTable = new DSearchTable(minLen, maxLen, maxOffs);
        searchTable->findMatches(&(inBuf.front()), inBuf.size());
      }
      if (config.optimizeIterations > 16)
        config.optimizeIterations = 16;
      std::list< SplitOptimizationBlock >   splitPositions;
      SplitOptimizationBlock  tmpBlock;
      size_t  maxBlockSize = 65535 + size_t(compType == 0);
      progressCnt = 0;
      progressMax = inBuf.size() * config.optimizeIterations;
      if (config.blockSize > 0) {
        // force block size specified by the user
        tmpBlock.startPos = 0;
        do {
          tmpBlock.nBytes = config.blockSize;
          if (tmpBlock.nBytes > maxBlockSize)
            tmpBlock.nBytes = maxBlockSize;
          if (!compressBlock(tmpBlock, inBuf, startAddr,
                             tmpBlock.startPos, tmpBlock.nBytes, isLastBlock)) {
            return false;
          }
          splitPositions.push_back(tmpBlock);
          tmpBlock.startPos = tmpBlock.startPos + tmpBlock.nBytes;
        } while (!tmpBlock.isLastBlock);
      }
      else if (config.splitOptimizationDepth < 10) {
        // split large files to improve statistical compression
        std::map< uint64_t, bool >    splitOptimizationCache;
        size_t  splitDepth = config.splitOptimizationDepth;
        size_t  splitCnt = size_t(1) << splitDepth;
        while (((inBuf.size() + splitCnt - 1) / splitCnt) > maxBlockSize) {
          splitDepth++;                 // limit block size to <= 64K
          splitCnt = splitCnt << 1;
        }
        while (splitCnt > inBuf.size()) {
          splitDepth--;                 // avoid zero block size
          splitCnt = splitCnt >> 1;
        }
        {
          size_t  tmp = 0;
          size_t  tmp2 = (inBuf.size() + splitCnt - 1) / splitCnt;
          do {
            tmp++;
            tmp2 = tmp2 << 1;
          } while (tmp < splitDepth && tmp2 <= maxBlockSize);
          progressMax = progressMax * (tmp + size_t(tmp > 1));
        }
        {
          // create initial block list
          size_t  tmp = 0;
          for (size_t startPos = 0; startPos < inBuf.size(); ) {
            tmp = tmp + inBuf.size();
            tmpBlock.nBytes = tmp / splitCnt;
            tmp = tmp % splitCnt;
            if (!compressBlock(tmpBlock, inBuf, startAddr,
                               startPos, tmpBlock.nBytes, isLastBlock)) {
              return false;
            }
            startPos = startPos + tmpBlock.nBytes;
            splitPositions.push_back(tmpBlock);
          }
        }
        while (true) {
          bool    mergeFlag = false;
          std::list< SplitOptimizationBlock >::iterator i_0 =
              splitPositions.begin();
          while (i_0 != splitPositions.end()) {
            std::list< SplitOptimizationBlock >::iterator i_1 = i_0;
            i_1++;
            if (i_1 == splitPositions.end())
              break;
            uint64_t  cacheKey = uint64_t((*i_0).startPos)
                                 | (uint64_t((*i_1).startPos) << 20)
                                 | (uint64_t((*i_1).nBytes) << 40);
            if (splitOptimizationCache.find(cacheKey)
                != splitOptimizationCache.end()) {
              // if this pair of blocks was already tested earlier,
              // skip testing again
              i_0 = i_1;
              continue;
            }
            size_t  nBytes = (*i_0).nBytes + (*i_1).nBytes;
            if (nBytes > maxBlockSize) {
              i_0 = i_1;
              continue;                 // limit block size to <= 64K
            }
            if ((progressCnt + (nBytes * config.optimizeIterations))
                > progressMax) {
              progressMax = progressCnt + (nBytes * config.optimizeIterations);
            }
            if (!compressBlock(tmpBlock, inBuf, startAddr,
                               (*i_0).startPos, nBytes, isLastBlock)) {
              return false;
            }
            if (tmpBlock.compressedSize
                <= ((*i_0).compressedSize + (*i_1).compressedSize)) {
              // splitting does not reduce size, so use merged block
              (*i_0) = tmpBlock;
              i_0 = splitPositions.erase(i_1);
              mergeFlag = true;
            }
            else {
              i_0 = i_1;
              // blocks not merged: remember block positions and sizes, so that
              // the same pair of blocks will not need to be tested again
              splitOptimizationCache[cacheKey] = true;
            }
          }
          if (!mergeFlag)
            break;
        }
      }
      else {
        // block optimization algorithm for -X mode (very expensive)
        size_t  minBlockSize = (compType == 0 ? 64 : 85);
        // calculate total amount of data to be processed
        progressMax = 0;
        for (size_t i = 0; i < inBuf.size(); i = i + minBlockSize) {
          for (size_t j = minBlockSize; j <= maxBlockSize; j += minBlockSize) {
            if ((i % j) != 0)
              continue;
            if ((i + j) >= inBuf.size()) {
              progressMax = progressMax + (inBuf.size() - i);
              break;
            }
            progressMax = progressMax + j;
          }
        }
        progressMax = progressMax * config.optimizeIterations;
        // find optimal path
        size_t  nBlocks = (inBuf.size() + minBlockSize - 1) / minBlockSize;
        std::vector< SplitOptimizationBlock > splitPositions_(nBlocks + 1);
        splitPositions_[nBlocks].buf.clear();
        splitPositions_[nBlocks].startPos = inBuf.size();
        splitPositions_[nBlocks].nBytes = 0;
        splitPositions_[nBlocks].compressedSize = 0;
        splitPositions_[nBlocks].isLastBlock = true;
        for (size_t i = nBlocks; i-- > 0; ) {
          size_t  startPos = i * minBlockSize;
          splitPositions_[i].compressedSize = 0x7FFFFFFF;
          for (size_t j = minBlockSize; j <= maxBlockSize; j += minBlockSize) {
            if ((startPos % j) != 0)
              continue;
            if (!compressBlock(tmpBlock, inBuf, startAddr, startPos, j,
                               isLastBlock)) {
              return false;
            }
            tmpBlock.compressedSize +=
                (splitPositions_[i + (j / minBlockSize)].compressedSize);
            if (tmpBlock.compressedSize <= splitPositions_[i].compressedSize)
              splitPositions_[i] = tmpBlock;
            if (tmpBlock.isLastBlock)
              break;
          }
        }
        for (size_t i = 0; i < nBlocks; ) {
          splitPositions.push_back(splitPositions_[i]);
          i += ((splitPositions_[i].nBytes + minBlockSize - 1) / minBlockSize);
        }
      }
      delete searchTable;
      searchTable = (DSearchTable *) 0;
      std::vector< unsigned int >   outBufTmp;
      std::list< SplitOptimizationBlock >::iterator i_ = splitPositions.begin();
      while (i_ != splitPositions.end()) {
        huffmanEncodeBlock((*i_).buf,
                           &(inBuf.front()) + (*i_).startPos, (*i_).nBytes);
        for (size_t i = 0; i < (*i_).buf.size(); i++)
          outBufTmp.push_back((*i_).buf[i]);
        i_ = splitPositions.erase(i_);
      }
      if (progressDisplayEnabled) {
        setProgressPercentage(100);
        progressMessage("");
      }
      // pack output data
      packOutputData(outBufTmp, isLastBlock);
    }
    catch (...) {
      if (searchTable) {
        delete searchTable;
        searchTable = (DSearchTable *) 0;
      }
      if (progressDisplayEnabled)
        progressMessage("");
      throw;
    }
    return true;
  }

}       // namespace Ep128Compress

