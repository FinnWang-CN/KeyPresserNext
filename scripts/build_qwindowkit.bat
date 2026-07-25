@echo off
setlocal
set QT_DIR=C:\Qt\5.15.2\msvc2019_64
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat
set ROOT=%~dp0..
set SRC=%ROOT%\third_party\qwindowkit
set BUILD=%ROOT%\third_party\qwindowkit-build
set INSTALL=%ROOT%\third_party\qwindowkit-install

if not exist "%SRC%\CMakeLists.txt" (
  echo Cloning QWindowKit...
  mkdir "%ROOT%\third_party" 2>nul
  git clone --recursive --depth 1 https://ghproxy.net/https://github.com/stdware/qwindowkit.git "%SRC%"
  if errorlevel 1 exit /b 1
)

call "%VCVARS%"
if errorlevel 1 exit /b 1

cmake -B "%BUILD%" -S "%SRC%" ^
  -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
  -DQWINDOWKIT_BUILD_QUICK=OFF ^
  -DQWINDOWKIT_BUILD_EXAMPLES=OFF ^
  -DQWINDOWKIT_BUILD_DOCUMENTATIONS=OFF ^
  -DQWINDOWKIT_BUILD_STATIC=ON ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL%" ^
  -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

cmake --build "%BUILD%" --config Release --target install -j 8
if errorlevel 1 exit /b 1
cmake --build "%BUILD%" --config Debug --target install -j 8
if errorlevel 1 exit /b 1

echo QWindowKit installed to %INSTALL%
endlocal
