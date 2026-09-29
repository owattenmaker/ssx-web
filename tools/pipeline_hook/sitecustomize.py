"""State-pack hook for the ISO pipeline (tools/setup_from_iso.py, tools/statepack.py, docs/iso-pipeline.md).

Loaded by every Python step the pipeline runs (PYTHONPATH=tools/pipeline_hook); inert unless SSX3_HOOK_MODE is set.

trace    (maintainers, with the real PS2 savestates and captures): records which files each step reads and writes, and
         which bytes of the savestates' EE memory and of the capture streams it actually looks at (the "footprint"),
         so tools/statepack.py can ship only those bytes. Refuses writes into SSX3_HOOK_GUARD (the live tree).
restore  (users): the savestates and captures were rebuilt from the state pack; reading one returns data that hashes
         to the original's recorded digests, so the exporters' provenance fields stay byte-identical.

The exporters run unchanged: the hook works at the open() / ZipFile.read / struct / re / hashlib level.
SPDX-License-Identifier: GPL-3.0
"""
import os
import sys

MODE = os.environ.get('SSX3_HOOK_MODE')


def _chain_next_sitecustomize():
    # Keep the interpreter's own sitecustomize (e.g. Homebrew's), which ours shadows on PYTHONPATH.
    here = os.path.dirname(os.path.abspath(__file__))
    for entry in sys.path:
        if not entry or os.path.abspath(entry) == here:
            continue
        candidate = os.path.join(entry, 'sitecustomize.py')
        if os.path.isfile(candidate):
            with open(candidate, 'rb') as f:
                code = compile(f.read(), candidate, 'exec')
            exec(code, {'__name__': 'sitecustomize', '__file__': candidate})
            return


if MODE in ('trace', 'restore'):
    import atexit
    import builtins
    import hashlib
    import io
    import json
    import re
    import struct
    import zipfile

    ROOT = os.path.realpath(os.environ['SSX3_HOOK_ROOT'])
    LOGDIR = os.environ.get('SSX3_HOOK_DIR', os.path.join(ROOT, 'local/pipeline/trace'))
    STEP = os.environ.get('SSX3_HOOK_STEP', 'unknown')
    STATE_ROOTS = [r.strip('/') for r in os.environ.get('SSX3_HOOK_STATE_ROOTS', 'local/reference:local/ps2-capture').split(':') if r]
    GUARD = os.path.realpath(os.environ['SSX3_HOOK_GUARD']) if os.environ.get('SSX3_HOOK_GUARD') else None
    _open = io.open
    _real_open = builtins.open

    def _rel(path):
        """Path relative to ROOT (symlinks resolved only up to the checkout: state trees may be links to elsewhere)."""
        try:
            p = os.fspath(path)
        except TypeError:
            return None
        if isinstance(p, bytes):
            p = os.fsdecode(p)
        p = os.path.abspath(p)
        root_abs = os.path.abspath(os.environ['SSX3_HOOK_ROOT'])
        for base in (root_abs, ROOT):
            if p == base or p.startswith(base + os.sep):
                return os.path.relpath(p, base)
        real = os.path.realpath(p)
        if real.startswith(ROOT + os.sep):
            return os.path.relpath(real, ROOT)
        return None

    def _is_state(rel):
        return rel is not None and any(rel == r or rel.startswith(r + '/') for r in STATE_ROOTS)

    # ------------------------------------------------------------------------------------------------ trace
    if MODE == 'trace':
        LOG = dict(step=STEP, argv=sys.argv, pid=os.getpid(), cwd=os.getcwd(), reads={}, writes=[], probes=[], listings={},
                   footprints={}, whole={}, escapes=[])
        _spans = {}          # key -> list of [a, b)

        def _mark(key, a, b):
            if b <= a:
                return
            lst = _spans.setdefault(key, [])
            lst.append((a, b))
            if len(lst) > 200000:
                _spans[key] = _merge(lst)

        def _merge(lst):
            lst.sort()
            out = []
            for a, b in lst:
                if out and a <= out[-1][1]:
                    if b > out[-1][1]:
                        out[-1] = (out[-1][0], b)
                else:
                    out.append((a, b))
            return out

        def _whole(key, why):
            LOG['whole'].setdefault(key, why)

        def _escape(key, what):
            if len(LOG['escapes']) < 200:
                import traceback
                LOG['escapes'].append(dict(key=key, what=what, where=traceback.format_stack(limit=4)[:-1]))

        class Traced(bytes):
            """bytes that remember which of their bytes were looked at (key = 'path' or 'path::member')."""

            def __getitem__(self, k):
                if type(k) is slice:
                    a, b, step = k.indices(len(self))
                    if step == 1:
                        _mark(self._k, a, b)
                    elif b > a:
                        _mark(self._k, a, b)
                else:
                    i = k + len(self) if k < 0 else k
                    _mark(self._k, i, i + 1)
                return bytes.__getitem__(self, k)

            def _found(self, sub, at, n=None):
                if at >= 0:
                    _mark(self._k, at, at + (len(sub) if n is None else n))
                return at

            def find(self, sub, *a):
                r = bytes.find(self, sub, *a)
                return self._found(sub if isinstance(sub, (bytes, bytearray)) else b'?', r)

            def rfind(self, sub, *a):
                r = bytes.rfind(self, sub, *a)
                return self._found(sub if isinstance(sub, (bytes, bytearray)) else b'?', r)

            def index(self, sub, *a):
                r = bytes.index(self, sub, *a)
                return self._found(sub if isinstance(sub, (bytes, bytearray)) else b'?', r)

            def rindex(self, sub, *a):
                r = bytes.rindex(self, sub, *a)
                return self._found(sub if isinstance(sub, (bytes, bytearray)) else b'?', r)

            def startswith(self, prefix, start=0, *a):
                r = bytes.startswith(self, prefix, start, *a)
                n = max((len(p) for p in prefix), default=0) if isinstance(prefix, tuple) else len(prefix)
                _mark(self._k, start, start + n)
                return r

            def endswith(self, suffix, *a):
                _whole(self._k, 'endswith'); return bytes.endswith(self, suffix, *a)

            def count(self, sub, *a):
                at = bytes.find(self, sub, *a)
                while at >= 0:
                    _mark(self._k, at, at + max(1, len(sub) if isinstance(sub, (bytes, bytearray)) else 1))
                    at = bytes.find(self, sub, at + 1)
                return bytes.count(self, sub, *a)

            for _name in ('split', 'rsplit', 'decode', 'hex', 'strip', 'lstrip', 'rstrip', 'splitlines', 'partition',
                          'rpartition', 'replace', 'upper', 'lower', 'translate', '__iter__', '__contains__', '__eq__', '__ne__',
                          '__hash__', '__add__', '__mul__', '__reversed__', '__lt__', '__gt__', '__le__', '__ge__'):
                def _make(name):
                    base = getattr(bytes, name)

                    def method(self, *a, **kw):
                        _whole(self._k, name)
                        return base(self, *a, **kw)
                    method.__name__ = name
                    return method
                locals()[_name] = _make(_name)
            del _name, _make

        def _traced(data, key):
            t = Traced(data)
            t._k = key
            LOG['footprints'].setdefault(key, None)
            return t

        # struct: the exporters' main way of reading memory
        _uf, _u, _iu = struct.unpack_from, struct.unpack, struct.iter_unpack

        def unpack_from(fmt, buffer, offset=0):
            if type(buffer) is Traced:
                n = struct.calcsize(fmt)
                _mark(buffer._k, offset if offset >= 0 else offset + len(buffer), (offset if offset >= 0 else offset + len(buffer)) + n)
            return _uf(fmt, buffer, offset)

        def unpack(fmt, buffer):
            if type(buffer) is Traced:
                _mark(buffer._k, 0, len(buffer))
            return _u(fmt, buffer)

        def iter_unpack(fmt, buffer):
            if type(buffer) is Traced:
                _mark(buffer._k, 0, len(buffer))
            return _iu(fmt, buffer)

        struct.unpack_from, struct.unpack, struct.iter_unpack = unpack_from, unpack, iter_unpack
        _Struct = struct.Struct

        class Struct(_Struct):
            def unpack_from(self, buffer, offset=0):
                if type(buffer) is Traced:
                    o = offset if offset >= 0 else offset + len(buffer)
                    _mark(buffer._k, o, o + self.size)
                return _Struct.unpack_from(self, buffer, offset)

            def unpack(self, buffer):
                if type(buffer) is Traced:
                    _mark(buffer._k, 0, len(buffer))
                return _Struct.unpack(self, buffer)

            def iter_unpack(self, buffer):
                if type(buffer) is Traced:
                    _mark(buffer._k, 0, len(buffer))
                return _Struct.iter_unpack(self, buffer)
        struct.Struct = Struct

        # re on memory images: only the matched spans determine the result
        _re_search, _re_match, _re_finditer, _re_findall, _re_fullmatch = re.search, re.match, re.finditer, re.findall, re.fullmatch

        def _track_match(m, s):
            if m is not None and type(s) is Traced:
                _mark(s._k, m.start(), m.end())
            return m

        re.search = lambda p, s, flags=0: _track_match(_re_search(p, s, flags), s)
        re.match = lambda p, s, flags=0: _track_match(_re_match(p, s, flags), s)
        re.fullmatch = lambda p, s, flags=0: _track_match(_re_fullmatch(p, s, flags), s)

        def finditer(p, s, flags=0):
            for m in _re_finditer(p, s, flags):
                yield _track_match(m, s)
        re.finditer = finditer

        def findall(p, s, flags=0):
            if type(s) is Traced:
                for m in _re_finditer(p, s, flags):
                    _mark(s._k, m.start(), m.end())
            return _re_findall(p, s, flags)
        re.findall = findall

        _mv = builtins.memoryview

        class _MemoryviewMeta(type):
            def __instancecheck__(cls, obj):
                return isinstance(obj, _mv)

        class memoryview(metaclass=_MemoryviewMeta):
            def __new__(cls, obj):
                if type(obj) is Traced:
                    _whole(obj._k, 'memoryview')
                return _mv(obj)
        builtins.memoryview = memoryview

        def _log_open(rel, mode):
            if rel is None:
                return
            if any(c in mode for c in 'wax+'):
                if rel not in LOG['writes']:
                    LOG['writes'].append(rel)
            else:
                LOG['reads'][rel] = LOG['reads'].get(rel, 0) + 1

        class _TracedFile:
            """A read-only binary file of a state root: full reads come back Traced, partial reads mark their range."""

            def __init__(self, f, key):
                self._f, self._key = f, key

            def read(self, n=-1):
                at = self._f.tell()
                data = self._f.read(n)
                if at == 0 and (n is None or n < 0):
                    return _traced(data, self._key)
                LOG['footprints'].setdefault(self._key, None)
                _mark(self._key, at, at + len(data))
                return data

            def readinto(self, b):
                at = self._f.tell(); n = self._f.readinto(b)
                LOG['footprints'].setdefault(self._key, None)
                _mark(self._key, at, at + (n or 0)); return n

            def __iter__(self):
                _whole(self._key, 'iterate'); return iter(self._f)

            def __enter__(self):
                return self

            def __exit__(self, *a):
                self._f.close()

            def __getattr__(self, name):
                return getattr(self._f, name)

        def _open_hook(file, mode='r', *args, **kwargs):
            rel = _rel(file) if not isinstance(file, int) else None
            if rel is not None and any(c in mode for c in 'wax+'):
                real = os.path.realpath(os.fspath(file))
                if GUARD and (real == GUARD or real.startswith(GUARD + os.sep)):
                    raise PermissionError(f'pipeline trace: refusing to write into the guarded tree: {real}')
            _log_open(rel, mode)
            f = _open(file, mode, *args, **kwargs)
            if (rel is not None and 'b' in mode and 'r' in mode and not any(c in mode for c in 'wax+') and _is_state(rel)
                    and not rel.endswith(('.p2s', '.zip', '.json'))):
                return _TracedFile(f, rel)
            return f

        io.open = _open_hook
        builtins.open = _open_hook

        _zread, _zopen = zipfile.ZipFile.read, zipfile.ZipFile.open

        def _zkey(z, name):
            rel = _rel(z.filename) if isinstance(z.filename, (str, bytes, os.PathLike)) else None
            if isinstance(name, zipfile.ZipInfo):
                name = name.filename
            return (rel + '::' + name) if _is_state(rel) else None

        def zread(self, name, pwd=None):
            data = _zread(self, name, pwd)
            key = _zkey(self, name)
            return _traced(data, key) if key else data
        zipfile.ZipFile.read = zread

        def zopen(self, name, mode='r', pwd=None, **kw):
            f = _zopen(self, name, mode, pwd, **kw)
            key = _zkey(self, name) if mode == 'r' else None
            return _TracedFile(f, key) if key else f
        zipfile.ZipFile.open = zopen

        # existence probes and listings of state roots (a restored tree must answer them the same way)
        _exists, _isfile, _isdir, _stat, _listdir, _scandir = (os.path.exists, os.path.isfile, os.path.isdir, os.stat,
                                                                 os.listdir, os.scandir)

        def _probe(path):
            rel = _rel(path) if not isinstance(path, int) else None
            if _is_state(rel) and rel not in LOG['probes'] and len(LOG['probes']) < 100000:
                LOG['probes'].append(rel)

        def stat(path, *a, **kw):
            _probe(path); return _stat(path, *a, **kw)
        os.stat = stat

        def exists(path):
            _probe(path); return _exists(path)
        os.path.exists = exists

        def listdir(path='.'):
            names = _listdir(path)
            rel = _rel(path) if not isinstance(path, int) else None
            if _is_state(rel):
                LOG['listings'].setdefault(rel, sorted(names))
            return names
        os.listdir = listdir

        def scandir(path='.'):
            rel = _rel(path) if not isinstance(path, int) else None
            it = _scandir(path)
            if not _is_state(rel):
                return it
            entries = list(it)
            it.close()
            LOG['listings'].setdefault(rel, sorted(e.name for e in entries))

            class _It:
                def __iter__(self): return iter(entries)
                def __next__(self): raise StopIteration
                def __enter__(self): return self
                def __exit__(self, *a): pass
                def close(self): pass
            return _It()
        os.scandir = scandir

        def _dump():
            for key, lst in _spans.items():
                LOG['footprints'][key] = [list(r) for r in _merge(lst)]
            if not (LOG['reads'] or LOG['writes'] or LOG['footprints']):
                return
            os.makedirs(os.path.join(LOGDIR, STEP), exist_ok=True)
            path = os.path.join(LOGDIR, STEP, f'{os.getpid()}-{id(LOG):x}.json')
            with _real_open.__self__.open(path, 'w') if False else _open(path, 'w') as f:
                json.dump(LOG, f)
        atexit.register(_dump)

    # ------------------------------------------------------------------------------------------------ restore
    else:
        REGISTRY = os.path.join(ROOT, 'local/statepack/restored.json')
        try:
            with _open(REGISTRY) as f:
                RESTORED = json.load(f)['files']
        except FileNotFoundError:
            RESTORED = {}

        class Restored(bytes):
            """Bytes rebuilt from the state pack; hashlib returns the original's digests for them."""

        def _restored(data, digests):
            r = Restored(data)
            r._d = digests
            return r

        class _RestoredFile:
            def __init__(self, f, digests):
                self._f, self._d = f, digests

            def read(self, n=-1):
                at = self._f.tell()
                data = self._f.read(n)
                if at == 0 and (n is None or n < 0):
                    return _restored(data, self._d)
                return data

            def __enter__(self):
                return self

            def __exit__(self, *a):
                self._f.close()

            def __getattr__(self, name):
                return getattr(self._f, name)

        def _open_hook(file, mode='r', *args, **kwargs):
            f = _open(file, mode, *args, **kwargs)
            if 'b' in mode and 'r' in mode and not any(c in mode for c in 'wax+') and not isinstance(file, int):
                entry = RESTORED.get(_rel(file) or '')
                if entry:
                    return _RestoredFile(f, entry['digests'])
            return f
        io.open = _open_hook
        builtins.open = _open_hook

        _zread = zipfile.ZipFile.read

        def zread(self, name, pwd=None):
            data = _zread(self, name, pwd)
            entry = RESTORED.get(_rel(self.filename) or '') if isinstance(self.filename, (str, os.PathLike)) else None
            member = name.filename if isinstance(name, zipfile.ZipInfo) else name
            if entry and member in entry.get('members', {}):
                return _restored(data, entry['members'][member]['digests'])
            return data
        zipfile.ZipFile.read = zread

        class _Fixed:
            def __init__(self, name, hexdigest):
                self.name, self._hex = name, hexdigest
                self.digest_size = len(hexdigest) // 2

            def hexdigest(self):
                return self._hex

            def digest(self):
                return bytes.fromhex(self._hex)

            def copy(self):
                return self

            def update(self, data):
                raise RuntimeError('state pack: a restored buffer was hashed together with other data')

        def _wrap(name, ctor):
            def make(data=b'', *a, **kw):
                if type(data) is Restored:
                    if name not in data._d:
                        raise RuntimeError(f'state pack: no recorded {name} digest for a restored buffer')
                    return _Fixed(name, data._d[name])
                h = ctor(data, *a, **kw) if data != b'' or a or kw else ctor()
                return h
            return make
        for _n in ('sha256', 'sha1', 'md5'):
            setattr(hashlib, _n, _wrap(_n, getattr(hashlib, _n)))
        _new = hashlib.new

        def new(name, data=b'', **kw):
            if type(data) is Restored:
                return getattr(hashlib, name.lower())(data)
            return _new(name, data, **kw)
        hashlib.new = new

_chain_next_sitecustomize()
