#!/usr/bin/env python3
"""Render the illustrated guide in the shared Quiet studio layout."""
import importlib.util
import re
import sys
from pathlib import Path
from studio_site import ROOT, footer, head, header
from project_config import REPO


def render_manual(portable=False):
    spec = importlib.util.spec_from_file_location("fm1_manual", ROOT / "base/tools/manual_html.py")
    converter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(converter)
    converter.NAME = "AURA-1"
    converter.REPO = REPO
    converter.HOME = False
    converter.EMBED = False
    converter.IMG_BASE = ROOT / "docs"
    md = (ROOT / "docs/MANUAL.md").read_text(encoding="utf-8")
    lines, fenced = [], False
    for line in md.splitlines():
        if line.lstrip().startswith("```"):
            fenced = not fenced
        if not fenced:
            line = re.sub(r"^####+ ", "### ", line)
            line = re.sub(r"^\s*\* ", "- ", line)
            line = re.sub(r"^\s+(- )", r"\1", line)
        lines.append(line)
    body = converter.convert("\n".join(lines))
    body = re.sub(r'<header class="site">.*?</header>\s*', "", body, count=1, flags=re.S)
    body = re.sub(r"<h1>.*?</h1>\s*", "", body, count=1, flags=re.S)
    toc_match = re.search(r'<nav class="contents".*?</nav>', body, re.S)
    toc = toc_match.group(0) if toc_match else ""
    first_section = re.search(r'<h2 id="s1">', body)
    overview = body[:toc_match.start()] if toc_match else ""
    body = body[first_section.start():] if first_section else body
    overview = overview.replace("<hr>", "")
    body = re.sub(r'(<div class="table"[^>]*>)(<table><thead><tr>.*?</tr>)',
                  lambda m: m.group(1).replace('class="table"', 'class="table wide-table"') + m.group(2)
                  if m.group(2).count("<th>") >= 4 else m.group(0), body, flags=re.S)

    def figures(text):
        text = re.sub(r'<img src="(img/[^\"]+\.png)"', r'<img width="240" height="240" src="\1"', text)
        text = re.sub(r'<img src="(img/[^\"]+\.svg)"', r'<img width="1000" height="720" src="\1"', text)
        return re.sub(r'(<figure class="wide">)(<img[^>]+>)',
                      r'\1<button class="image-zoom" type="button" aria-label="Open the instrument controls at full size">\2</button>', text)

    overview, body = figures(overview), figures(body)
    prefix = "assets/" if portable else "../assets/"
    site_header = header("manual", "../")
    site_footer = footer("../")
    if portable:
        site_header = site_header.replace('href="../emu/"', 'href="./"').replace('href="../manual/"', 'href="manual.html"')
        site_footer = site_footer.replace('href="../emu/"', 'href="./"').replace('href="../manual/"', 'href="manual.html"')
    play_url = "./" if portable else "../emu/"
    return ('<!doctype html>\n<html lang="en"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1">'
            '<title>The guide | AURA-1</title>' +
            head("Get to know AURA-1: controls, the generative garden, Solo mode, atmospheric textures, MIDI, saving and recovery.", "manual/", prefix) +
            '</head><body class="studio-site document-page"><a class="skip" href="#content">Skip to the guide</a>' + site_header +
            '<div class="manual-layout"><aside class="manual-sidebar" aria-label="Guide navigation">'
            '<div class="manual-search"><label for="guide-search">Find a control or an idea</label>'
            '<input id="guide-search" type="search" placeholder="Try rain, freeze, MIDI…" autocomplete="off" aria-controls="guide-results">'
            '<p id="guide-search-status" class="search-status" role="status"></p><ul id="guide-results" class="search-results"></ul></div>'
            '<div id="guide-contents"><p class="contents-label">In this guide</p>' + toc + '</div></aside>'
            '<main class="manual-content" id="content" tabindex="-1"><p class="section-kicker">The AURA-1 field guide</p>'
            '<h1>A little guidance.<br>A lot to explore.</h1><p class="page-intro">Start with a few notes. Come back here whenever you want to go a little further.</p>'
            '<section class="manual-quickstart" aria-labelledby="quickstart-title"><h2 id="quickstart-title">Your first minute</h2>'
            '<ol><li>Switch on. Garden begins to play.</li><li>Press a lit key to take its note out of the scale. Press again to bring it back.</li>'
            '<li>Turn PRESETS to discover another piece. Turn MASTER for volume.</li></ol>'
            f'<a class="text-link" href="{play_url}">Try it in your browser</a></section>'
            '<details class="manual-overview"><summary>Meet the instrument and its controls</summary>' + overview + '</details>' +
            body + '</main></div>' + site_footer + '</body></html>')


def main():
    target = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "app/web/emu/manual.html")
    target.parent.mkdir(parents=True, exist_ok=True)
    page = render_manual(portable=target.name == "manual.html")
    target.write_text(page, encoding="utf-8")
    print(f"Rendered studio guide: {target} ({len(page)} characters)")


if __name__ == "__main__":
    main()
