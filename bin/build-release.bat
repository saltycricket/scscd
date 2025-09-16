@ECHO OFF
IF [%1]==[] goto usage

SET VER=%1

rm -rf release
mkdir release

cp -r res release/scscd-%VER%-og
cp build/og/scscd.dll release/scscd-%VER%-og/F4SE/Plugins/scscd.dll
cp scscd-gui/bin/Release/net8.0-windows/scscd-gui.exe release/scscd-%VER%-og/scscd-config.exe
rm release/scscd-%VER%-og.7z
cd release
cd scscd-%VER%-og
7z a -r ../scscd-%VER%-og.7z .
cd ..
cd ..

cp -r res release/scscd-%VER%-ng
cp build/ng/scscd.dll release/scscd-%VER%-ng/F4SE/Plugins/scscd.dll
cp scscd-gui/bin/Release/net8.0-windows/scscd-gui.exe release/scscd-%VER%-ng/scscd-config.exe
rm release/scscd-%VER%-ng.7z
cd release
cd scscd-%VER%-ng
7z a -r ../scscd-%VER%-ng.7z .
cd ..
cd ..

goto :eof

:usage
@echo Usage: %0 ^<version^>
exit /B 1
