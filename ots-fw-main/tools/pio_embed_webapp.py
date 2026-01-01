Import("env")

# PlatformIO extra_script (pre)
# Regenerates the embedded WiFi config webapp header before each build.

from pathlib import Path
import subprocess

project_dir = Path(env["PROJECT_DIR"]).resolve()
script = project_dir / "tools" / "embed_webapp.py"
webapp_dir = project_dir / "webapp"
out_header = project_dir / "include" / "webapp" / "ots_webapp.h"


def _latest_mtime(paths):
    latest = 0.0
    for p in paths:
        try:
            latest = max(latest, p.stat().st_mtime)
        except FileNotFoundError:
            pass
    return latest


inputs = [webapp_dir / "index.html", webapp_dir / "style.css", webapp_dir / "app.js", script]
input_mtime = _latest_mtime(inputs)
output_mtime = _latest_mtime([out_header])

if output_mtime < input_mtime:
    python_exe = env.subst("$PYTHONEXE") or "python3"
    subprocess.check_call([python_exe, str(script)])
