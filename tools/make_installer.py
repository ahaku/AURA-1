#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Build the AURA-1 Web MIDI installer in the shared studio layout."""

import hashlib
import json
import shutil
import sys
import re
from pathlib import Path
from project_config import REPO

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "base"
INSTALL_DIR = ROOT / "install"
FIRMWARE_DIR = ROOT / "firmware"
DEFAULT_PACKAGE = FIRMWARE_DIR / "aura-1.fwsc"


def package_info(package, version=None):
    sys.path.insert(0, str(BASE / "tools"))
    from make_site import product_of, read_conf
    if not package.is_file():
        raise SystemExit(f"Missing AURA-1 firmware package: {package}")
    conf = read_conf(ROOT / "app" / "fm1.toml")
    product = product_of(package.read_bytes())
    prefix = conf["id_prefix"]
    match = re.fullmatch(re.escape(prefix) + r"(\d\d)(\d\d)(\d\d)", product)
    if not match and product != prefix + "00":
        raise SystemExit(f"Invalid AURA-1 firmware identity: {product!r}")
    if version:
        release = re.fullmatch(r"(\d{1,2})\.(\d{1,2})(?:\.(\d{1,2}))?(?:-[A-Za-z0-9]+)?", version)
        if version == "dev":
            valid = product == prefix + "00"
        else:
            valid = bool(release and match and
                         tuple(map(int, match.groups())) ==
                         (int(release[1]), int(release[2]), int(release[3] or 0)))
        if not valid:
            raise SystemExit(f"Version {version!r} does not match firmware identity {product!r}")
    if not version:
        if match:
            major, minor, patch = map(int, match.groups())
            version = f"{major}.{minor}.{patch}" if patch else f"{major}.{minor}"
        elif product == prefix + "00":
            version = "dev"
    return conf, product, version


def strip_module(src: str) -> str:
    """Strip ES module export/import to allow inlining."""
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def build_installer(package=DEFAULT_PACKAGE, version=None, pkg_name="aura-1.fwsc"):
    conf, product, version = package_info(Path(package), version)
    INSTALL_DIR.mkdir(parents=True, exist_ok=True)
    FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    raw_html = (BASE / "web" / "installer.html").read_text(encoding="utf-8")
    pkg_js = (BASE / "web" / "fm1pkg.js").read_text(encoding="utf-8")
    ota_js = (BASE / "web" / "fm1ota.js").read_text(encoding="utf-8")

    ports_js = (BASE / "web" / "fm1ports.js").read_text(encoding="utf-8")
    lib = "\n".join(strip_module(js) for js in (ports_js, pkg_js, ota_js))
    from make_site import MBOOT_VERSION
    boot = BASE / "web" / "m-bootloader" / f"fm1-uboot-{MBOOT_VERSION}.fwsc"
    boot_name = f"m-bootloader-{MBOOT_VERSION}.fwsc"
    shutil.copy(boot, FIRMWARE_DIR / boot_name)
    shutil.copy(boot.parent / "LICENSE", FIRMWARE_DIR / "m-bootloader-LICENSE.txt")
    meta = json.dumps({
        "version": version,
        "product": product,
        "backup": bool(conf.get("backup")),
        "boot": {"pkg": "../firmware/" + boot_name, "version": MBOOT_VERSION,
                 "sha256": hashlib.sha256(boot.read_bytes()).hexdigest()},
        "pkg": "../firmware/" + pkg_name
    })

    # Clean up redundant & obsolete elements from upstream template
    # Keep the template's language state and DOM hooks; hide the switch in the AURA theme.
    html = re.sub(r'(<button type="button" class="lang")', r'\1 hidden style="display:none"', raw_html)
    # 3. Remove floating brick & back links from above Install button
    html = re.sub(r'<p class="small"><span data-t="brick"></span>.*?</p>\s*<p class="small" data-t="back"></p>\s*<p class="small"><a href="https://www.m-vave.com/download".*?</p>\s*', '', html, flags=re.DOTALL)
    # Keep mbootopt: the current installer offers the bundled bootloader after installation.
    # 5. Remove empty/dead backup box on main page
    html = re.sub(r'<p class="backup">.*?</p>\s*(?=<p><button type="button" id="go")', '', html, flags=re.DOTALL)
    # 6. Remove backup box from confirm dialog
    html = re.sub(r'(<dialog id="confirm".*?<h2 id="confirmh".*?</h2>\s*)<p class="backup">.*?</p>\s*', r'\1', html, flags=re.DOTALL)

    html = html.replace("/*LIB*/", lib)
    html = html.replace("/*META*/", meta)
    app_js = (ROOT / "app" / conf["install_app"]).read_text(encoding="utf-8") if conf.get("install_app") else "null"
    html = html.replace("/*APP*/", app_js)

    replacements = {
        "NAME": "AURA-1",
        "REPO": REPO,
        "REPO_SHORT": REPO.removeprefix("https://"),
        "LICENSE": "MPL-2.0",
        "RES": "resources",
        "RES_JA": "リソース",
        "CREDITS": " Built on fm1-amb by Charles Vestal, fm1-base and felucca-bsp."
    }
    for k, v in replacements.items():
        html = html.replace("{{" + k + "}}", v)

    from studio_site import restyle_installer
    html = restyle_installer(html, version)

    # Clean activation script: smoothly reveals progress bar when flashing starts
    reveal_script = """
<script>
(() => {
  const bar = document.getElementById("bar");
  const status = document.getElementById("status");
  if (!bar) return;
  function showBar() {
    bar.classList.add("active");
  }
  const desc = Object.getOwnPropertyDescriptor(HTMLProgressElement.prototype, "value");
  if (desc && desc.set) {
    Object.defineProperty(bar, "value", {
      set: function(v) {
        if (v > 0) showBar();
        return desc.set.call(this, v);
      },
      get: function() {
        return desc.get.call(this);
      }
    });
  }
  if (status) {
    const obs = new MutationObserver(() => {
      const txt = (status.textContent || "").trim();
      if (txt && !txt.startsWith("AURA-1")) {
        showBar();
      }
    });
    obs.observe(status, { childList: true, characterData: true, subtree: true });
  }
  document.getElementById("go")?.addEventListener("click", showBar);
  document.getElementById("stockgo")?.addEventListener("click", showBar);
  document.getElementById("confirmgo")?.addEventListener("click", showBar);
})();
</script>
"""
    html = html.replace("</body>", f"{reveal_script}\n</body>")

    out_file = INSTALL_DIR / "index.html"
    out_file.write_text(html, encoding="utf-8")
    print(f"Generated {out_file} ({len(html)} bytes)")


if __name__ == "__main__":
    build_installer()
