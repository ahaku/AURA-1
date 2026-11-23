# Request the checked GitHub Actions build and deployment from main.
$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)
& gh workflow run deploy-pages.yml --ref main
if ($LASTEXITCODE -ne 0) { throw "Could not start the Pages workflow" }
Write-Host "Build and deployment requested. Follow it with: gh run list --workflow deploy-pages.yml"
