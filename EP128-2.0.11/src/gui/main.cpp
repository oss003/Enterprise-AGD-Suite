
// ep128emu -- portable Enterprise 128 emulator
// Copyright (C) 2003-2017 Istvan Varga <istvanv@users.sourceforge.net>
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
#include "ep128vm.hpp"
#include "zx128vm.hpp"
#include "cpc464vm.hpp"
#include "tvc64vm.hpp"
#include "system.hpp"
#include "guicolor.hpp"

#include <typeinfo>

#ifdef WIN32
#  include <windows.h>
#else
#  include <dirent.h>
#endif

static void cfgErrorFunc(void *userData, const char *msg)
{
  (void) userData;
#ifndef WIN32
  std::fprintf(stderr, "WARNING: %s\n", msg);
#else
  (void) MessageBoxA((HWND) 0, (LPCSTR) msg, (LPCSTR) "ep128emu error",
                     MB_OK | MB_ICONWARNING);
#endif
}

int8_t getSnapshotType(const Ep128Emu::File& f)
{
  if (f.getBufferDataSize() < 40)
    throw Ep128Emu::Exception("invalid snapshot file");
  const unsigned char   *buf = f.getBufferData();
  if (buf[0] != 0x45 || buf[1] != 0x50 || buf[2] != 0x80)
    throw Ep128Emu::Exception("invalid snapshot file");
  // check LSB of chunk type (0x455080xx, see src/fileio.hpp)
  if ((buf[3] & 0xF0) >= 0x20 && (buf[3] & 0xF0) <= 0x40)
    return int8_t(((buf[3] & 0xF0) >> 4) - 1);  // Spectrum, CPC, TVC
  if (buf[3] >= 0x0B)                   // Plus/4
    throw Ep128Emu::Exception("unsupported machine type in snapshot file");
  return 0;                             // Enterprise
}

int main(int argc, char **argv)
{
  Fl_Window *w = (Fl_Window *) 0;
  Ep128Emu::VirtualMachine  *vm = (Ep128Emu::VirtualMachine *) 0;
  Ep128Emu::AudioOutput     *audioOutput = (Ep128Emu::AudioOutput *) 0;
#ifdef ENABLE_MIDI_PORT
  Ep128Emu::MIDIPort        *midiPort = (Ep128Emu::MIDIPort *) 0;
#endif
  Ep128Emu::EmulatorConfiguration   *config =
      (Ep128Emu::EmulatorConfiguration *) 0;
  Ep128Emu::VMThread        *vmThread = (Ep128Emu::VMThread *) 0;
  Ep128EmuGUI               *gui_ = (Ep128EmuGUI *) 0;
  const char      *cfgFileName = "ep128cfg.dat";
  Ep128Emu::File  *snapshotFile = (Ep128Emu::File *) 0;
  int       snapshotNameIndex = 0;
  int       colorScheme = 0;
  int8_t    machineType = -1;   // 0: EP (default), 1: ZX, 2: CPC, 3: TVC
  int8_t    retval = 0;
#ifdef DISABLE_OPENGL_DISPLAY
  bool      glEnabled = false;
#else
  bool      glEnabled = true;
  bool      glCanDoSingleBuf = false;
  bool      glCanDoDoubleBuf = false;
#endif
  bool      configLoaded = false;

#ifdef WIN32
  timeBeginPeriod(1U);
#else
  // set machine type if the program name in argv[0] begins with
  // "zx", "cpc" or "tvc"
  for (size_t i = 0; argv[0][i] != '\0'; i++) {
    if (i == 0 || argv[0][i] == '/') {
      if (argv[0][i] == '/')
        i++;
      if (std::strncmp(argv[0] + i, "zx", 2) == 0)
        machineType = 1;
      else if (std::strncmp(argv[0] + i, "cpc", 3) == 0)
        machineType = 2;
      else if (std::strncmp(argv[0] + i, "tvc", 3) == 0)
        machineType = 3;
      else
        machineType = -1;
    }
  }
#endif
  try {
    // find out machine type to be emulated
    for (int i = 1; i < argc; i++) {
      if (std::strcmp(argv[i], "-cfg") == 0 && i < (argc - 1)) {
        i++;
      }
      else if (std::strcmp(argv[i], "-snapshot") == 0) {
        if (++i >= argc)
          throw Ep128Emu::Exception("missing snapshot file name");
        snapshotNameIndex = i;
      }
      else if (std::strcmp(argv[i], "-colorscheme") == 0) {
        if (++i >= argc)
          throw Ep128Emu::Exception("missing color scheme number");
        colorScheme = int(std::atoi(argv[i]));
        colorScheme = (colorScheme >= 0 && colorScheme <= 3 ? colorScheme : 0);
      }
      else if (std::strcmp(argv[i], "-ep128") == 0) {
        machineType = 0;
      }
      else if (std::strcmp(argv[i], "-zx") == 0) {
        machineType = 1;
      }
      else if (std::strcmp(argv[i], "-cpc") == 0) {
        machineType = 2;
      }
      else if (std::strcmp(argv[i], "-tvc") == 0) {
        machineType = 3;
      }
#ifndef DISABLE_OPENGL_DISPLAY
      else if (std::strcmp(argv[i], "-opengl") == 0) {
        glEnabled = true;
      }
#endif
      else if (std::strcmp(argv[i], "-no-opengl") == 0) {
        glEnabled = false;
      }
      else if (std::strcmp(argv[i], "-h") == 0 ||
               std::strcmp(argv[i], "-help") == 0 ||
               std::strcmp(argv[i], "--help") == 0) {
        std::fprintf(stderr, "Usage: %s [OPTIONS...]\n", argv[0]);
        std::fprintf(stderr, "The allowed options are:\n");
        std::fprintf(stderr,
                     "    -h | -help | --help "
                     "print this message\n");
        std::fprintf(stderr,
                     "    -ep128 | -zx | -cpc | -tvc\n                        "
                     "select the type of machine to be emulated\n");
        std::fprintf(stderr,
                     "    -cfg <FILENAME>     "
                     "load ASCII format configuration file\n");
        std::fprintf(stderr,
                     "    -snapshot <FNAME>   "
                     "load snapshot or demo file on startup\n");
#ifndef DISABLE_OPENGL_DISPLAY
        std::fprintf(stderr,
                     "    -opengl             "
                     "use OpenGL video driver (this is the default)\n");
#endif
        std::fprintf(stderr,
                     "    -no-opengl          "
                     "use software video driver\n");
        std::fprintf(stderr,
                     "    -colorscheme <N>    "
                     "use GUI color scheme N (0, 1, 2, or 3)\n");
        std::fprintf(stderr,
                     "    OPTION=VALUE        "
                     "set configuration variable 'OPTION' to 'VALUE'\n");
        std::fprintf(stderr,
                     "    OPTION              "
                     "set boolean configuration variable 'OPTION' to true\n");
#ifdef WIN32
        timeEndPeriod(1U);
#endif
        return 0;
      }
    }

    Fl::lock();
    Ep128Emu::setGUIColorScheme(colorScheme);
    audioOutput = new Ep128Emu::AudioOutput_PortAudio();
#ifndef DISABLE_OPENGL_DISPLAY
    if (glEnabled) {
      glCanDoSingleBuf = bool(Fl_Gl_Window::can_do(FL_RGB | FL_SINGLE));
      glCanDoDoubleBuf = bool(Fl_Gl_Window::can_do(FL_RGB | FL_DOUBLE));
      if (glCanDoSingleBuf | glCanDoDoubleBuf)
        w = new Ep128Emu::OpenGLDisplay(32, 32, 384, 288, "");
      else
        glEnabled = false;
    }
#endif
    if (!glEnabled)
      w = new Ep128Emu::FLTKDisplay(32, 32, 384, 288, "");
    w->end();
    if (snapshotNameIndex > 0) {
      snapshotFile = new Ep128Emu::File(argv[snapshotNameIndex], false);
      if (machineType < 0)
        machineType = getSnapshotType(*snapshotFile);
    }
    if (machineType == 1) {
      cfgFileName = "zx128cfg.dat";
      vm = new ZX128::ZX128VM(*(dynamic_cast<Ep128Emu::VideoDisplay *>(w)),
                              *audioOutput);
    }
    else if (machineType == 2) {
      cfgFileName = "cpc_cfg.dat";
      vm = new CPC464::CPC464VM(*(dynamic_cast<Ep128Emu::VideoDisplay *>(w)),
                                *audioOutput);
    }
    else if (machineType == 3) {
      cfgFileName = "tvc_cfg.dat";
      vm = new TVC64::TVC64VM(*(dynamic_cast<Ep128Emu::VideoDisplay *>(w)),
                              *audioOutput);
    }
    else {
      vm = new Ep128::Ep128VM(*(dynamic_cast<Ep128Emu::VideoDisplay *>(w)),
                              *audioOutput);
    }
#ifdef ENABLE_MIDI_PORT
    midiPort = new Ep128Emu::MIDIPort(*vm);
#endif
    config = new Ep128Emu::EmulatorConfiguration(
        *vm, *(dynamic_cast<Ep128Emu::VideoDisplay *>(w)), *audioOutput
#ifdef ENABLE_MIDI_PORT
        , *midiPort
#endif
        );
    config->setErrorCallback(&cfgErrorFunc, (void *) 0);
    // load base configuration (if available)
    {
      Ep128Emu::File  *f = (Ep128Emu::File *) 0;
#ifndef WIN32
      std::string baseDir(Ep128Emu::getEp128EmuHomeDirectory());
      bool    makecfgNeeded = false;
      bool    makecfgDone = false;
#endif
      try {
        try {
          f = new Ep128Emu::File(cfgFileName, true);
        }
        catch (Ep128Emu::Exception& e) {
#ifndef WIN32
          makecfgNeeded = true;
        }
        while (true) {
          if (makecfgNeeded)
#endif
          {
            std::string cmdLine = "\"";
            cmdLine += argv[0];
            size_t  i = cmdLine.length();
            while (i > 1) {
              i--;
              if (cmdLine[i] == '/' || cmdLine[i] == '\\') {
                i++;
                break;
              }
            }
            cmdLine.resize(i);
#ifdef WIN32
            cmdLine += "makecfg\"";
#else
#  ifndef __APPLE__
            cmdLine += "epmakecfg\" -c \"";
#  else
            cmdLine += "epmakecfg\" -f \"";
#  endif
            cmdLine += baseDir;
            cmdLine += '"';
            makecfgDone = true;
#endif
            std::system(cmdLine.c_str());
          }
          f = new Ep128Emu::File(cfgFileName, true);
#ifndef WIN32
          config->registerChunkType(*f);
          f->processAllChunks();
          delete f;
          f = (Ep128Emu::File *) 0;
          if (makecfgDone)
            break;
          // check if there is a valid ROM image on segment 0x00
          makecfgNeeded = config->memory.rom[0x00].file.empty();
          if (!makecfgNeeded) {
            std::FILE *tmp =
                Ep128Emu::fileOpen(config->memory.rom[0x00].file.c_str(), "rb");
            makecfgNeeded = (!tmp);
            if (!makecfgNeeded) {
              std::fclose(tmp);
              break;
            }
            // get install directory from existing configuration
            std::string romName;
            Ep128Emu::splitPath(config->memory.rom[0x00].file,
                                baseDir, romName);
            DIR     *d = (DIR *) 0;
            if (baseDir.length() > 6 && !romName.empty() &&
                std::strcmp(baseDir.c_str() + (baseDir.length() - 6), "/roms/")
                == 0) {
              baseDir.resize(baseDir.length() - 6);
              d = opendir(baseDir.c_str());
            }
            if (d)
              closedir(d);
            else
              baseDir = Ep128Emu::getEp128EmuHomeDirectory();
          }
          // invalid configuration, try running makecfg if not done yet
          delete config;
          config = (Ep128Emu::EmulatorConfiguration *) 0;
          makecfgNeeded = true;
          config = new Ep128Emu::EmulatorConfiguration(
              *vm, *(dynamic_cast<Ep128Emu::VideoDisplay *>(w)), *audioOutput
#ifdef ENABLE_MIDI_PORT
              , *midiPort
#endif
              );
          config->setErrorCallback(&cfgErrorFunc, (void *) 0);
#endif
        }
#ifdef WIN32
        config->registerChunkType(*f);
        f->processAllChunks();
        delete f;
#endif
      }
      catch (...) {
        if (f)
          delete f;
      }
    }
    configLoaded = true;
    // check command line for any additional configuration
    for (int i = 1; i < argc; i++) {
      if (std::strcmp(argv[i], "-ep128") == 0 ||
          std::strcmp(argv[i], "-zx") == 0 ||
          std::strcmp(argv[i], "-cpc") == 0 ||
          std::strcmp(argv[i], "-tvc") == 0 ||
#ifndef DISABLE_OPENGL_DISPLAY
          std::strcmp(argv[i], "-opengl") == 0 ||
#endif
          std::strcmp(argv[i], "-no-opengl") == 0)
        continue;
      if (std::strcmp(argv[i], "-cfg") == 0) {
        if (++i >= argc)
          throw Ep128Emu::Exception("missing configuration file name");
        config->loadState(argv[i], false);
      }
      else if (std::strcmp(argv[i], "-snapshot") == 0 ||
               std::strcmp(argv[i], "-colorscheme") == 0) {
        i++;
      }
      else {
        const char  *s = argv[i];
#ifdef __APPLE__
        if (std::strncmp(s, "-psn_", 5) == 0)
          continue;
#endif
        if (*s == '-')
          s++;
        if (*s == '-')
          s++;
        const char  *p = std::strchr(s, '=');
        if (!p)
          (*config)[s] = bool(true);
        else {
          std::string optName;
          while (s != p) {
            optName += (*s);
            s++;
          }
          p++;
          (*config)[optName] = p;
        }
      }
    }
#ifndef DISABLE_OPENGL_DISPLAY
    if (glEnabled) {
      if (config->display.bufferingMode == 0 && !glCanDoSingleBuf) {
        config->display.bufferingMode = 1;
        config->displaySettingsChanged = true;
      }
      if (config->display.bufferingMode != 0 && !glCanDoDoubleBuf) {
        config->display.bufferingMode = 0;
        config->displaySettingsChanged = true;
      }
    }
#endif
    config->applySettings();
    if (snapshotFile) {
      vm->registerChunkTypes(*snapshotFile);
      snapshotFile->processAllChunks();
      delete snapshotFile;
      snapshotFile = (Ep128Emu::File *) 0;
    }
    vmThread = new Ep128Emu::VMThread(*vm);
    gui_ = new Ep128EmuGUI(*(dynamic_cast<Ep128Emu::VideoDisplay *>(w)),
                           *audioOutput, *vm, *vmThread, *config);
    gui_->run();
  }
  catch (std::exception& e) {
    if (snapshotFile) {
      delete snapshotFile;
      snapshotFile = (Ep128Emu::File *) 0;
    }
    if (gui_) {
      gui_->errorMessage(e.what());
    }
    else {
#ifndef WIN32
      std::fprintf(stderr, " *** error: %s\n", e.what());
#else
      (void) MessageBoxA((HWND) 0, (LPCSTR) e.what(), (LPCSTR) "ep128emu error",
                         MB_OK | MB_ICONWARNING);
#endif
    }
    retval = int8_t(-1);
  }
  if (gui_)
    delete gui_;
  if (vmThread)
    delete vmThread;
  if (config) {
    if (configLoaded) {
      try {
        Ep128Emu::File  f;
        config->saveState(f);
        f.writeFile(cfgFileName, true);
      }
      catch (...) {
      }
    }
    delete config;
  }
#ifdef ENABLE_MIDI_PORT
  if (midiPort)
    delete midiPort;
#endif
  if (vm)
    delete vm;
  if (w)
    delete w;
  if (audioOutput)
    delete audioOutput;
#ifdef WIN32
  timeEndPeriod(1U);
#endif
  return int(retval);
}

