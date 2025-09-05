# Development

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
