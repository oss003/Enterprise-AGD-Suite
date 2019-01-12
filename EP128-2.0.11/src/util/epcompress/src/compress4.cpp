
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

#include "ep128emu.hpp"
#include "compress.hpp"
#include "comprlib.hpp"
#include "compress4.hpp"

#include <list>
#include <map>

namespace Ep128Compress {

  const size_t Compressor_M4::lengthPrefixSizeTable[lengthNumSlots] = {
    1, 2, 3, 4, 5, 6, 7, 8
  };

  const size_t Compressor_M4::offs3SlotCntTable[4] = {
    4, 8, 16, 32
  };

  // --------------------------------------------------------------------------

  void Compressor_M4::writeRepeatCode(std::vector< unsigned int >& buf,
                                      size_t d, size_t n)
  {
    EncodeTable&  offsEncodeTable =
        (n > 2 ? offs3EncodeTable :
         (n > 1 ? offs2EncodeTable : offs1EncodeTable));
    unsigned int  offsPrefixSize =
        (unsigned int) (n > 2 ? offs3PrefixSize :
                        (n > 1 ? offs2PrefixSize : offs1PrefixSize));
    n = n - minRepeatLen;
    d = d - minRepeatDist;
    unsigned int  slotNum =
        (unsigned int) lengthEncodeTable.getSymbolSlotIndex((unsigned int) n);
    unsigned int  slotSize =
        (unsigned int) lengthEncodeTable.getSlotSize(slotNum);
    slotNum = slotNum + 2U;
    if (!slotSize) {
      buf.push_back((slotNum << 24) | ((1U << slotNum) - 2U));
    }
    else {
      buf.push_back(((slotNum << 24) | (((1U << slotNum) - 2U) << slotSize))
                    + lengthEncodeTable.encodeSymbol((unsigned int) n));
    }
    slotNum =
        (unsigned int) offsEncodeTable.getSymbolSlotIndex((unsigned int) d);
    slotSize = (unsigned int) offsEncodeTable.getSlotSize(slotNum);
    if (!slotSize) {
      buf.push_back((offsPrefixSize << 24) | slotNum);
    }
    else {
      buf.push_back(((offsPrefixSize << 24) | (slotNum << slotSize))
                    + offsEncodeTable.encodeSymbol((unsigned int) d));
    }
  }

  inline size_t Compressor_M4::getRepeatCodeLength(size_t d, size_t n) const
  {
    n = n - minRepeatLen;
    size_t  nBits = lengthEncodeTable.getSymbolSize(n) + 1;
    d = d - minRepeatDist;
    if ((n + minRepeatLen) > 2)
      nBits += offs3EncodeTable.getSymbolSize(d);
    else if ((n + minRepeatLen) > 1)
      nBits += offs2EncodeTable.getSymbolSize(d);
    else
      nBits += offs1EncodeTable.getSymbolSize(d);
    return nBits;
  }

  void Compressor_M4::optimizeMatches_noStats(LZMatchParameters *matchTable,
                                              size_t *bitCountTable,
                                              size_t offs, size_t nBytes)
  {
    // simplified optimal parsing code for the first optimization pass
    // when no symbol size information is available from the statistical
    // compression
    static const size_t matchSize1 = 4; // 6 if offset > 16, 8 if offset > 64
    static const size_t matchSize2 = 6; // 8 if offset > 1024
    static const size_t matchSize3 = 8;
    for (size_t i = nBytes; i-- > 0; ) {
      size_t  bestSize = 0x7FFFFFFF;
      size_t  bestLen = 1;
      size_t  bestOffs = 0;
      const unsigned int  *matchPtr = searchTable->getMatches(offs + i);
      size_t  len = matchPtr[0];        // match length
      if (len > (nBytes - i))
        len = nBytes - i;
      if (len > maxRepeatLen) {
        if (matchPtr[1] > 1024U) {
          // long LZ77 match
          bestSize = bitCountTable[i + len] + matchSize3;
          bestOffs = matchPtr[1] >> 10;
          bestLen = len;
          len = maxRepeatLen;
        }
        else {
          // if a long RLE match is possible, use that
          matchTable[i].d = 1;
          matchTable[i].len = (unsigned int) len;
          bitCountTable[i] = bitCountTable[i + len] + matchSize3;
          continue;
        }
      }
      // otherwise check all possible LZ77 match lengths,
      matchPtr++;
      for ( ; len > 0; len = (*matchPtr & 0x03FFU)) {
        if (len > (nBytes - i))
          len = nBytes - i;
        unsigned int  d = *matchPtr >> 10;
        size_t        nxtLen = *(++matchPtr) & 0x03FFU;
        nxtLen = (nxtLen >= config.minLength ? nxtLen : (config.minLength - 1));
        if (len <= nxtLen)
          continue;                     // ignore match
        if (len >= 3) {
          size_t  minLenM1 = (nxtLen > 2 ? nxtLen : 2);
          do {
            size_t  nBits = bitCountTable[i + len] + matchSize3;
            if (nBits <= bestSize) {
              bestSize = nBits;
              bestOffs = d;
              bestLen = len;
            }
          } while (--len > minLenM1);
          if (nxtLen >= 2)
            continue;
          len = 2;
        }
        // check short match lengths:
        if (len == 2) {                 // 2 bytes
          if (d <= offs2MaxValue) {
            size_t  nBits = bitCountTable[i + 2] + matchSize2
                            + (size_t(d > 1024U) << 1);
            if (nBits <= bestSize) {
              bestSize = nBits;
              bestOffs = d;
              bestLen = 2;
            }
          }
          if (nxtLen >= 1)
            continue;
        }
        if (d <= offs1MaxValue) {       // 1 byte
          size_t  nBits = bitCountTable[i + 1] + matchSize1
                          + ((size_t(d > 16U) + size_t(d > 64U)) << 1);
          if (nBits <= bestSize) {
            bestSize = nBits;
            bestOffs = d;
            bestLen = 1;
          }
        }
      }
      if (bestSize >= (bitCountTable[i + 1] + 8)) {
        // literal byte,
        size_t  nBits = bitCountTable[i + 1] + 9;
        if (nBits <= bestSize) {
          bestSize = nBits;
          bestOffs = 0;
          bestLen = 1;
        }
        for (size_t k = literalSequenceMinLength;
             k <= literalSequenceMaxLength && (i + k) <= nBytes;
             k++) {
          // and all possible literal sequence lengths
          nBits = bitCountTable[i + k] + (k * 8 + literalSequenceMinLength - 1);
          if (nBits > (bestSize + literalSequenceMinLength - 1))
            break;      // quit the loop earlier if the data can be compressed
          if (nBits <= bestSize) {
            bestSize = nBits;
            bestOffs = 0;
            bestLen = k;
          }
        }
      }
      matchTable[i].d = (unsigned int) bestOffs;
      matchTable[i].len = (unsigned int) bestLen;
      bitCountTable[i] = bestSize;
    }
  }

  void Compressor_M4::optimizeMatches(LZMatchParameters *matchTable,
                                      size_t *bitCountTable,
                                      uint64_t *offsSumTable,
                                      size_t offs, size_t nBytes)
  {
    size_t  len1BitsP1 = 16383;
    size_t  len2BitsP1 = 16383;
    if (config.minLength < 3)
      len2BitsP1 = lengthEncodeTable.getSymbolSize(2U - minRepeatLen) + 1;
    if (config.minLength < 2)
      len1BitsP1 = lengthEncodeTable.getSymbolSize(1U - minRepeatLen) + 1;
    for (size_t i = nBytes; i-- > 0; ) {
      size_t  bestSize = 0x7FFFFFFF;
      size_t  bestLen = 1;
      size_t  bestOffs = 0;
      uint64_t  bestOffsSum = uint64_t(0) - uint64_t(1);
      const unsigned int  *matchPtr = searchTable->getMatches(offs + i);
      size_t  len = matchPtr[0];        // match length
      if (len > (nBytes - i))
        len = nBytes - i;
      if (len > maxRepeatLen) {
        if (matchPtr[1] > 1024U) {
          // long LZ77 match
          bestOffs = matchPtr[1] >> 10;
          bestLen = len;
          bestSize = getRepeatCodeLength(bestOffs, len)
                     + bitCountTable[i + len];
          bestOffsSum = offsSumTable[i + len] + bestOffs;
          len = maxRepeatLen;
        }
        else {
          // if a long RLE match is possible, use that
          matchTable[i].d = 1;
          matchTable[i].len = (unsigned int) len;
          bitCountTable[i] = bitCountTable[i + len]
                             + getRepeatCodeLength(1, len);
          offsSumTable[i] = offsSumTable[i + len] + 1UL;
          continue;
        }
      }
      // otherwise check all possible LZ77 match lengths,
      matchPtr++;
      for ( ; len > 0; len = (*(++matchPtr) & 0x03FFU)) {
        if (len > (nBytes - i))
          len = nBytes - i;
        unsigned int  d = *matchPtr >> 10;
        if (len >= 3) {
          // flag bit + offset bits
          size_t  nBitsBase = offs3EncodeTable.getSymbolSize(
                                  d - (unsigned int) minRepeatDist) + 1;
          do {
            size_t  nBits = lengthEncodeTable.getSymbolSize(
                                (unsigned int) (len - minRepeatLen))
                            + nBitsBase + bitCountTable[i + len];
            if (nBits < bestSize ||
                (nBits == bestSize &&
                 (offsSumTable[i + len] + d) <= bestOffsSum)) {
              bestSize = nBits;
              bestOffs = d;
              bestLen = len;
              bestOffsSum = offsSumTable[i + len] + d;
            }
          } while (--len >= 3);
        }
        // check short match lengths:
        if (len == 2) {                                         // 2 bytes
          if (d <= offs2EncodeTable.getSymbolsEncoded()) {
            size_t  nBits = len2BitsP1 + offs2EncodeTable.getSymbolSize(
                                             d - (unsigned int) minRepeatDist)
                            + bitCountTable[i + 2];
            if (nBits < bestSize ||
                (nBits == bestSize &&
                 (offsSumTable[i + 2] + d) <= bestOffsSum)) {
              bestSize = nBits;
              bestOffs = d;
              bestLen = 2;
              bestOffsSum = offsSumTable[i + 2] + d;
            }
          }
        }
        if (d <= offs1EncodeTable.getSymbolsEncoded()) {        // 1 byte
          size_t  nBits = len1BitsP1 + offs1EncodeTable.getSymbolSize(
                                           d - (unsigned int) minRepeatDist)
                          + bitCountTable[i + 1];
          if (nBits < bestSize ||
              (nBits == bestSize && (offsSumTable[i + 1] + d) <= bestOffsSum)) {
            bestSize = nBits;
            bestOffs = d;
            bestLen = 1;
            bestOffsSum = offsSumTable[i + 1] + d;
          }
        }
      }
      if (bestSize >= (bitCountTable[i + 1] + 8)) {
        // literal byte,
        size_t  nBits = bitCountTable[i + 1] + 9;
        if (nBits < bestSize ||
            (nBits == bestSize && offsSumTable[i + 1] <= bestOffsSum)) {
          bestSize = nBits;
          bestOffs = 0;
          bestLen = 1;
          bestOffsSum = offsSumTable[i + 1];
        }
        if ((i + literalSequenceMinLength) <= nBytes &&
            (bitCountTable[i + literalSequenceMinLength]
             + (literalSequenceMinLength * 8)) <= bestSize) {
          for (size_t k = literalSequenceMinLength;
               k <= literalSequenceMaxLength && (i + k) <= nBytes;
               k++) {
            // and all possible literal sequence lengths
            nBits = bitCountTable[i + k]
                    + (k * 8 + literalSequenceMinLength - 1);
            if (nBits > bestSize) {
              if (nBits > (bestSize + literalSequenceMinLength - 1))
                break;  // quit the loop earlier if the data can be compressed
              continue;
            }
            if (nBits == bestSize) {
              if (offsSumTable[i + k] > (offsSumTable[i + bestLen] + bestOffs))
                continue;
            }
            bestSize = nBits;
            bestOffs = 0;
            bestLen = k;
          }
          bestOffsSum = offsSumTable[i + bestLen] + bestOffs;
        }
      }
      matchTable[i].d = (unsigned int) bestOffs;
      matchTable[i].len = (unsigned int) bestLen;
      bitCountTable[i] = bestSize;
      offsSumTable[i] = bestOffsSum;
    }
  }

  void Compressor_M4::compressData_(std::vector< unsigned int >& tmpOutBuf,
                                    const std::vector< unsigned char >& inBuf,
                                    size_t offs, size_t nBytes,
                                    bool firstPass, bool fastMode)
  {
    size_t  endPos = offs + nBytes;
    tmpOutBuf.clear();
    if (!firstPass) {
      // generate optimal encode tables for offset values
      offs1EncodeTable.updateTables(false);
      offs2EncodeTable.updateTables(false);
      offs3EncodeTable.updateTables(fastMode);
      offs3NumSlots = offs3EncodeTable.getSlotCnt();
      offs3PrefixSize = offs3EncodeTable.getSlotPrefixSize(0);
    }
    // compress data by searching for repeated byte sequences,
    // and replacing them with length/distance codes
    std::vector< LZMatchParameters >  matchTable(nBytes);
    {
      std::vector< size_t > bitCountTable(nBytes + 1, 0);
      if (!firstPass) {
        std::vector< uint64_t > offsSumTable(nBytes + 1, 0UL);
        lengthEncodeTable.setUnencodedSymbolSize(lengthNumSlots + 15);
        optimizeMatches(&(matchTable.front()), &(bitCountTable.front()),
                        &(offsSumTable.front()), offs, nBytes);
      }
      else {
        // first pass: no symbol size information is available yet
        optimizeMatches_noStats(&(matchTable.front()), &(bitCountTable.front()),
                                offs, nBytes);
      }
    }
    lengthEncodeTable.setUnencodedSymbolSize(8192);
    // generate optimal encode table for length values
    for (size_t i = offs; i < endPos; ) {
      LZMatchParameters&  tmp = matchTable[i - offs];
      if (tmp.d > 0) {
        long    unencodedCost = long(tmp.len) * 9L - 1L;
        unencodedCost -=
            (tmp.len > 1 ? long(offs2PrefixSize) : long(offs1PrefixSize));
        unencodedCost = (unencodedCost > 0L ? unencodedCost : 0L);
        lengthEncodeTable.addSymbol(tmp.len - minRepeatLen,
                                    size_t(unencodedCost));
      }
      i += size_t(tmp.len);
    }
    lengthEncodeTable.updateTables(false);
    // update LZ77 offset statistics for calculating encode tables later
    for (size_t i = offs; i < endPos; ) {
      LZMatchParameters&  tmp = matchTable[i - offs];
      if (tmp.d > 0) {
        if (lengthEncodeTable.getSymbolSize(tmp.len - minRepeatLen) <= 64) {
          long    unencodedCost = long(tmp.len) * 9L - 1L;
          unencodedCost -=
              long(lengthEncodeTable.getSymbolSize(tmp.len - minRepeatLen));
          unencodedCost = (unencodedCost > 0L ? unencodedCost : 0L);
          if (tmp.len > 2) {
            offs3EncodeTable.addSymbol(tmp.d - minRepeatDist,
                                       size_t(unencodedCost));
          }
          else if (tmp.len > 1) {
            offs2EncodeTable.addSymbol(tmp.d - minRepeatDist,
                                       size_t(unencodedCost));
          }
          else {
            offs1EncodeTable.addSymbol(tmp.d - minRepeatDist,
                                       size_t(unencodedCost));
          }
        }
      }
      i += size_t(tmp.len);
    }
    // first pass: there are no offset encode tables yet, so no data is written
    if (firstPass)
      return;
    // write encode tables
    tmpOutBuf.push_back(0x02000000U | (unsigned int) (offs3PrefixSize - 2));
    for (size_t i = 0; i < offs1NumSlots; i++) {
      unsigned int  c = (unsigned int) offs1EncodeTable.getSlotSize(i);
      tmpOutBuf.push_back(0x04000000U | c);
    }
    for (size_t i = 0; i < lengthNumSlots; i++) {
      unsigned int  c = (unsigned int) lengthEncodeTable.getSlotSize(i);
      tmpOutBuf.push_back(0x04000000U | c);
    }
    for (size_t i = 0; i < offs2NumSlots; i++) {
      unsigned int  c = (unsigned int) offs2EncodeTable.getSlotSize(i);
      tmpOutBuf.push_back(0x04000000U | c);
    }
    for (size_t i = 0; i < offs3NumSlots; i++) {
      unsigned int  c = (unsigned int) offs3EncodeTable.getSlotSize(i);
      tmpOutBuf.push_back(0x04000000U | c);
    }
    // write compressed data
    for (size_t i = offs; i < endPos; ) {
      LZMatchParameters&  tmp = matchTable[i - offs];
      if (tmp.d > 0) {
        // check if this match needs to be replaced with literals:
        size_t  nBits = getRepeatCodeLength(tmp.d, tmp.len);
        if (nBits > 64) {
          // if the match cannot be encoded, assume "infinite" size
          nBits = 0x7FFFFFFF;
        }
        if ((size_t(tmp.len) >= literalSequenceMinLength &&
             nBits > (size_t(tmp.len) * 8 + literalSequenceMinLength - 1)) ||
            nBits >= (size_t(tmp.len) * 9)) {
          tmp.d = 0;
        }
      }
      if (tmp.d > 0) {
        // write LZ77 match
        writeRepeatCode(tmpOutBuf, tmp.d, tmp.len);
        i = i + tmp.len;
      }
      else {
        while (size_t(tmp.len) >= literalSequenceMinLength) {
          // write literal sequence
          size_t  len = tmp.len;
          len = (len < literalSequenceMaxLength ?
                 len : literalSequenceMaxLength);
          tmpOutBuf.push_back((unsigned int) ((lengthNumSlots + 1) << 24)
                              | ((1U << (unsigned int) (lengthNumSlots + 1))
                                 - 1U));
          tmpOutBuf.push_back(0x88000000U
                              | (unsigned int) (len + 2
                                                - literalSequenceMinLength));
          for (size_t j = 0; j < len; j++) {
            tmpOutBuf.push_back(0x88000000U | (unsigned int) inBuf[i]);
            i++;
          }
          tmp.len -= (unsigned int) len;
        }
        while (tmp.len > 0) {
          // write literal byte(s)
          tmpOutBuf.push_back(0x01000000U);
          tmpOutBuf.push_back(0x88000000U | (unsigned int) inBuf[i]);
          i++;
          tmp.len--;
        }
      }
    }
  }

  bool Compressor_M4::compressData(std::vector< unsigned int >& tmpOutBuf,
                                   const std::vector< unsigned char >& inBuf,
                                   bool isLastBlock, size_t offs, size_t nBytes,
                                   bool fastMode)
  {
    // the 'offs' and 'nBytes' parameters allow compressing a buffer
    // as multiple chunks for possibly improved statistical compression
    if (nBytes < 1 || offs >= inBuf.size())
      return true;
    if (nBytes > (inBuf.size() - offs))
      nBytes = inBuf.size() - offs;
    lengthEncodeTable.clear();
    offs1EncodeTable.clear();
    offs2EncodeTable.clear();
    offs3EncodeTable.clear();
    std::vector< uint64_t >     hashTable;
    std::vector< unsigned int > bestBuf;
    std::vector< unsigned int > tmpBuf;
    size_t  bestSize = 0x7FFFFFFF;
    bool    doneFlag = false;
    for (size_t i = 0; i < config.optimizeIterations; i++) {
      if (progressDisplayEnabled) {
        if (!setProgressPercentage(int(progressCnt * 100 / progressMax)))
          return false;
        progressCnt++;
      }
      if (doneFlag)     // if the compression cannot be optimized further,
        continue;       // quit the loop earlier
      tmpBuf.clear();
      compressData_(tmpBuf, inBuf, offs, nBytes, (i == 0), fastMode);
      if (i == 0)       // the first optimization pass writes no data
        continue;
      // calculate compressed size and hash value
      size_t    compressedSize = 17;
      uint64_t  h = 1UL;
      for (size_t j = 0; j < tmpBuf.size(); j++) {
        compressedSize += size_t((tmpBuf[j] & 0x7F000000U) >> 24);
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
    tmpOutBuf.push_back(0x090001FFU);
    tmpOutBuf.push_back(0x88000000U | (unsigned int) (!isLastBlock));
    return true;
  }

  // --------------------------------------------------------------------------

  Compressor_M4::Compressor_M4(std::vector< unsigned char >& outBuf_)
    : Compressor(outBuf_),
      lengthEncodeTable(lengthNumSlots, lengthMaxValue,
                        &(lengthPrefixSizeTable[0])),
      offs1EncodeTable(offs1NumSlots, offs1MaxValue, (size_t *) 0,
                       offs1PrefixSize),
      offs2EncodeTable(offs2NumSlots, offs2MaxValue, (size_t *) 0,
                       offs2PrefixSize),
      offs3EncodeTable(0, offs3MaxValue, (size_t *) 0,
                       2, 5, &(offs3SlotCntTable[0])),
      offs3NumSlots(4),
      offs3PrefixSize(2),
      searchTable((LZSearchTable *) 0),
      savedOutBufPos(0x7FFFFFFF),
      outputShiftReg(0x00),
      outputBitCnt(0)
  {
  }

  Compressor_M4::~Compressor_M4()
  {
    if (searchTable)
      delete searchTable;
  }

  bool Compressor_M4::compressData(const std::vector< unsigned char >& inBuf,
                                   unsigned int startAddr, bool isLastBlock,
                                   bool enableProgressDisplay)
  {
    (void) startAddr;
    if (outputBitCnt < 0)
      throw Ep128Emu::Exception("internal error: compressing to closed buffer");
    if (inBuf.size() < 1)
      return true;
    progressDisplayEnabled = enableProgressDisplay;
    try {
      if (enableProgressDisplay) {
        progressMessage("Compressing data");
        setProgressPercentage(0);
      }
      if (searchTable) {
        delete searchTable;
        searchTable = (LZSearchTable *) 0;
      }
      {
        size_t  maxOffs = inBuf.size() - 1;
        maxOffs = (maxOffs > 1 ?
                   (maxOffs < config.maxOffset ? maxOffs : config.maxOffset)
                   : 1);
        searchTable =
            new LZSearchTable(config.minLength, maxRepeatLen, lengthMaxValue,
                              offs1MaxValue, offs2MaxValue, maxOffs);
      }
      searchTable->findMatches(&(inBuf.front()), 0, inBuf.size());
      // split large files to improve statistical compression
      std::list< SplitOptimizationBlock >   splitPositions;
      std::map< uint64_t, size_t >          splitOptimizationCache;
      size_t  splitDepth = config.splitOptimizationDepth - 1;
      {
        while (inBuf.size() > (size_t(1) << (splitDepth + 16)))
          splitDepth++;         // limit block size to 64K
        size_t  splitCnt = size_t(1) << splitDepth;
        if (splitCnt > inBuf.size())
          splitCnt = inBuf.size();
        progressCnt = 0;
        progressMax = splitCnt
                      + (splitCnt > 1 ? (splitCnt - 1) : 0)
                      + (splitCnt > 2 ? (splitCnt - 2) : 0)
                      + (splitCnt > 3 ? (splitCnt - 3) : 0);
        progressMax = progressMax * config.optimizeIterations;
        progressMax =
            progressMax * ((splitDepth / 2) + 2) / ((splitDepth / 2) + 1);
        // create initial block list
        size_t  tmp = 0;
        for (size_t startPos = 0; startPos < inBuf.size(); ) {
          SplitOptimizationBlock  tmpBlock;
          tmpBlock.startPos = startPos;
          if (config.blockSize < 1) {
            tmp = tmp + inBuf.size();
            tmpBlock.nBytes = tmp / splitCnt;
            tmp = tmp % splitCnt;
          }
          else {
            // force block size specified by the user
            tmpBlock.nBytes = (config.blockSize < (inBuf.size() - startPos) ?
                               config.blockSize : (inBuf.size() - startPos));
          }
          startPos = startPos + tmpBlock.nBytes;
          splitPositions.push_back(tmpBlock);
        }
      }
      while (config.blockSize < 1) {
        size_t  bestMergePos = 0;
        long    bestMergeBits = 0x7FFFFFFFL;
        // find the pair of blocks that reduce the total compressed size
        // the most when merged
        std::list< SplitOptimizationBlock >::iterator curBlock =
            splitPositions.begin();
        while (curBlock != splitPositions.end()) {
          std::list< SplitOptimizationBlock >::iterator nxtBlock = curBlock;
          nxtBlock++;
          if (nxtBlock == splitPositions.end())
            break;
          if (((*curBlock).nBytes + (*nxtBlock).nBytes) > 65536) {
            curBlock++;
            continue;                   // limit block size to <= 64K
          }
          size_t  nBitsSplit = 0;
          size_t  nBitsMerged = 0;
          for (size_t i = 0; i < 3; i++) {
            // i = 0: merged block, i = 1: first block, i = 2: second block
            size_t  startPos = 0;
            size_t  endPos = 0;
            switch (i) {
            case 0:
              startPos = (*curBlock).startPos;
              endPos = startPos + (*curBlock).nBytes + (*nxtBlock).nBytes;
              break;
            case 1:
              startPos = (*curBlock).startPos;
              endPos = startPos + (*curBlock).nBytes;
              break;
            case 2:
              startPos = (*nxtBlock).startPos;
              endPos = startPos + (*nxtBlock).nBytes;
              break;
            }
            uint64_t  cacheKey = (uint64_t(startPos) << 32) | uint64_t(endPos);
            if (splitOptimizationCache.find(cacheKey)
                == splitOptimizationCache.end()) {
              // if this block is not in the cache yet, compress it,
              // and store the compressed size in the cache
              std::vector< unsigned int > tmpBuf;
              if (!compressData(tmpBuf, inBuf, false,
                                startPos, endPos - startPos, true)) {
                delete searchTable;
                searchTable = (LZSearchTable *) 0;
                if (progressDisplayEnabled)
                  progressMessage("");
                return false;
              }
              // calculate compressed size
              size_t  nBits = 0;
              for (size_t j = 0; j < tmpBuf.size(); j++)
                nBits += size_t((tmpBuf[j] & 0x7F000000U) >> 24);
              splitOptimizationCache[cacheKey] = nBits;
            }
            size_t  nBits = splitOptimizationCache[cacheKey];
            switch (i) {
            case 0:
              nBitsMerged = nBits;
              break;
            default:
              nBitsSplit += nBits;
              break;
            }
          }
          // calculate size change when merging blocks
          long    sizeDiff = long(nBitsMerged) - long(nBitsSplit);
          if (sizeDiff < bestMergeBits) {
            bestMergePos = (*curBlock).startPos;
            bestMergeBits = sizeDiff;
          }
          curBlock++;
        }
        if (bestMergeBits > 0L)         // no more blocks can be merged
          break;
        // merge the best pair of blocks and continue
        curBlock = splitPositions.begin();
        while ((*curBlock).startPos != bestMergePos)
          curBlock++;
        std::list< SplitOptimizationBlock >::iterator nxtBlock = curBlock;
        nxtBlock++;
        (*curBlock).nBytes = (*curBlock).nBytes + (*nxtBlock).nBytes;
        splitPositions.erase(nxtBlock);
      }
      // compress all blocks again with full optimization
      {
        size_t  progressPercentage = 0;
        if (progressCnt > 0 && progressMax > 0) {
          progressPercentage = (progressCnt * 100) / progressMax;
          if (progressPercentage > 85)
            progressPercentage = 85;
        }
        size_t  tmp = config.optimizeIterations * splitPositions.size();
        progressCnt = (tmp * progressPercentage) / (100 - progressPercentage);
        progressMax = progressCnt + tmp;
      }
      std::vector< unsigned int >   outBufTmp;
      std::list< SplitOptimizationBlock >::iterator i_ = splitPositions.begin();
      while (i_ != splitPositions.end()) {
        std::vector< unsigned int > tmpBuf;
        if (!compressData(tmpBuf, inBuf,
                          (isLastBlock &&
                           ((*i_).startPos + (*i_).nBytes) >= inBuf.size()),
                          (*i_).startPos, (*i_).nBytes, false)) {
          delete searchTable;
          searchTable = (LZSearchTable *) 0;
          if (progressDisplayEnabled)
            progressMessage("");
          return false;
        }
        for (size_t i = 0; i < tmpBuf.size(); i++)
          outBufTmp.push_back(tmpBuf[i]);
        i_++;
      }
      delete searchTable;
      searchTable = (LZSearchTable *) 0;
      if (progressDisplayEnabled) {
        setProgressPercentage(100);
        progressMessage("");
      }
      // pack output data
      for (size_t i = 0; i < outBufTmp.size(); i++) {
        unsigned int  c = outBufTmp[i];
        if (c >= 0x80000000U) {
          // special case for literal bytes, which are stored byte-aligned
          if (outputBitCnt > 0 && savedOutBufPos >= outBuf.size()) {
            // reserve space for the shift register to be stored later when
            // it is full, and save the write position
            savedOutBufPos = outBuf.size();
            outBuf.push_back((unsigned char) 0x00);
          }
          unsigned int  nBytes = ((c & 0x7F000000U) + 0x07000000U) >> 27;
          while (nBytes > 0U) {
            nBytes--;
            outBuf.push_back((unsigned char) ((c >> (nBytes * 8U)) & 0xFFU));
          }
        }
        else {
          unsigned int  nBits = c >> 24;
          c = c & 0x00FFFFFFU;
          for (unsigned int j = nBits; j > 0U; ) {
            j--;
            unsigned int  b = (unsigned int) (bool(c & (1U << j)));
            outputShiftReg = ((outputShiftReg & 0x7F) << 1) | (unsigned char) b;
            if (++outputBitCnt >= 8) {
              if (savedOutBufPos >= outBuf.size()) {
                outBuf.push_back(outputShiftReg);
              }
              else {
                // store at saved position if any literal bytes were inserted
                outBuf[savedOutBufPos] = outputShiftReg;
                savedOutBufPos = 0x7FFFFFFF;
              }
              outputShiftReg = 0x00;
              outputBitCnt = 0;
            }
          }
        }
      }
      if (isLastBlock) {
        while (outputBitCnt != 0) {
          outputShiftReg = ((outputShiftReg & 0x7F) << 1);
          if (++outputBitCnt >= 8) {
            if (savedOutBufPos >= outBuf.size()) {
              outBuf.push_back(outputShiftReg);
            }
            else {
              // store at saved position if any literal bytes were inserted
              outBuf[savedOutBufPos] = outputShiftReg;
              savedOutBufPos = 0x7FFFFFFF;
            }
            outputShiftReg = 0x00;
            outputBitCnt = 0;
          }
        }
        outputBitCnt = -1;              // set output buffer closed flag
      }
    }
    catch (...) {
      if (searchTable) {
        delete searchTable;
        searchTable = (LZSearchTable *) 0;
      }
      if (progressDisplayEnabled)
        progressMessage("");
      throw;
    }
    return true;
  }

}       // namespace Ep128Compress

