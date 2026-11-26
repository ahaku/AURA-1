#!/usr/bin/env python3
"""Check the assembled site before uploading it to GitHub Pages."""
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urljoin, urlsplit

from make_installer import package_info
from project_config import SITE


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.urls = []
        self.base = None

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "base":
            self.base = attrs.get("href")
        for attr in ("src", "href"):
            if attr in attrs and tag != "base":
                self.urls.append(attrs[attr])


def check_site(directory):
    root = Path(directory).resolve()
    errors = []
    required = ("index.html", "404.html", "install/index.html", "manual/index.html",
                "emu/index.html", "emu/app.wasm", "emu/sw.js", "emu/manifest.webmanifest",
                "assets/social-card.png", "sitemap.xml", "robots.txt", ".nojekyll")
    for name in required:
        if not (root / name).is_file():
            errors.append(f"Missing required file: {name}")

    def local_url(source, url, base=None):
        current = urljoin(SITE, source.relative_to(root).as_posix())
        resolved = urlsplit(urljoin(urljoin(current, base or ""), url))
        site = urlsplit(SITE)
        if resolved.scheme not in ("http", "https") or resolved.netloc != site.netloc:
            return None
        if not resolved.path.startswith(site.path):
            errors.append(f"{source.relative_to(root)}: link outside site prefix: {url}")
            return None
        target = (root / unquote(resolved.path[len(site.path):])).resolve()
        if not target.is_relative_to(root):
            errors.append(f"{source.relative_to(root)}: link outside output: {url}")
            return None
        if target.is_dir():
            target /= "index.html"
        if not target.is_file():
            errors.append(f"{source.relative_to(root)}: missing target: {url}")
        return target

    for page in root.rglob("*.html"):
        parser = Links()
        content = page.read_text(encoding="utf-8")
        parser.feed(content)
        for url in parser.urls:
            local_url(page, url, parser.base)
        if re.search(r"\{\{[A-Z_]+\}\}|__(?:VERSION|PKG|REPO|INSTALL_FIRMWARE_LABEL|DOWNLOAD_FIRMWARE_LABEL)__|/\*(?:LIB|META|APP)\*/", content):
            errors.append(f"{page.relative_to(root)}: unresolved template")

    worker = root / "emu/sw.js"
    if worker.is_file():
        content = worker.read_text(encoding="utf-8")
        assets = re.search(r"const PRECACHE_ASSETS\s*=\s*\[(.*?)\];", content, re.S)
        if not assets:
            errors.append("Missing emulator offline asset list")
        else:
            for url in re.findall(r"['\"]([^'\"]+)['\"]", assets[1]):
                local_url(worker, url)

    installer = root / "install/index.html"
    if installer.is_file():
        match = re.search(r"const meta = (\{.*?\});", installer.read_text(encoding="utf-8"))
        if not match:
            errors.append("Missing installer package metadata")
        else:
            meta = json.loads(match[1])
            package = local_url(installer, meta["pkg"])
            boot = local_url(installer, meta["boot"]["pkg"])
            if package and package.is_file():
                try:
                    _, product, _ = package_info(package, meta["version"])
                    if product != meta["product"]:
                        errors.append("Installer product does not match its firmware")
                except SystemExit as error:
                    errors.append(str(error))
            if boot and boot.is_file():
                import hashlib
                if hashlib.sha256(boot.read_bytes()).hexdigest() != meta["boot"]["sha256"]:
                    errors.append("Installer bootloader checksum does not match")
    return errors


if __name__ == "__main__":
    failures = check_site(sys.argv[1] if len(sys.argv) > 1 else "app/build/pages")
    if failures:
        raise SystemExit("\n".join(failures))
    print("Site checks passed: local links, offline assets and installer packages")
