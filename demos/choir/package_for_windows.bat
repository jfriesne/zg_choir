setlocal disableDelayedExpansion

@echo off
for /f "tokens=1,2,3 delims= " %%a in (ChoirProtocol.h) do (
   if "%%b"=="CHOIR_VERSION_STRING" (SET CHOIR_VERSION=%%c)
)
@echo on

REM remove the quote marks from %CHOIR_VERSION%
set CHOIR_VERSION=%CHOIR_VERSION:~1,-1%
echo CHOIR_VERSION IS %CHOIR_VERSION%

set CHOIR_NAME=ZGChoir
set CHOIR_DIR=%CHOIR_NAME%_Dist

nmake

rd /s /q %CHOIR_DIR%
mkdir %CHOIR_DIR%

copy .\Release\%CHOIR_NAME%.exe %CHOIR_DIR%
mkdir %CHOIR_DIR%\songs
copy .\songs\* %CHOIR_DIR%\songs\
copy .\html\README.html %CHOIR_DIR%\
mkdir %CHOIR_DIR%\images
copy .\html\images\*.png %CHOIR_DIR%\images

set QTLIBDIR=%QTDIR%\qtbase\lib
copy %QTLIBDIR%\Qt5Multimedia.dll %CHOIR_DIR%
copy %QTLIBDIR%\Qt5Widgets.dll %CHOIR_DIR%
copy %QTLIBDIR%\Qt5Gui.dll %CHOIR_DIR%
copy %QTLIBDIR%\Qt5Core.dll %CHOIR_DIR%
copy C:\windows\system32\vcruntime140.dll %CHOIR_DIR%

"\Program Files (x86)\Inno Setup 5\ISCC.exe" zg_choir.iss /DCHOIR_VERSION=%CHOIR_VERSION%
