# Contributing to AURA-1

Clone with `git clone --recurse-submodules https://github.com/ahaku/AURA-1`.
See [BUILDING.md](BUILDING.md) for dependencies and local builds.

Open an issue with the firmware version, FM-1 model, steps to reproduce and
expected behaviour. For browser problems, include the browser and OS versions
and relevant console errors. Remove personal data from logs before attaching them.

For a code change, explain the resulting behaviour and run `sh tests/run_tests.sh`.
Firmware changes should also pass `./build.sh` and be tried on hardware when
possible. Site changes should pass `python3 tools/check_site.py app/build/pages`
and the browser regression check documented in BUILDING.md. State any checks
you could not run. CI checks pull requests before deployment.

Keep submodules at explicit commits and retain upstream license and copyright
notices. Contributions to MPL-covered files remain under MPL-2.0.
Do not commit generated builds, credentials or local SDK/toolchain files.
