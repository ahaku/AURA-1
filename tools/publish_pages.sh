#!/bin/sh
# Request the checked GitHub Actions build and deployment from main.
set -eu
cd "$(dirname "$0")/.."
command -v gh >/dev/null 2>&1 || { echo 'Install GitHub CLI and run gh auth login first.' >&2; exit 1; }
gh workflow run deploy-pages.yml --ref main
echo 'Build and deployment requested. Follow it with: gh run list --workflow deploy-pages.yml'
