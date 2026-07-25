#!/usr/bin/env python3
"""Smoke-check .kpn shape used by KeyPresserNext."""
import json
import tempfile
from pathlib import Path

sample = {
    "_comment": "KeyPresserNext settings. Edit carefully. Images live in *.assets/",
    "version": 1,
    "window": {
        "display": "normal.auto",
        "mouse": "windows",
        "keypad": "windows",
        "bindMode": 0,
        "topmost": False,
        "runMode": "independent",
        "hotkeyStart": "F9",
        "hotkeyStop": "F10",
        "geometry": {"x": 100, "y": 80, "w": 960, "h": 720},
    },
    "rows": [
        {
            "enabled": True,
            "kind": "key",
            "hold": False,
            "shortcut": "A",
            "intervalMinMs": 1000,
            "intervalMaxMs": 1500,
            "target": {"mode": "none"},
        },
        {
            "enabled": True,
            "kind": "mouse",
            "button": "left",
            "intervalMinMs": 800,
            "intervalMaxMs": 800,
            "target": {"mode": "coordinate", "x": 10, "y": 20},
        },
    ],
}

td = Path(tempfile.mkdtemp())
kpn = td / "demo.kpn"
kpn.write_text(json.dumps(sample, indent=2), encoding="utf-8")
loaded = json.loads(kpn.read_text(encoding="utf-8"))
assert loaded["window"]["display"] == "normal.auto"
assert len(loaded["rows"]) == 2
print("kpn smoke ok:", kpn)
