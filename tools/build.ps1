param([string]$Toolchain = "$PSScriptRoot\..\.tools\CEdev")
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
$compilerRoot = (Resolve-Path $Toolchain).Path
if (!(Test-Path "$compilerRoot\bin\cedev-config.exe")) { throw 'Install CEdev v15 in .tools/CEdev or pass -Toolchain.' }
# CEdev makefiles require paths without spaces. Use a temporary free drive.
$drive = @('Q','R','S','T','U','V') | Where-Object { !(Test-Path "${_}:\") } | Select-Object -First 1
if (!$drive) { throw 'No free temporary drive letter Q..V.' }
$oldPath = $env:PATH
try {
    & subst "${drive}:" $projectRoot
    if ($LASTEXITCODE) { throw 'Could not map build directory.' }
    $relative = [IO.Path]::GetRelativePath($projectRoot, $compilerRoot)
    $mappedCompiler = "${drive}:\$relative"
    $env:PATH = "$mappedCompiler\bin;$oldPath"
    Push-Location "${drive}:\"
    try {
        & "$mappedCompiler\bin\make.exe" -j2
        if ($LASTEXITCODE) { throw "CE build failed ($LASTEXITCODE)." }
    } finally { Pop-Location }
} finally {
    $env:PATH = $oldPath
    & subst "${drive}:" /D
}
