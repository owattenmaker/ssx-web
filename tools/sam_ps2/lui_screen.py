"""Moved to tools/lui_screen.py (the exporters share it); this name stays so the Sam tools here keep importing it."""
import importlib.util as _util
import sys as _sys
from pathlib import Path as _Path

_spec = _util.spec_from_file_location('lui_screen', _Path(__file__).resolve().parents[1] / 'lui_screen.py')
_module = _util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
_sys.modules[__name__] = _module
globals().update({_k: getattr(_module, _k) for _k in dir(_module) if not _k.startswith('__')})  # path-based loaders
