param(
    [string]$Package = "firmware/aura-1.fwsc",
    [string]$Version = "",
    [string]$Emsdk = $env:EMSDK,
    [string]$Python = "python"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
& "$PSScriptRoot/build_web.ps1" -Emsdk $Emsdk -Python $Python
& $Python tools/make_pages.py $Package $Version app/build/pages
if ($LASTEXITCODE -ne 0) { throw "Site generation failed" }
& $Python tools/check_site.py app/build/pages
if ($LASTEXITCODE -ne 0) { throw "Site validation failed" }
Write-Host "Site ready at $Root/app/build/pages"
Write-Host "Preview: $Python tools/serve_site.py --port 8080"
