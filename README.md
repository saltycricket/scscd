# Wasteland Outfitter (aka SC Smart Clothing Distributor)

Are you a clothing-mod addict like me, but can't stand that almost nobody actually takes advantage of the variety you've made available to them? Look, I like playing dress-up as much as anyone, but I want the characters in the world to make their own darn choices! This bugged me for too long so I had to fix it.

Enter, Wasteland Outfitter. Now NPCs will dress themselves on their own, without any intervention by you. Well, it does a whole lot more, but that's the main thrust of this mod.


## Details

Please see https://www.nexusmods.com/fallout4/mods/89573 for details.


## Development

First you must update submodules:

  git submodule init --recursive
  git submodule update --recursive

Then you must install xmake **in Windows powershell**:
    > irm https://xmake.io/psget.text | iex

Then you must install vcpkg, which is in dep/vcpkg or at: https://github.com/microsoft/vcpkg
  > bootstrap-vcpkg.bat
  > vcpkg integrate install

After installing vcpkg you must install the project dependencies from this project root:
    > vcpkg install

Next you must build CommonLibF4:
    > cd dep/CommonLibF4
  > xmake -y

Notice CommonLibF4 .lib files are in dep/CommonLibF4/build/windows/x64/release.

Now you should be able to build the solution in MSVC++.
