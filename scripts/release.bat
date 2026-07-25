@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul

rem ============================================================
rem  KeyPresserNext 发布脚本：Release 编译 + windeployqt + Inno Setup
rem  用法：
rem    scripts\release.bat
rem    scripts\release.bat --skip-build     仅打包已有 release
rem    scripts\release.bat --skip-installer 只编译并产出 dist\package
rem  环境变量（可选覆盖）：
rem    QT_DIR   VCVARS   ISCC   BUILD_DIR
rem ============================================================

set "ROOT=%~dp0.."
pushd "%ROOT%" || exit /b 1
set "ROOT=%CD%"

if not defined QT_DIR set "QT_DIR=C:\Qt\5.15.2\msvc2019_64"
if not defined VCVARS set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined BUILD_DIR set "BUILD_DIR=%ROOT%\build-release"
if not defined ISCC set "ISCC="

set "SKIP_BUILD=0"
set "SKIP_INSTALLER=0"
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--skip-build" set "SKIP_BUILD=1"
if /i "%~1"=="--skip-installer" set "SKIP_INSTALLER=1"
if /i "%~1"=="-h" goto usage
if /i "%~1"=="--help" goto usage
shift
goto parse_args
:args_done

call :resolve_iscc
call :read_version
echo.
echo ========== KeyPresserNext 发布 ==========
echo 版本     : %APP_VERSION%
echo Qt       : %QT_DIR%
echo 构建目录 : %BUILD_DIR%
echo ISCC     : %ISCC%
echo ========================================
echo.

if not exist "%QT_DIR%\bin\qmake.exe" (
  echo [错误] 未找到 qmake：%QT_DIR%\bin\qmake.exe
  echo 请设置 QT_DIR 指向 Qt 5.15 MSVC 64-bit 目录。
  exit /b 1
)
if not exist "%ROOT%\third_party\qwindowkit-install\share\QWindowKit\qmake\QWKWidgets.pri" (
  echo [错误] 未找到 QWindowKit，请先运行 scripts\build_qwindowkit.bat
  exit /b 1
)

if "%SKIP_BUILD%"=="1" goto after_build

if not exist "%VCVARS%" (
  call :find_vcvars
)
if not exist "%VCVARS%" (
  echo [错误] 未找到 vcvars64.bat，请设置 VCVARS。
  exit /b 1
)

echo [1/5] 初始化 MSVC 环境...
call "%VCVARS%"
if errorlevel 1 (
  echo [错误] vcvars64 失败
  exit /b 1
)

echo [2/5] qmake...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"
"%QT_DIR%\bin\qmake.exe" "%ROOT%\KeyPresserNext.pro" -spec win32-msvc "CONFIG+=release" "CONFIG-=debug"
if errorlevel 1 (
  popd
  echo [错误] qmake 失败
  exit /b 1
)

echo [3/5] 编译 Release...
where jom >nul 2>&1
if not errorlevel 1 (
  jom -j %NUMBER_OF_PROCESSORS% release
) else if exist "C:\Qt\Tools\QtCreator\bin\jom.exe" (
  "C:\Qt\Tools\QtCreator\bin\jom.exe" -j %NUMBER_OF_PROCESSORS% release
) else (
  nmake release
)
if errorlevel 1 (
  popd
  echo [错误] 编译失败
  exit /b 1
)
popd

:after_build

set "EXE=%BUILD_DIR%\release\KeyPresserNextCommunity.exe"
if not exist "%EXE%" (
  rem 兼容在仓库 build\release 下已有产物
  if exist "%ROOT%\build\release\KeyPresserNextCommunity.exe" (
    set "BUILD_DIR=%ROOT%\build"
    set "EXE=%BUILD_DIR%\release\KeyPresserNextCommunity.exe"
  )
)
if not exist "%EXE%" (
  echo [错误] 未找到 %EXE%
  echo 请先完整编译，或去掉 --skip-build。
  exit /b 1
)

echo [4/5] 部署到 dist\package ...
set "PKG=%ROOT%\dist\package"
if exist "%PKG%" rmdir /s /q "%PKG%"
mkdir "%PKG%"

copy /Y "%EXE%" "%PKG%\KeyPresserNextCommunity.exe" >nul
if errorlevel 1 (
  echo [错误] 复制 exe 失败
  exit /b 1
)

rem 引擎 DLL（NextCore / NextBase）
copy /Y "%ROOT%\engine\runtime\bin\NextCore.dll" "%PKG%\" >nul
copy /Y "%ROOT%\engine\runtime\bin\tools.dll" "%PKG%\" >nul
copy /Y "%ROOT%\engine\runtime\bin\NextBase.dll" "%PKG%\" >nul

echo     windeployqt ...
"%QT_DIR%\bin\windeployqt.exe" --release --compiler-runtime --force ^
  "%PKG%\KeyPresserNextCommunity.exe"
if errorlevel 1 (
  echo [错误] windeployqt 失败
  exit /b 1
)

rem 已去掉 WebEngine：若残留旧产物则清理
del /Q "%PKG%\Qt5WebEngine*.dll" 2>nul
del /Q "%PKG%\QtWebEngineProcess*.exe" 2>nul
if exist "%PKG%\resources" (
  rem WebEngine 资源目录；无 WebEngine 时不应存在
  if not exist "%PKG%\Qt5WebEngineCore.dll" (
    rmdir /s /q "%PKG%\resources" 2>nul
  )
)

rem 更新日志（开源版不附带用户帮助文档）
if exist "%ROOT%\docs\更新日志.md" (
  mkdir "%PKG%\docs" 2>nul
  copy /Y "%ROOT%\docs\更新日志.md" "%PKG%\docs\" >nul
)

rem 清理无用调试符号，缩小体积
del /Q "%PKG%\*.pdb" 2>nul

echo     包内容已就绪：%PKG%

if "%SKIP_INSTALLER%"=="1" (
  echo.
  echo [--skip-installer] 已跳过安装包，可直接运行：
  echo   %PKG%\KeyPresserNextCommunity.exe
  goto done
)

if not exist "%ISCC%" (
  echo [错误] 未找到 Inno Setup 编译器 ISCC.exe
  echo 请安装 Inno Setup 6/7，或设置 ISCC 环境变量。
  echo 程序目录已生成，可稍后手动打包：%PKG%
  exit /b 1
)

echo [5/5] Inno Setup 打包...
if not exist "%ROOT%\dist\installer" mkdir "%ROOT%\dist\installer"
"%ISCC%" /DMyAppVersion=%APP_VERSION% /DPackageDir="%PKG%" "%ROOT%\installer\KeyPresserNext.iss"
if errorlevel 1 (
  echo [错误] ISCC 失败
  exit /b 1
)

echo.
echo ========== 完成 ==========
echo 绿色包  : %PKG%
echo 安装包  : %ROOT%\dist\installer\KeyPresserNext-%APP_VERSION%-setup.exe
echo ========================

:done
popd
endlocal
exit /b 0

:usage
echo 用法: scripts\release.bat [--skip-build] [--skip-installer]
exit /b 0

:read_version
set "APP_VERSION=1.0.0"
for /f "usebackq delims=" %%V in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0read_app_version.ps1" "%ROOT%\appversion.h"`) do set "APP_VERSION=%%V"
for /f "tokens=* delims= " %%A in ("%APP_VERSION%") do set "APP_VERSION=%%A"
if "%APP_VERSION%"=="" set "APP_VERSION=1.0.0"
exit /b 0

:resolve_iscc
if defined ISCC if exist "%ISCC%" exit /b 0
if exist "C:\Program Files\Inno Setup 7\ISCC.exe" set "ISCC=C:\Program Files\Inno Setup 7\ISCC.exe" & exit /b 0
if exist "C:\Program Files (x86)\Inno Setup 7\ISCC.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 7\ISCC.exe" & exit /b 0
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe" & exit /b 0
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe" & exit /b 0
where ISCC >nul 2>&1
if not errorlevel 1 (
  for /f "delims=" %%P in ('where ISCC') do set "ISCC=%%P" & exit /b 0
)
set "ISCC="
exit /b 0

:find_vcvars
for %%E in (Professional Enterprise Community BuildTools) do (
  if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
    exit /b 0
  )
)
for %%E in (Professional Enterprise Community BuildTools) do (
  if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat"
    exit /b 0
  )
)
exit /b 0
