# Build and test browser targets on Windows. EMSDK may point to any SDK checkout.
param(
    [string]$Emsdk = $env:EMSDK,
    [string]$Python = "python"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONUTF8 = "1"
if (-not $Emsdk) {
    $SiblingSdk = Join-Path (Split-Path -Parent $Root) "emsdk"
    if (Test-Path (Join-Path $SiblingSdk "upstream/emscripten")) { $Emsdk = $SiblingSdk }
}
if ($Emsdk) {
    $Emsdk = (Resolve-Path -LiteralPath $Emsdk).Path
    $env:PATH = "$(Join-Path $Emsdk 'upstream/emscripten');$(Join-Path $Emsdk 'upstream/bin');$env:PATH"
    $env:EM_CONFIG = Join-Path $Emsdk ".emscripten"
}
function Invoke-Native {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
Invoke-Native $Python @("-c", "from PIL import features; assert features.check('raqm'), 'Install FriBiDi (fribidi.dll) on PATH for Pillow; see BUILDING.md'")
Invoke-Native $Python @("base/tools/build.py", "app", "--web")
Invoke-Native $Python @("tools/studio_site.py")
New-Item -ItemType Directory -Path "app/build/app" -Force | Out-Null
Invoke-Native "emcc" @(
    "-O2", "-ffp-contract=off", "-std=gnu99", "-Wall", "-Wextra", "-Wno-unused-function",
    "-DEN_HOST", "-Iapp/dsp", "-Ibase/include", "--no-entry", "-sSTANDALONE_WASM",
    "-sSTACK_SIZE=262144", "-sINITIAL_MEMORY=8388608", "-sFILESYSTEM=0",
    "-o", "app/build/app/amb.wasm", "web/app/amb_api.c", "app/dsp/amb.c"
)
Copy-Item "web/app/index.html", "web/app/worklet.js" "app/build/app/" -Force
Invoke-Native "node" @("tests/host/emu_test.mjs", "app/build/emu/app.wasm")
Invoke-Native "node" @("tests/host/app_test.mjs", "app/build/app/amb.wasm")
Write-Host "Browser builds and tests passed: app/build/emu and app/build/app"
