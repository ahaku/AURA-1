"""Public project locations shared by site generators."""
import os
from pathlib import Path
import tomllib

ROOT = Path(__file__).resolve().parents[1]
CONFIG = tomllib.loads((ROOT / "app/fm1.toml").read_text(encoding="utf-8"))
REPO = os.environ.get("FM1_REPO", CONFIG["repo"]).rstrip("/")
SITE = os.environ.get("FM1_SITE_URL", CONFIG["site"]).rstrip("/") + "/"
