"""Shared presentation and portable assets for the AURA-1 static site."""
from pathlib import Path
import html
import hashlib
import json
import os
import re
import shutil
from project_config import REPO, SITE

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "web" / "site"


def header(current="", up=""):
    links = [("emu/", "Play", "play"), ("manual/", "Guide", "manual"), ("install/", "Install", "install")]
    nav = ""
    for url, label, key in links:
        active = ' aria-current="page"' if current == key else ''
        nav += f'<a href="{up}{url}"{active}><span>{label}</span></a>'
    return (f'<header class="studio-header"><a class="studio-brand" href="{up or "./"}" aria-label="AURA-1 home">'
            '<span class="brand-seed" aria-hidden="true"><i></i><i></i><i></i><i></i></span>AURA-1</a>'
            '<span class="studio-edition">A generative instrument for the FM-1</span>'
            f'<nav aria-label="Main navigation">{nav}</nav></header>')


def footer(up=""):
    return (f'<footer class="studio-footer"><a class="footer-brand" href="{up or "./"}">AURA-1<span>Let it grow.</span></a>'
            f'<div class="footer-links"><a href="{up}emu/">Play in your browser</a><a href="{up}manual/">Read the guide</a>'
            f'<a href="{html.escape(REPO, quote=True)}">Source on GitHub</a></div>'
            '<p>Open community firmware for the M-VAVE FM-1. Built on fm1-amb, fm1-base and felucca-bsp. '
            'M-VAVE and FM-1 are trademarks of their owners. AURA-1 is not affiliated with M-VAVE.</p>'
            '<p class="footer-license"><a href="https://www.mozilla.org/MPL/2.0/">MPL-2.0</a> · '
            'Manrope by Mikhail Sharanda, SIL Open Font License.</p></footer>')


def head(description, route="", assets="assets/"):
    description = html.escape(description, quote=True)
    canonical = SITE + route
    script_version = hashlib.sha256((ASSETS / "site.js").read_bytes()).hexdigest()[:12]
    icon_version = hashlib.sha256((ASSETS / "icon.svg").read_bytes()).hexdigest()[:12]
    return (f'<meta name="description" content="{description}">\n'
            '<meta name="theme-color" content="#f2f0e7">\n'
            f'<link rel="canonical" href="{canonical}">\n'
            '<meta property="og:type" content="website">\n'
            '<meta property="og:title" content="AURA-1 | Music that grows itself">\n'
            f'<meta property="og:description" content="{description}">\n'
            f'<meta property="og:url" content="{canonical}">\n'
            f'<meta property="og:image" content="{SITE}assets/social-card.png">\n'
            '<meta name="twitter:card" content="summary_large_image">\n'
            f'<link rel="icon" href="{assets}favicon.ico?v={icon_version}" sizes="16x16 32x32 48x48">\n'
            f'<link rel="icon" href="{assets}icon.svg?v={icon_version}" type="image/svg+xml" sizes="any">\n'
            f'<link rel="apple-touch-icon" href="{assets}apple-touch-icon.png?v={icon_version}">\n'
            f'<link rel="preload" href="{assets}fonts/manrope-400_800-normal.woff2" as="font" type="font/woff2" crossorigin>\n'
            f'<link rel="stylesheet" href="{assets}studio.css">\n'
            f'<script src="{assets}site.js?v={script_version}" defer></script>\n' +
            '<script type="application/ld+json">' + json.dumps({'@context':'https://schema.org','@type':'WebApplication','name':'AURA-1','url':SITE+'emu/','applicationCategory':'MultimediaApplication','operatingSystem':'Web browser'}) + '</script>\n')


def copy_assets(destination):
    target = Path(destination) / "assets"
    shutil.copytree(ASSETS, target, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("index.html", "404.html", "og.html"))


def copy_manual_images(destination):
    """Lossless output compression; the original device screenshots stay untouched."""
    from PIL import Image
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / "docs/img").iterdir():
        if not source.is_file():
            continue
        target = destination / source.name
        if source.suffix.lower() == ".png":
            with Image.open(source) as image:
                image.save(target, format="PNG", optimize=True)
        else:
            shutil.copy2(source, target)


def prepare_emulator(destination=None):
    destination = Path(destination) if destination else ROOT / "app/build/emu"
    destination.mkdir(parents=True, exist_ok=True)
    copy_assets(destination)
    for name in ("sw.js", "manual.html", "manifest.webmanifest", "favicon.ico", "icon.svg", "icon-192.png", "icon-512.png"):
        source = ROOT / "app/web/emu" / name
        if source.is_file():
            shutil.copy2(source, destination / name)
    copy_manual_images(destination / "img")
    shutil.copytree(ROOT / "app/web/words", destination / "words", dirs_exist_ok=True)
    from theme_manual import render_manual
    (destination / "manual.html").write_text(render_manual(portable=True), encoding="utf-8")
    worker = (destination / "sw.js").read_text(encoding="utf-8")
    word_assets = ",".join("'words/" + path.name + "'" for path in sorted((destination / "words").glob("*.json")))
    worker = worker.replace("/* WORD_FILES */", word_assets)
    # The app has no emulator parts: retain the loader hooks without failed requests.
    (destination / "app.js").write_text("// AURA-1 has no additional emulator parts.\n", encoding="utf-8")
    (destination / "app_worklet.js").write_text("// AURA-1's processor is worklet.js.\n", encoding="utf-8")
    if (destination / "app.wasm").exists():
        shutil.copy2(destination / "app.wasm", destination / "amb.wasm")
    index = destination / "index.html"
    if index.exists():
        page = index.read_text(encoding="utf-8")
        # A controlled embed may still use an older offline cache. Content
        # versions also make ordinary browser caches request changed assets.
        for asset in ("assets/studio-emu.css", "assets/studio-controls.js", "assets/site.js", "icon.svg", "favicon.ico"):
            digest = hashlib.sha256((destination / asset).read_bytes()).hexdigest()[:12]
            versioned = asset + "?v=" + digest
            page = re.sub(re.escape(asset) + r'(?:\?v=[a-f0-9]+)?(?=["\'])', versioned, page)
            worker = worker.replace("'" + asset + "'", "'" + versioned + "'")
        icon_version = hashlib.sha256((ASSETS / "icon.svg").read_bytes()).hexdigest()[:12]
        for asset in ("assets/icon.svg", "assets/favicon.ico", "assets/apple-touch-icon.png"):
            worker = worker.replace("'" + asset + "'", "'" + asset + "?v=" + icon_version + "'")
        page = re.sub(r"<!-- STUDIO_META -->.*?<!-- /STUDIO_META -->", "", page, flags=re.S)
        metadata = head("Play AURA-1, the generative ambient instrument for the M-VAVE FM-1, in your browser. Shape the garden, play Solo and record your session.", "emu/")
        tags = re.findall(r'<link rel="canonical"[^>]*>|<meta property="og:[^>]*>|<meta name="twitter:[^>]*>|<script type="application/ld\+json">.*?</script>', metadata, re.S)
        page = page.replace("</head>", "<!-- STUDIO_META -->\n" + "\n".join(tags) + "\n<!-- /STUDIO_META -->\n</head>")
        index.write_text(page, encoding="utf-8")
    (destination / "sw.js").write_text(worker, encoding="utf-8")


def restyle_installer(page, version):
    page = re.sub(r"<style>.*?</style>", "", page, flags=re.S)
    page = page.replace('<html lang="en">', '<html lang="en" data-theme="light">')
    page = page.replace("</head>", head("Install AURA-1 on your M-VAVE FM-1 with USB MIDI. Connect, confirm your device and install from Chrome or Edge.", "install/", "../assets/") + "</head>")
    page = page.replace("<body>", '<body class="studio-site installer-page"><a class="skip" href="#content">Skip to content</a>' + header("install", "../"))
    page = page.replace("<main>", '<main id="content" class="installer-main" tabindex="-1">')
    page = page.replace('<h1>Install AURA-1</h1>', '<div class="page-heading"><p class="section-kicker">Make it yours</p><h1>Install AURA-1.</h1><p class="page-intro">The same instrument, a different way to play. Install AURA-1 directly over USB.</p></div><ol class="install-steps" aria-label="Installation steps"><li><span>1</span>Connect your FM-1</li><li><span>2</span>Select the device</li><li><span>3</span>Install AURA-1</li></ol><div class="install-workspace"><section class="install-console" aria-label="Firmware installer"><h2 class="install-console-title">Ready when you are.</h2>')
    # Optional bootloader remains an explicit choice, with its complete explanation.
    page = page.replace('<div id="mbootopt" hidden>', '<details class="advanced-options"><summary>Optional bootloader</summary><div id="mbootopt" hidden>')
    page = page.replace('  <p><button type="button" id="go"', '  </details><p><button type="button" id="go"')
    sidebar = (f'</section><aside class="install-device"><span class="device-status" id="midi-support">Checking browser support</span>'
               '<img src="../assets/instrument.svg" width="1000" height="640" alt="The FM-1 running AURA-1">'
               f'<dl><div><dt>Instrument</dt><dd>M-VAVE FM-1</dd></div><div><dt>Firmware</dt><dd>AURA-1 {html.escape(version)}</dd></div>'
               '<div><dt>Connection</dt><dd>USB MIDI</dd></div></dl><a href="../manual/#s1">Installation and recovery guide</a></aside></div>')
    page = page.replace('<h2 data-t="stockh">', sidebar + '<details class="installer-disclosure"><summary>Restore the original firmware</summary><h2 data-t="stockh">', 1)
    page = page.replace('<h2 data-t="reporth">', '</details><details class="installer-disclosure"><summary>Read a crash report</summary><h2 data-t="reporth">', 1)
    page = page.replace("</main>", "</details></main>", 1)
    for key, label in (("confirmgo", "Confirm installation"), ("confirmcancel", "Cancel installation")):
        page = re.sub(fr'(<button\b[^>]*id="{key}")', fr'\1 aria-label="{label}"', page)
    page = re.sub(r"<footer>.*?</footer>", footer("../"), page, count=1, flags=re.S)
    page = re.sub(r'(<button\b[^>]*data-t="confirmcancel"[^>]*>)(\s*)(</button>)', r'\1Cancel\3', page)
    page = re.sub(r'(<button\b[^>]*data-t="appcancel"[^>]*>)(\s*)(</button>)', r'\1Cancel\3', page)
    page = re.sub(r'(<button\b[^>]*data-t="appgo"[^>]*>)(\s*)(</button>)', r'\1Continue\3', page)
    return page


if __name__ == "__main__":
    prepare_emulator()
