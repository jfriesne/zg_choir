set CHOIR_NAME=ZGChoir
REM set CHOIR_VERSION=`grep CHOIR_VERSION_STRING ChoirProtocol.h | cut -d \" -f 2`
set CHOIR_VERSION=0.8b
set CHOIR_DIR=.\%CHOIR_NAME%_v%CHOIR_VERSION%_for_Windows

rd /s /q %CHOIR_DIR%
mkdir %CHOIR_DIR%

move .\Release\%CHOIR_NAME%.exe %CHOIR_DIR%
mkdir %CHOIR_DIR%\songs
copy .\songs\* %CHOIR_DIR%\songs\
copy .\README.txt %CHOIR_DIR%

set QTLIBDIR=%QTDIR%\qtbase\lib
copy %QTLIBDIR%\Qt5Multimedia.dll %CHOIR_DIR%
copy %QTLIBDIR%\Qt5Widgets.dll %CHOIR_DIR%
copy %QTLIBDIR%\Qt5Gui.dll %CHOIR_DIR%
copy %QTLIBDIR%\Qt5Core.dll %CHOIR_DIR%
copy C:\windows\system32\vcruntime140.dll %CHOIR_DIR%

zip -r %CHOIR_DIR%.zip %CHOIR_DIR%
