"""Regression checks for the generated manual (python tests/host/manual_test.py)."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_manual_figures
from theme_manual import render_manual


class ManualNavigationTest(unittest.TestCase):
    def test_credits_link_targets_section_twelve(self):
        for portable in (False, True):
            with self.subTest(portable=portable):
                manual = render_manual(portable=portable)
                self.assertIn('<a href="#s12">Credits &amp; License</a>', manual)
                self.assertIn('<h2 id="s12"><span class="num">12</span>Credits &amp; License</h2>', manual)


class ManualFigureTest(unittest.TestCase):
    def test_generator_writes_utf8_svg(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            screen = ROOT / "docs/img/screen-garden.png"
            with patch.object(gen_manual_figures, "OUT", output), patch.object(
                sys, "argv", ["gen_manual_figures.py", str(screen)]
            ), contextlib.redirect_stdout(io.StringIO()):
                gen_manual_figures.main()
            svg = output / "fm1-panel.svg"
            ET.parse(svg)
            self.assertIn("·", svg.read_text(encoding="utf-8"))

    def test_committed_svg_is_valid(self):
        ET.parse(ROOT / "docs/img/fm1-panel.svg")


if __name__ == "__main__":
    unittest.main()
