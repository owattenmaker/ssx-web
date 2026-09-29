"""Content/dependency cached compilation for development-only C++ oracles."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib, json, os, shlex, subprocess, time


def cached_oracle_build(command, cache):
    cache = Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    sources = [Path(arg) for arg in command if arg.endswith('.cpp')]
    first = min(command.index(str(path)) for path in sources)
    prefix = command[:first]
    version = subprocess.check_output(prefix[:2] + ['--version'])
    def digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    def compile_one(source):
        key = hashlib.sha256(json.dumps(prefix).encode() + version + str(source).encode() + source.read_bytes()).hexdigest()
        obj, meta = cache / (key + '.o'), cache / (key + '.json')
        if obj.exists() and meta.exists():
            dependencies = json.loads(meta.read_text())
            if all(Path(name).exists() and digest(name) == value for name,value in dependencies.items()):
                return str(obj)
        pending = cache / (key + f'.{os.getpid()}.o')
        depfile = pending.with_suffix('.d')
        for attempt in range(3):
            started = time.time_ns()
            subprocess.run(prefix + ['-c', str(source), '-MMD', '-MF', str(depfile), '-o', str(pending)], check=True)
            raw = depfile.read_text().replace('\\\n', '')
            dependencies = [Path(name) for name in shlex.split(raw.partition(':')[2])]
            if any(path.stat().st_mtime_ns >= started for path in dependencies):
                continue
            meta.write_text(json.dumps({str(path):digest(path) for path in dependencies}, sort_keys=True))
            pending.replace(obj)
            depfile.unlink()
            return str(obj)
        raise RuntimeError(f'Oracle dependencies changed repeatedly while compiling {source}')
    with ThreadPoolExecutor(max_workers=6) as pool:
        objects = list(pool.map(compile_one, sources))
    replacements = dict(zip(map(str,sources), objects))
    subprocess.run([replacements.get(arg,arg) for arg in command], check=True)
