@ECHO OFF
IF [%1]==[] goto usage

SET VER=%1

rm -rf scscd-%VER%-og
rm -rf scscd-%VER%-ng

cp -r res scscd-%VER%-og
cp build\og\scscd.dll scscd-%VER%-og\F4SE\Plugins\scscd.dll
cp scscd-gui\bin\Release\net8.0-windows\scscd-gui.exe scscd-%VER%-og\scscd-config.exe
rm scscd-%VER%-og.7z
cd scscd-%VER%-og
7z a -r scscd-%VER%-og.7z .
cd ..

cp -r res scscd-%VER%-ng
cp build\ng\scscd.dll scscd-%VER%-ng\F4SE\Plugins\scscd.dll
cp scscd-gui\bin\Release\net8.0-windows\scscd-gui.exe scscd-%VER%-ng\scscd-config.exe
rm scscd-%VER%-ng.7z
cd scscd-%VER%-ng
7z a -r ../scscd-%VER%-ng.7z .
cd ..

goto :eof

:usage
@echo Usage: %0 ^<version^>
exit /B 1
