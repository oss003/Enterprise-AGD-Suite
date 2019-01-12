
// epimgconv: Enterprise 128 and TVC image converter utility
// Copyright (C) 2008-2016 Istvan Varga <istvanv@users.sourceforge.net>
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
//
// The Enterprise 128 program files generated by this utility are not covered
// by the GNU General Public License, and can be used, modified, and
// distributed without any restrictions.

#include "epimgconv.hpp"
#include "img_cfg.hpp"

namespace Ep128ImgConv {

  static const char *outputFormatNames[] = {
    "Enterprise program",               // 0
    "IVIEW (uncompressed)",             // 1
    "IVIEW (epcompress -m2)",           // 2
    "IVIEW (epcompress -m0)",           // 3
    "IVIEW (epcompress -m3)",           // 4
    "IVIEW (ZLib)",                     // 5
    "Agsys CRF",                        // 6
    "ZozoTools VL\\/VS",                // 7
    "PaintBox",                         // 8
    "Zaxial",                           // 9
    "Raw image data",                   // 10
    "TVC KEP (uncompressed)",           // 11
    "TVC KEP (RLE)",                    // 12
    "TVC KEP+ (epcompress -m2)",        // 13
    "TVC KEP+ (epcompress -m0)",        // 14
    "TVC KEP+ (epcompress -m3)",        // 15
    "TVC KEP+ (ZLib)"                   // 16
  };

  static const char *videoModeNames[] = {
    "2 colors",                         // 0
    "4 colors",                         // 1
    "16 colors A (pre-dither)",         // 2
    "16 colors B (post-dither)",        // 3
    "16 colors C (slow)",               // 4
    "256 colors",                       // 5
    "Attribute",                        // 6
    "TVC 2 colors",                     // 7
    "TVC 4 colors",                     // 8
    "TVC 16 colors",                    // 9
    "2 colors interlaced",              // 10
    "4 colors interlaced",              // 11
    "16 colors A interlaced",           // 12
    "16 colors B interlaced",           // 13
    "16 colors C interlaced",           // 14
    "256 colors interlaced",            // 15
    "Attribute interlaced",             // 16
    "TVC 2 colors interlaced",          // 17
    "TVC 4 colors interlaced",          // 18
    "TVC 16 colors interlaced"          // 19
  };

  static const char *ditherNames[] = {
    "No dither",                        // 0
    "Diffuse (Floyd-Steinberg)",        // 1
    "Diffuse (Stucki)",                 // 2
    "Diffuse (Jarvis)",                 // 3
    "Ordered (Bayer)",                  // 4
    "Ordered (randomized)"              // 5
  };

  void ImageConvConfig::configChangeCallbackBoolean(void *userData_,
                                                    const std::string& name_,
                                                    bool value_)
  {
    (void) name_;
    (void) value_;
    reinterpret_cast<ImageConvConfig *>(userData_)->configChangeFlag = true;
  }

  void ImageConvConfig::configChangeCallbackInteger(void *userData_,
                                                    const std::string& name_,
                                                    int value_)
  {
    (void) name_;
    (void) value_;
    reinterpret_cast<ImageConvConfig *>(userData_)->configChangeFlag = true;
  }

  void ImageConvConfig::configChangeCallbackFloat(void *userData_,
                                                  const std::string& name_,
                                                  double value_)
  {
    (void) name_;
    (void) value_;
    reinterpret_cast<ImageConvConfig *>(userData_)->configChangeFlag = true;
  }

  ImageConvConfig::ImageConvConfig()
    : Ep128Emu::ConfigurationDB()
  {
    resetDefaultSettings();
    createKey("outputFormat", outputFormat);
    (*this)["outputFormat"].setRange(0.0, 16.0);
    createKey("compressionLevel", compressionLevel);
    (*this)["compressionLevel"].setRange(1.0, 9.0);
    createKey("conversionType", conversionType);
    (*this)["conversionType"].setRange(0.0, 19.0);
    (*this)["conversionType"].setCallback(&configChangeCallbackInteger,
                                          (void *) this, true);
    createKey("width", width);
    (*this)["width"].setRange(-255.0, 255.0);
    (*this)["width"].setCallback(&configChangeCallbackInteger,
                                 (void *) this, true);
    createKey("height", height);
    (*this)["height"].setRange(-16384.0, 16384.0);
    (*this)["height"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("borderColor", borderColor);
    (*this)["borderColor"].setRange(0.0, 255.0);
    (*this)["borderColor"].setCallback(&configChangeCallbackInteger,
                                       (void *) this, true);
    createKey("paletteResolution", paletteResolution);
    (*this)["paletteResolution"].setRange(0.0, 1.0);
    (*this)["paletteResolution"].setCallback(&configChangeCallbackInteger,
                                             (void *) this, true);
    createKey("conversionQuality", conversionQuality);
    (*this)["conversionQuality"].setRange(1.0, 9.0);
    (*this)["conversionQuality"].setCallback(&configChangeCallbackInteger,
                                             (void *) this, true);
    createKey("colorErrorScale", colorErrorScale);
    (*this)["colorErrorScale"].setRange(0.05, 1.0);
    (*this)["colorErrorScale"].setCallback(&configChangeCallbackFloat,
                                           (void *) this, true);
    createKey("ditherType", ditherType);
    (*this)["ditherType"].setRange(0.0, 5.0);
    (*this)["ditherType"].setCallback(&configChangeCallbackInteger,
                                      (void *) this, true);
    createKey("ditherDiffusion", ditherDiffusion);
    (*this)["ditherDiffusion"].setRange(0.0, 1.0);
    (*this)["ditherDiffusion"].setCallback(&configChangeCallbackFloat,
                                           (void *) this, true);
    createKey("scaleMode", scaleMode);
    (*this)["scaleMode"].setRange(0.0, 1.0);
    (*this)["scaleMode"].setCallback(&configChangeCallbackInteger,
                                     (void *) this, true);
    createKey("scaleX", scaleX);
    (*this)["scaleX"].setRange(0.1, 10.0);
    (*this)["scaleX"].setCallback(&configChangeCallbackFloat,
                                  (void *) this, true);
    createKey("scaleY", scaleY);
    (*this)["scaleY"].setRange(0.1, 10.0);
    (*this)["scaleY"].setCallback(&configChangeCallbackFloat,
                                  (void *) this, true);
    createKey("offsetX", offsetX);
    (*this)["offsetX"].setRange(-10000.0, 10000.0);
    (*this)["offsetX"].setCallback(&configChangeCallbackFloat,
                                   (void *) this, true);
    createKey("offsetY", offsetY);
    (*this)["offsetY"].setRange(-10000.0, 10000.0);
    (*this)["offsetY"].setCallback(&configChangeCallbackFloat,
                                   (void *) this, true);
    createKey("yMin", yMin);
    (*this)["yMin"].setRange(-0.5, 1.0);
    (*this)["yMin"].setCallback(&configChangeCallbackFloat,
                                (void *) this, true);
    createKey("yMax", yMax);
    (*this)["yMax"].setRange(0.0, 2.0);
    (*this)["yMax"].setCallback(&configChangeCallbackFloat,
                                (void *) this, true);
    createKey("colorSaturationMult", colorSaturationMult);
    (*this)["colorSaturationMult"].setRange(0.0, 8.0);
    (*this)["colorSaturationMult"].setCallback(&configChangeCallbackFloat,
                                               (void *) this, true);
    createKey("gammaCorrection", gammaCorrection);
    (*this)["gammaCorrection"].setRange(0.25, 4.0);
    (*this)["gammaCorrection"].setCallback(&configChangeCallbackFloat,
                                           (void *) this, true);
    createKey("fixBias", fixBias);
    (*this)["fixBias"].setRange(-1.0, 31.0);
    (*this)["fixBias"].setCallback(&configChangeCallbackInteger,
                                   (void *) this, true);
    createKey("color0", paletteColors[0]);
    (*this)["color0"].setRange(-1.0, 255.0);
    (*this)["color0"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("color1", paletteColors[1]);
    (*this)["color1"].setRange(-1.0, 255.0);
    (*this)["color1"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("color2", paletteColors[2]);
    (*this)["color2"].setRange(-1.0, 255.0);
    (*this)["color2"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("color3", paletteColors[3]);
    (*this)["color3"].setRange(-1.0, 255.0);
    (*this)["color3"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("color4", paletteColors[4]);
    (*this)["color4"].setRange(-1.0, 255.0);
    (*this)["color4"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("color5", paletteColors[5]);
    (*this)["color5"].setRange(-1.0, 255.0);
    (*this)["color5"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("color6", paletteColors[6]);
    (*this)["color6"].setRange(-1.0, 255.0);
    (*this)["color6"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("color7", paletteColors[7]);
    (*this)["color7"].setRange(-1.0, 255.0);
    (*this)["color7"].setCallback(&configChangeCallbackInteger,
                                  (void *) this, true);
    createKey("noInterpolation", noInterpolation);
    (*this)["noInterpolation"].setCallback(&configChangeCallbackBoolean,
                                           (void *) this, true);
    createKey("noCompress", noCompress);
  }

  ImageConvConfig::~ImageConvConfig()
  {
  }

  void ImageConvConfig::resetDefaultSettings()
  {
    outputFormat = 0;
    compressionLevel = 5;
    conversionType = 2;
    width = 40;
    height = 240;
    borderColor = 0x00;
    paletteResolution = 1;
    conversionQuality = 3;
    colorErrorScale = 0.5;
    ditherType = 1;
    ditherDiffusion = 0.95;
    scaleMode = 0;
    scaleX = 1.0;
    scaleY = 1.0;
    offsetX = 0.0;
    offsetY = 0.0;
    yMin = 0.0;
    yMax = 1.0;
    colorSaturationMult = 1.0;
    gammaCorrection = 1.0;
    fixBias = -1;
    for (int i = 0; i < 8; i++)
      paletteColors[i] = -1;
    noInterpolation = false;
    noCompress = false;
    configChangeFlag = true;
  }

  const char * ImageConvConfig::getOutputFormatName(int outFmt)
  {
    if (outFmt < 0 || outFmt >= int(sizeof(outputFormatNames) / sizeof(char *)))
      return (char *) 0;
    return outputFormatNames[outFmt];
  }

  const char * ImageConvConfig::getVideoModeName(int mode)
  {
    if (mode < 0 || mode >= int(sizeof(videoModeNames) / sizeof(char *)))
      return (char *) 0;
    return videoModeNames[mode];
  }

  const char * ImageConvConfig::getDitherName(int d)
  {
    if (d < 0 || d >= int(sizeof(ditherNames) / sizeof(char *)))
      return (char *) 0;
    return ditherNames[d];
  }

  int ImageConvConfig::getOutputFormat() const
  {
    if (outputFormat < 0 || outputFormat >= 17)
      return 0;
    if (outputFormat < 2)
      return outputFormat;
    if (outputFormat < 6)
      return ((outputFormat - 1) * 10 + compressionLevel);
    if (outputFormat < 11)
      return (outputFormat - 4);
    if (outputFormat < 13)
      return (outputFormat == 11 ? 50 : 55);
    return ((outputFormat - 12) + (compressionLevel < 9 ? 50 : 55));
  }

  void ImageConvConfig::setOutputFormat(int outFmt)
  {
    compressionLevel = 5;
    if (outFmt < 0 || outFmt > 59 || (outFmt >= 7 && outFmt < 11) ||
        outFmt == 20 || outFmt == 30 || outFmt == 40) {
      outputFormat = 0;
      return;
    }
    if (outFmt < 7) {
      outputFormat = (outFmt < 2 ? outFmt : (outFmt + 4));
      return;
    }
    if (outFmt < 50) {
      outputFormat = (outFmt / 10) + 1;
      compressionLevel = outFmt % 10;
      return;
    }
    if (outFmt == 50 || outFmt == 55) {
      outputFormat = (outFmt == 50 ? 11 : 12);
      return;
    }
    outputFormat = (outFmt % 5) + 12;
    compressionLevel = (outFmt < 55 ? 5 : 9);
  }

}       // namespace Ep128ImgConv

