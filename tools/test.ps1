$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install Visual Studio C++ Build Tools for portable engine tests.' }
New-Item -ItemType Directory -Force "$projectRoot\build\tests" | Out-Null
$batch = @"
@echo off
call "$vs\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "$projectRoot"
cl /nologo /std:c11 /W4 /D_CRT_SECURE_NO_WARNINGS /I src /Fe:build\tests\tests.exe /Fo:build\tests\ src\game.c src\level.c src\storage.c src\editor.c src\content.c tests\test_main.c tests\test_game.c tests\test_editor.c tests\test_storage.c tests\test_content.c tests\test_regression.c
if errorlevel 1 exit /b 1
build\tests\tests.exe
exit /b %errorlevel%
"@
$batch | Set-Content -Encoding ascii "$projectRoot\build\tests\run.cmd"
& cmd.exe /d /c "$projectRoot\build\tests\run.cmd"
if ($LASTEXITCODE) { throw "Host tests failed ($LASTEXITCODE)." }
