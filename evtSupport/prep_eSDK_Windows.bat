
REM Set location of installed emergent vision SDK
SET EVT_DIR=C:\Program Files\EVT

REM Create appropriate directories
mkdir os
mkdir os\windows-x64

REM Copy include folder and header files
xcopy /s "%EVT_DIR%\eSDK\include\*.h" .

REM Copy .lib and .dll files to the appropriate locations
xcopy /s "%EVT_DIR%\eSDK\lib\*.*" os\windows-x64
xcopy /s "%EVT_DIR%\eSDK\bin\*.*" os\windows-x64
