[CmdletBinding()]
param([switch]$RegressionOnly)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runName = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 6)
$runDirectory = Join-Path $repoRoot "artifacts/qc/$runName"
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
$manifestPath = Join-Path $runDirectory 'manifest.json'
$manifest = [ordered]@{
    StartedAt = (Get-Date).ToString('o')
    Repository = $repoRoot
    Status = 'Running'
    Configuration = 'Release'
    Platform = 'Win32'
    Regression = 'NotRun'
    Build = 'NotRun'
    Installed = $false
    Commands = @()
}

function Get-QcInputFingerprint {
    $files = @(& git -C $repoRoot ls-files --cached --others --exclude-standard)
    if ($LASTEXITCODE -ne 0) { throw 'Unable to enumerate source inputs with Git.' }
    @($files | Sort-Object -Unique | Where-Object {
        $_ -match '^(DLL/|GUI/|tests/|tools/|RSMods\.sln$)' -and
        (Test-Path -LiteralPath (Join-Path $repoRoot $_) -PathType Leaf)
    } | ForEach-Object {
        [ordered]@{ Path = $_; SHA256 = (Get-FileHash -LiteralPath (Join-Path $repoRoot $_) -Algorithm SHA256).Hash }
    })
}

try {
    $manifest.Revision = (& git -C $repoRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Unable to identify source revision.' }
    $manifest.WorkingTree = @(& git -C $repoRoot status --short)
    $inputsBefore = @(Get-QcInputFingerprint)
    $manifest.Inputs = $inputsBefore
    $testScript = Join-Path $repoRoot 'tests/run-qc-regression.ps1'
    $testOutput = Join-Path $runDirectory 'tests'
    $manifest.Commands += "powershell -NoProfile -ExecutionPolicy Bypass -File `"$testScript`" -OutputDirectory `"$testOutput`""
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $testScript -OutputDirectory $testOutput *> (Join-Path $runDirectory 'regression.log')
    if ($LASTEXITCODE -ne 0) { $manifest.Regression = 'Failed'; throw 'QC regression checks failed; inspect regression.log.' }
    $manifest.Regression = 'Passed'

    if (-not $RegressionOnly) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        if (-not $msbuild) { throw 'Visual Studio MSBuild and C++ tools are required.' }
        $output = Join-Path $runDirectory 'bin'
        $intermediate = Join-Path $runDirectory 'obj'
        $buildArguments = @(
            (Join-Path $repoRoot 'DLL/DLL.vcxproj'), '/t:Build', '/p:Configuration=Release', '/p:Platform=Win32',
            '/p:PostBuildEventUseInBuild=false', '/p:PreBuildEventUseInBuild=false',
            "/p:OutDir=$output\", "/p:IntDir=$intermediate\", '/m', '/nologo', '/v:minimal'
        )
        $manifest.Commands += (@($msbuild) + $buildArguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        & $msbuild @buildArguments *> (Join-Path $runDirectory 'build.log')
        if ($LASTEXITCODE -ne 0) { $manifest.Build = 'Failed'; throw 'DLL build failed; inspect build.log.' }
        $dll = Join-Path $output 'xinput1_3.dll'
        $bytes = [IO.File]::ReadAllBytes($dll)
        if ($bytes.Length -lt 64) { throw 'Invalid DLL header.' }
        $pe = [BitConverter]::ToInt32($bytes, 60)
        if ($pe -lt 0 -or $pe + 24 -gt $bytes.Length -or
            [BitConverter]::ToUInt32($bytes, $pe) -ne 17744 -or
            [BitConverter]::ToUInt16($bytes, $pe + 4) -ne 332 -or
            ([BitConverter]::ToUInt16($bytes, $pe + 22) -band 8192) -eq 0) {
            throw 'Build output is not an x86 DLL.'
        }
        $guiOutput = Join-Path $runDirectory 'gui'
        $guiIntermediate = Join-Path $runDirectory 'gui-obj'
        $guiArguments = @((Join-Path $repoRoot 'GUI/GUI.csproj'), '/restore', '/t:Build', '/p:Configuration=Release', '/p:Platform=x64',
            '/p:PostBuildEvent=', '/p:PreBuildEvent=', "/p:OutputPath=$guiOutput\", "/p:IntermediateOutputPath=$guiIntermediate\",
            "/p:BaseIntermediateOutputPath=$guiIntermediate\", '/nologo', '/v:minimal')
        $manifest.Commands += (@($msbuild) + $guiArguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        & $msbuild @guiArguments *> (Join-Path $runDirectory 'gui-build.log')
        if ($LASTEXITCODE -ne 0) { throw 'GUI build failed; inspect gui-build.log.' }
        $guiExe = Join-Path $guiOutput 'RSMods.exe'
        if (!(Test-Path -LiteralPath $guiExe)) { throw 'GUI executable missing.' }
        $guiBytes = [IO.File]::ReadAllBytes($guiExe)
        $guiPe = [BitConverter]::ToInt32($guiBytes, 60)
        if ([BitConverter]::ToUInt16($guiBytes, $guiPe + 4) -ne 34404) { throw 'GUI output is not x64.' }
        $manifest.GUIDependencies = @(Get-ChildItem -LiteralPath $guiOutput -Filter '*.dll' | ForEach-Object {
            [ordered]@{Name=$_.Name; Path=$_.FullName; SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
        })
        $manifest.GUIArtifacts = @('RSMods.exe','RSMods.exe.config') | ForEach-Object {
            $artifact = Join-Path $guiOutput $_
            if (Test-Path -LiteralPath $artifact) { [ordered]@{Name=$_; Path=$artifact; SHA256=(Get-FileHash -LiteralPath $artifact).Hash} }
        }
        $csc = Join-Path (Split-Path $msbuild) 'Roslyn/csc.exe'
        $jsonDll = Join-Path $repoRoot 'GUI/Lib/Newtonsoft.Json.dll'
        $csTest = Join-Path $testOutput 'qc-overrides-csharp.exe'
        & $csc /nologo "/r:$jsonDll" "/out:$csTest" (Join-Path $repoRoot 'GUI/QCOverrides.cs') (Join-Path $repoRoot 'tests/QCOverridesRegression.cs') *> (Join-Path $runDirectory 'gui-tests.log')
        if ($LASTEXITCODE -ne 0) { throw 'C# test compilation failed.' }
        Copy-Item -LiteralPath $jsonDll -Destination $testOutput
        & $csTest (Join-Path $repoRoot 'tests/qc-overrides-fixtures.json') *>> (Join-Path $runDirectory 'gui-tests.log')
        if ($LASTEXITCODE -ne 0) { throw 'C# shared fixture tests failed.' }
        $manifest.GUIRegression = 'Passed'
        $manifest.Build = 'Passed'
        $manifest.DLL = $dll
        $manifest.SHA256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    }

    $inputsAfter = @(Get-QcInputFingerprint)
    if (($inputsBefore | ConvertTo-Json -Depth 4 -Compress) -ne ($inputsAfter | ConvertTo-Json -Depth 4 -Compress)) {
        throw 'Source inputs changed during verification. Rerun before deploying.'
    }
    $manifest.Status = 'Passed'
} catch {
    $manifest.Status = 'Failed'
    $manifest.Error = $_.Exception.Message
    throw
} finally {
    $manifest.CompletedAt = (Get-Date).ToString('o')
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Write-Host "QC verification manifest: $manifestPath"
}
Write-Output ([pscustomobject]@{ Status = $manifest.Status; Regression = $manifest.Regression; Build = $manifest.Build; Manifest = $manifestPath; DLL = $manifest.DLL; SHA256 = $manifest.SHA256 })
