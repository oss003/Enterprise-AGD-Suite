@echo off

rem Compile AGD file
 cls
 copy AGDsources\%1.agd agd >nul
 if errorlevel 1 goto error
 cd AGD
 CompilerEP %1 %2
 copy %1.asm ..\Sjasm\ >nul
 del %1.*

rem Assemble file
 cd ..\Sjasm
 sjasm %1.asm %1.com
 copy %1.com ..\EP128-2.0.11\files >nul
 copy %1.com ..\EP128-2.0.11\files\agdgame.com >nul
 del %1.*

rem Start emulator
 echo Starting emulator with %1.atm
 cd ..\EP128-2.0.11
 ep128emu  -no-opengl -snapshot snapshot\agdgame.ep128s
 echo Quiting emulator
 cd ..
 goto end

:error
 echo %1.agd not found .....

:end
