"""Publication regressions: firmware identity and deployment metadata."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "base/tools"))
from make_installer import package_info
from project_config import CONFIG, REPO, SITE
from studio_site import footer, head, prepare_emulator


class PublicationTest(unittest.TestCase):
    def test_clean_emulator_includes_reader_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            prepare_emulator(output)
            for name in ("words.js", "tts.js"):
                self.assertEqual(
                    (output / "reader" / name).read_bytes(),
                    (ROOT / "app/web/emu/reader" / name).read_bytes(),
                )

    def test_fork_locations_are_consistent(self):
        self.assertIn(REPO, footer())
        self.assertIn(f'href="{SITE}manual/"', head("Guide", "manual/"))
        self.assertTrue(REPO.startswith("https://github.com/"))
        words = (ROOT / "app/words.tsv").read_text(encoding="utf-8")
        self.assertIn(f'# url: {CONFIG["site"]}words/', words)

    def test_wrong_product_rejected_even_with_explicit_version(self):
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "wrong.fwsc"
            package.write_bytes(b"placeholder")
            with patch("make_site.product_of", return_value="FM-1_9999999"):
                with self.assertRaisesRegex(SystemExit, "Invalid AURA-1"):
                    package_info(package, "0.0.3")

    def test_bundled_package_version_cannot_be_mislabelled(self):
        package = ROOT / "firmware/aura-1.fwsc"
        _, _, version = package_info(package)
        package_info(package, version)
        with self.assertRaisesRegex(SystemExit, "does not match"):
            package_info(package, "99.99.99")


if __name__ == "__main__":
    unittest.main()
