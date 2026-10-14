param([string]$OutputDirectory = (Join-Path $env:TEMP 'rsmods-qc-regression'))
$ErrorActionPreference = 'Stop'
# Default keyword data must agree in runtime, both settings paths and GUI paths.
$defaults = Get-Content (Join-Path $PSScriptRoot 'qc-keyword-defaults.json') -Raw | ConvertFrom-Json
foreach ($relative in @('DLL/Mods/QCAutomation.cpp','DLL/Settings.cpp','GUI/UI.cs','GUI/WriteSettings.cs')) {
    $source = Get-Content (Join-Path $PSScriptRoot "../$relative") -Raw
    foreach ($entry in $defaults.PSObject.Properties) {
        $literal = '"' + $entry.Value + '"'
        $expected = if ($relative -eq 'DLL/Settings.cpp') { 2 } else { 1 }
        if ([regex]::Matches($source, [regex]::Escape($literal)).Count -ne $expected) {
            throw "QC keyword defaults diverged: $relative / $($entry.Name)"
        }
    }
}
Write-Output 'PASS: C++/C# keyword default parity'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ build tools not found.' }
$vcvars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars32.bat'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$sourcePath = Join-Path $PSScriptRoot 'QCAutomationRegression.cpp'
$exePath = Join-Path $outputPath 'qc-regression.exe'
$objPath = Join-Path $outputPath 'qc-regression.obj'
# cmd is used only to initialize the compiler environment and invoke the compiler.
& cmd.exe /d /c "`"$vcvars`" >nul && cl /nologo /std:c++17 /EHsc /W3 `"$sourcePath`" /Fe`"$exePath`" /Fo`"$objPath`""
if ($LASTEXITCODE -ne 0) { throw 'QC regression harness compilation failed.' }
Push-Location (Join-Path $PSScriptRoot '..')
try { & $exePath } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw 'QC regression checks failed.' }

$assetSource = Join-Path $PSScriptRoot 'QCAssetRegression.cpp'
$assetExe = Join-Path $outputPath 'qc-assets.exe'
$assetObj = Join-Path $outputPath 'qc-assets.obj'
$minizSource = Join-Path $PSScriptRoot '../DLL/Lib/Miniz/miniz_tinfl.c'
$minizObj = Join-Path $outputPath 'miniz.obj'
& cmd.exe /d /c "`"$vcvars`" >nul && cl /nologo /c /TC `"$minizSource`" /Fo`"$minizObj`" && cl /nologo /std:c++17 /EHsc /W3 /DNOMINMAX `"$assetSource`" `"$minizObj`" /Fe`"$assetExe`" /Fo`"$assetObj`""
if ($LASTEXITCODE -ne 0) { throw 'QC asset harness compilation failed.' }
& $assetExe
if ($LASTEXITCODE -ne 0) { throw 'QC asset regression checks failed.' }

$timerSource = Join-Path $PSScriptRoot 'SongTimerRegression.cpp'
$timerExe = Join-Path $outputPath 'song-timer.exe'
$timerObj = Join-Path $outputPath 'song-timer.obj'
& cmd.exe /d /c "`"$vcvars`" >nul && cl /nologo /std:c++20 /EHsc /W3 `"$timerSource`" /Fe`"$timerExe`" /Fo`"$timerObj`""
if ($LASTEXITCODE -ne 0) { throw 'Song timer regression compilation failed.' }
& $timerExe
if ($LASTEXITCODE -ne 0) { throw 'Song timer regression checks failed.' }
