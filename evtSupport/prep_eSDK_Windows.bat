
SET EVT_DIR=C:\Program Files\EVT

mkdir os

cp -r "%EVT_DIR%\eSDK\include" .
cp -r "%EVT_DIR%\eSDK\lib" os\windows-x64
xcopy /s "%EVT_DIR%\eSDK\bin\*.*" os\windows-x64
