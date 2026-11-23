# Publishing AURA-1

Repository: <https://github.com/ahaku/AURA-1>

Website: <https://ahaku.github.io/AURA-1/>

## One-time GitHub setup

1. Commit and push the prepared files to `main`, including the workflows and lockfile.
2. In **Settings → Actions → General**, allow the actions used by this repository
   and the reusable workflow `zvenson/fm1-build/.github/workflows/build.yml@v1`.
   The workflows request their required permissions; no personal access token or
   repository secret is needed. An organisation policy may need an administrator.
3. Change repository visibility to public when ready. GitHub Free cannot enable
   Pages for this repository while it is private (the Pages API returns HTTP 422).
4. In **Settings → Pages → Build and deployment → Source**, select **GitHub Actions**.
   This replaces any old `gh-pages` branch configuration. Keep the default
   `github-pages` environment restricted to `main` for deployment.
5. The `public` event starts the build. After enabling Pages, run or rerun
   **Actions → Build, test and deploy Pages** on `main` if needed.
   Deployment is skipped while the repository is private.
6. Check the successful Actions run and open the homepage, `/emu/`, `/manual/`
   and `/install/`. The site URL is also attached to the `github-pages` deployment.

## What runs automatically

`deploy-pages.yml` builds on pushes to `main`, pull requests targeting `main`,
the repository becoming public and manual runs:

- Installs the pinned Python and browser dependencies, JieLi's Linux toolchain,
  AC79 SDK at commit `d179b4484759423312073f5fbb232501aa491047`, and Emscripten 6.0.10.
- Builds a fresh development `.fwsc`, tests the engine, update path, host scenarios,
  emulator and standalone web player, and checks the documentation.
- Generates the site using that exact firmware. Checks local links, offline assets,
  installer version and bootloader checksum, then runs the browser regressions.
- Uploads `aura-1-development-firmware` with `.fwsc`, `.bin`, `.rfm1`, `firmware.json` and
  `SHA256SUMS`, test logs, and the `github-pages` site artifact.
- Deploys only successful builds of public `main`. Pull requests have no deployment
  permissions and never publish the site.

The web installer follows `main` and labels its firmware `dev`. It never silently
falls back to the bundled package if a build fails. Stable firmware is distributed
through tagged Releases; the bundled `firmware/aura-1.fwsc` is version 0.0.3.

To redeploy, run the workflow manually or use `sh tools/publish_pages.sh` /
`tools/publish_pages.ps1` after signing in to GitHub CLI. These helpers request the
same checked Actions pipeline. Local previews use `tools/make_pages.py` and
`tools/serve_site.py`; they do not publish.

If this repository is forked, change `repo`, `site`, `install` and `author` in
`app/fm1.toml`, regenerate `firmware.json`, and update the README links.
`FM1_REPO` and `FM1_SITE_URL` override generated site metadata; CI derives them
from the current GitHub repository. For a custom domain, set `FM1_SITE_URL` in the
workflow and configure the domain in Pages settings before deploying.

## Tagged firmware releases

Follow [BUILDING.md](BUILDING.md#publishing-a-release). Use a new tag such as
`v0.0.4`; each numeric component must fit in two digits. Test a local release build
on an FM-1 before tagging. A suffix such as `v0.0.4-beta` makes a prerelease.

The release workflow creates or reuses a draft, invokes the shared FM-1 builder,
and blocks on tests. It builds `.fwsc`, the app `.bin`, an `.rfm1` host package and
`firmware.json`; the builder adds SHA-256 checksums and source provenance.
The final job verifies all packages against the source tag, commit and builder
before publishing the draft. Published tags are never rebuilt. A failed run can
leave a draft for inspection; correct the cause before retrying.

The manual release workflow on a branch produces development artifacts without
creating a release. Attestations require a public repository for the supported
GitHub Free configuration. A release does not change the Pages deployment, which
continues to follow `main`.

## Before opening the repository

- Review the working tree and commit only source, documentation and intended assets.
  `build/`, `app/build/`, generated installers, credentials and local environments
  are ignored. Existing history will also become public.
- Preserve the MPL-2.0 license and upstream copyright notices. Third-party font,
  SDK and platform notices are listed in [LICENSING.md](LICENSING.md).
- Check that the exact `base/` and `base/bsp/` submodule commits can be fetched
  from their public repositories using `git clone --recurse-submodules`.
- Require the **build** job in the `main` ruleset if you want to prevent merging
  changes with failing builds. Enable GitHub secret scanning and push protection
  in repository settings where available.
- After the first hosted run, verify the firmware download, checksum and public
  URLs. Local checks cannot verify repository permissions or a hosted deployment.
