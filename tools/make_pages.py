#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Make the AURA-1 site for GitHub Pages.

  index.html                  AURA-1 Quiet studio landing page
  img/                        screenshots (docs/img)
  emu/                        AURA-1 in the browser (app/build/emu, from tools/build_web.ps1 or ./build.sh --web)
  words/                      the screen reader's words files (app/web/words, from app/words.tsv)
  install/index.html          Web MIDI installer in the shared studio layout
  manual/index.html, img/     Illustrated studio guide (tools/theme_manual.py)
  firmware/aura-VERSION.fwsc  packaged firmware

Usage:
  tools/make_pages.py [PKG] [VERSION] [OUT_DIR]
"""
import html
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from make_installer import DEFAULT_PACKAGE, build_installer, package_info
from project_config import REPO

SRC = Path(__file__).resolve().parents[1]
BASE = SRC / "base"

LANDING = (SRC / "web/site/index.html").read_text(encoding="utf-8")



def main(pkg=None, version=None, out=None):
    pkg_path = Path(pkg) if pkg else DEFAULT_PACKAGE
    conf, _, version = package_info(pkg_path, version)
    pkg_filename = f"aura-{version}.fwsc"
    repo = REPO
    name = conf.get("name", "AURA-1")

    out = Path(out) if out else (SRC / "app" / "build" / "pages")
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware"):
        (out / d).mkdir(parents=True)

    shutil.copy(pkg_path, out / "firmware" / pkg_filename)

    # 1. Web Installer
    build_installer(package=pkg_path, version=version, pkg_name=pkg_filename)
    for boot_file in (SRC / "firmware").glob("m-bootloader-*"):
        shutil.copy(boot_file, out / "firmware" / boot_file.name)
    shutil.copy(SRC / "install" / "index.html", out / "install" / "index.html")

    # 2. The illustrated guide
    manual_html_py = BASE / "tools" / "manual_html.py"
    sub_env = {**os.environ, "PYTHONUTF8": "1"}
    if manual_html_py.exists():
        subprocess.run([sys.executable, str(manual_html_py), str(SRC / "docs" / "MANUAL.md"),
                        str(out / "manual" / "index.html"), "--name", name, "--repo", repo,
                        "--tagline", conf.get("tagline", "")], env=sub_env, check=True, capture_output=True)
        theme_script = SRC / "tools" / "theme_manual.py"
        if theme_script.exists():
            subprocess.run([sys.executable, str(theme_script), str(out / "manual" / "index.html")], env=sub_env, check=True)

    if (SRC / "docs" / "img").exists():
        from studio_site import copy_manual_images
        copy_manual_images(out / "manual/img")
        copy_manual_images(out / "img")

    from studio_site import copy_assets, header, footer, head
    copy_assets(out)

    # 3. The Landing Page
    install_label = "Install development firmware" if version == "dev" else f"Install firmware v{version}"
    download_label = "Development firmware" if version == "dev" else f"Firmware v{version}"
    landing_content = LANDING.replace("__INSTALL_FIRMWARE_LABEL__", html.escape(install_label)) \
                             .replace("__DOWNLOAD_FIRMWARE_LABEL__", html.escape(download_label)) \
                             .replace("__PKG__", pkg_filename) \
                             .replace("__REPO__", repo)
    landing_content = landing_content.replace("__STUDIO_HEAD__", head("AURA-1 turns the M-VAVE FM-1 into a generative ambient instrument. Listen in your browser, play along, or install the open firmware."))
    landing_content = landing_content.replace("__STUDIO_HEADER__", header()).replace("__STUDIO_FOOTER__", footer())
    (out / "index.html").write_text(landing_content, encoding="utf-8")
    from studio_site import SITE
    error_page = (SRC / "web/site/404.html").read_text(encoding="utf-8")
    error_page = error_page.replace("<head>", '<head><base href="' + SITE + '">')
    error_page = error_page.replace("__STUDIO_HEAD__", head("This AURA-1 page could not be found. Return to the studio, instrument or illustrated guide.", "404.html"))
    error_page = error_page.replace("__STUDIO_HEADER__", header()).replace("__STUDIO_FOOTER__", footer()).replace("__UP__", "./")
    (out / "404.html").write_text(error_page, encoding="utf-8")

    # 4. Emulator PWA
    emu_src = SRC / "app/build/emu" if (SRC / "app/build/emu/app.wasm").exists() else SRC / "emu"
    if emu_src.exists():
        shutil.copytree(emu_src, out / "emu", dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("o2", "web", "*.wav", "rain-*.html", "three.min.js", "aura-shader.js", "gsap.min.js", "GSAP-NOTICE.md"))
        from studio_site import prepare_emulator
        prepare_emulator(out / "emu")
        # Inject build timestamp into Service Worker for seamless PWA cache updates
        sw_out = out / "emu" / "sw.js"
        if sw_out.exists():
            import datetime
            ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d-%H%M%S")
            sw_code = sw_out.read_text(encoding="utf-8")
            sw_code = re.sub(r"const CACHE_NAME = ['\"][^'\"]+['\"];",
                             f"const CACHE_NAME = 'aura-v{version}-{ts}';",
                             sw_code)
            sw_out.write_text(sw_code, encoding="utf-8")

    # 5. Words for fm1-reader
    if (SRC / "app" / "web" / "words").exists():
        shutil.copytree(SRC / "app" / "web" / "words", out / "words", dirs_exist_ok=True)

    paths = ["", "emu/", "manual/", "install/"]
    from studio_site import SITE
    (out / "robots.txt").write_text(f"User-agent: *\nAllow: /\nSitemap: {SITE}sitemap.xml\n", encoding="utf-8")
    (out / "sitemap.xml").write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">' +
                                     ''.join(f'<url><loc>{SITE}{path}</loc></url>' for path in paths) + '</urlset>', encoding="utf-8")
    # Retire the worker from the earlier local emulator-at-root server layout.
    (out / "sw.js").write_text("self.addEventListener('install',()=>self.skipWaiting());\nself.addEventListener('activate',event=>event.waitUntil(self.registration.unregister()));\n", encoding="utf-8")
    # 6. GitHub Pages .nojekyll
    (out / ".nojekyll").write_text("")

    print(f"Site generated successfully in {out}")


if __name__ == "__main__":
    pkg_arg = sys.argv[1] if len(sys.argv) > 1 else None
    ver_arg = sys.argv[2] if len(sys.argv) > 2 else None
    out_arg = sys.argv[3] if len(sys.argv) > 3 else str(SRC / "app" / "build" / "pages")
    main(pkg_arg, ver_arg, out_arg)
