@ECHO OFF
IF [%1]==[] goto usage

SET VER=%1

rm -rf release
mkdir release

dotnet publish .\scscd-gui\scscd-gui.csproj ^
       -c Release ^
       -r win-x64 ^
       --self-contained false ^
       -p:PublishSingleFile=true
if %errorlevel% neq 0 exit /b %errorlevel%

cp -r res release/scscd-%VER%-og
if %errorlevel% neq 0 exit /b %errorlevel%
cp build/og/scscd.dll release/scscd-%VER%-og/F4SE/Plugins/scscd.dll
if %errorlevel% neq 0 exit /b %errorlevel%
cp scscd-gui/bin/Release/net8.0-windows/win-x64/publish/scscd-gui.exe release/scscd-%VER%-og/F4SE/Plugins/scscd/scscd-config.exe
if %errorlevel% neq 0 exit /b %errorlevel%
rm release/scscd-%VER%-og.7z
cd release
cd scscd-%VER%-og
7z a -r ../scscd-%VER%-og.7z .
if %errorlevel% neq 0 exit /b %errorlevel%
cd ..
cd ..

cp -r res release/scscd-%VER%-ng
if %errorlevel% neq 0 exit /b %errorlevel%
cp build/ng/scscd.dll release/scscd-%VER%-ng/F4SE/Plugins/scscd.dll
if %errorlevel% neq 0 exit /b %errorlevel%
cp scscd-gui/bin/Release/net8.0-windows/win-x64/publish/scscd-gui.exe release/scscd-%VER%-ng/F4SE/Plugins/scscd/scscd-config.exe
if %errorlevel% neq 0 exit /b %errorlevel%
rm release/scscd-%VER%-ng.7z
cd release
cd scscd-%VER%-ng
7z a -r ../scscd-%VER%-ng.7z .
if %errorlevel% neq 0 exit /b %errorlevel%
cd ..
cd ..

goto :eof

:usage
@echo Usage: %0 ^<version^>
exit /B 1
