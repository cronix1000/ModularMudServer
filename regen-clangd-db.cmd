@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake -S . -B build-clangd -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_COMPILER="C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin/clang-cl.exe" -DCMAKE_TOOLCHAIN_FILE=C:/Users/idang/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=%CD%/vcpkg_installed/x64-windows -DCMAKE_BUILD_TYPE=Release
