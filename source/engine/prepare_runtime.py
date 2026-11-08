"""Install missing, pinned cursor-preprocessing libraries in the toolbox directory.

The native toolbox never imports this worker during startup. prepare_runtime.ps1
supplies a usable private CPython first; this file uses only its standard library.
Scientific packages are probed in short-lived children, never loaded in this
installer. Published component directories are immutable, so repairing a damaged
installation does not replace DLLs that another preprocessing process has loaded.
"""
from __future__ import annotations

import argparse
import contextlib
import ctypes
import email.parser
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import ssl
import stat
import struct
import subprocess
import sys
import tempfile
import time
import traceback
import urllib.error
import urllib.parse
import urllib.request
import uuid
import zipfile


CHUNK = 64 * 1024
ARCHIVE_LIMIT = 64 * 1024 * 1024
UNPACKED_LIMIT = 192 * 1024 * 1024
NETWORK_TIMEOUT = 15
DOWNLOAD_DEADLINE = 900
EXPECTED_PACKAGES = (
    ("numpy", "2.2.6", "numpy", "2.2.6"),
    ("scipy", "1.15.3", "scipy", "1.15.3"),
    ("opencv-python-headless", "4.11.0.86", "cv2", "4.11.0"),
)


class Cancelled(InterruptedError):
    pass


class DependencyError(RuntimeError):
    pass


def linked(path: Path) -> bool:
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0) & 0x400
    )


def directory(path: Path) -> Path:
    if linked(path):
        raise DependencyError("依赖目录不能是链接或重解析点：" + str(path))
    path.mkdir(exist_ok=True)
    if not path.is_dir() or linked(path):
        raise DependencyError("无法使用依赖目录：" + str(path))
    return path


def atomic_bytes(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".tmp-" + uuid.uuid4().hex)
    try:
        with temporary.open("xb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        # Readers such as GetPrivateProfileStringW can briefly hold the old file.
        for attempt in range(6):
            try:
                os.replace(temporary, path)
                break
            except PermissionError:
                if attempt == 5:
                    raise
                time.sleep(0.05)
    finally:
        temporary.unlink(missing_ok=True)


def atomic_json(path: Path, value: dict) -> None:
    atomic_bytes(path, (json.dumps(value, ensure_ascii=False, sort_keys=True) + "\n").encode("utf-8"))


class Progress:
    def __init__(self, status: Path, cancel: Path):
        self.status = status
        self.cancel = cancel
        self.last_write = 0.0

    def checkpoint(self) -> None:
        if self.cancel.exists():
            raise Cancelled("已取消依赖准备")

    def write(self, state: str, message: str, done: int = 0, total: int = 0,
              *, error_kind: str = "", throttle: bool = False) -> None:
        now = time.monotonic()
        if throttle and now - self.last_write < 0.25:
            return
        self.last_write = now
        message = message.replace("\r", " ").replace("\n", " ")[:800]
        text = (f"[Progress]\nDone={done}\nTotal={total}\nState={state}\n"
                f"Message={message}\nErrorKind={error_kind}\n")
        atomic_bytes(self.status, text.encode("utf-16"))


def read_small_json(path: Path, maximum: int = 65536) -> dict:
    if linked(path) or path.stat().st_size > maximum:
        raise DependencyError("依赖记录无效：" + str(path))
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise DependencyError("依赖记录必须是对象")
    return value


def validate_record(row: dict, host: str) -> None:
    url = urllib.parse.urlsplit(row["url"])
    if (url.scheme != "https" or url.hostname != host or url.username or
            url.password or url.port not in (None, 443) or url.query or url.fragment or
            url.path.rsplit("/", 1)[-1] != row["filename"] or
            ".." in url.path.split("/")):
        raise DependencyError("依赖下载地址必须是清单中的官方 HTTPS 地址")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", row["filename"]):
        raise DependencyError("依赖文件名无效")
    if not re.fullmatch(r"[0-9a-f]{64}", row["sha256"]):
        raise DependencyError("依赖 SHA256 无效")
    if type(row["bytes"]) is not int or not 0 < row["bytes"] <= ARCHIVE_LIMIT:
        raise DependencyError("依赖压缩包大小无效")
    if type(row["unpacked_bytes"]) is not int or not 0 < row["unpacked_bytes"] <= UNPACKED_LIMIT:
        raise DependencyError("依赖解压大小无效")


def load_manifest(path: Path) -> dict:
    manifest = read_small_json(path)
    if (manifest.get("schema_version") != 1 or
            manifest.get("platform") != "windows-x86_64" or
            manifest.get("purpose") != "cursor-preprocessing-only"):
        raise DependencyError("不支持的依赖清单")
    python = manifest["python"]
    if python["version"] != "3.12.10" or python["abi"] != "cp312":
        raise DependencyError("不支持的 Python 版本")
    validate_record(python, "www.python.org")
    packages = manifest["packages"]
    if len(packages) != len(EXPECTED_PACKAGES):
        raise DependencyError("依赖集合不完整")
    for row, expected in zip(packages, EXPECTED_PACKAGES):
        if tuple(row[key] for key in ("id", "version", "module", "module_version")) != expected:
            raise DependencyError("依赖版本或顺序不匹配")
        validate_record(row, "files.pythonhosted.org")
        if type(row["members"]) is not int or not 0 < row["members"] < 5000:
            raise DependencyError("依赖成员数无效")
        roots = row["top_level"]
        if not isinstance(roots, list) or not roots or len(set(roots)) != len(roots):
            raise DependencyError("依赖目录清单无效")
        for root in roots:
            if len(safe_member(root)) != 1:
                raise DependencyError("依赖根目录无效")
    return manifest


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, new_url):
        raise DependencyError("依赖下载地址发生重定向，未使用未固定的来源")


def official_open(row: dict):
    validate_record(row, "files.pythonhosted.org")
    opener = urllib.request.build_opener(
        NoRedirect(), urllib.request.HTTPSHandler(context=ssl.create_default_context())
    )
    return opener.open(urllib.request.Request(
        row["url"], headers={"User-Agent": "RA2Toolbox-DependencySetup/1", "Accept-Encoding": "identity"}
    ), timeout=NETWORK_TIMEOUT)


def verified_archive_bytes(path: Path, row: dict, progress: Progress) -> bytes:
    progress.checkpoint()
    if linked(path):
        raise DependencyError("下载缓存不能是链接")
    with path.open("rb") as stream:
        if os.fstat(stream.fileno()).st_size != row["bytes"]:
            raise DependencyError("下载文件大小校验失败")
        data = bytearray()
        digest = hashlib.sha256()
        while True:
            progress.checkpoint()
            block = stream.read(min(CHUNK, row["bytes"] - len(data) + 1))
            if not block:
                break
            data.extend(block)
            digest.update(block)
            if len(data) > row["bytes"]:
                raise DependencyError("下载文件超过清单大小")
    if len(data) != row["bytes"] or digest.hexdigest() != row["sha256"]:
        raise DependencyError("下载文件 SHA256 校验失败，未安装")
    return bytes(data)


def download_archive(row: dict, cache: Path, progress: Progress, *, open_url=official_open) -> Path:
    validate_record(row, "files.pythonhosted.org")
    target = cache / (row["sha256"] + "-" + row["filename"])
    if target.exists() or linked(target):
        try:
            verified_archive_bytes(target, row, progress)
            return target
        except Cancelled:
            raise
        except (DependencyError, OSError):
            if linked(target):
                raise DependencyError("下载缓存含链接，未覆盖")
            target.unlink(missing_ok=True)
    last_error = None
    for attempt in range(2):
        progress.checkpoint()
        part = target.with_name(target.name + ".part-" + uuid.uuid4().hex)
        try:
            progress.write("downloading", f"正在下载 {row['id']}（{row['bytes'] / 1048576:.1f} MiB）", 0, row["bytes"])
            deadline = time.monotonic() + DOWNLOAD_DEADLINE
            digest = hashlib.sha256()
            received = 0
            with contextlib.closing(open_url(row)) as response, part.open("xb") as stream:
                if response.geturl() != row["url"] or response.status != 200:
                    raise DependencyError("依赖下载响应不是固定来源的完整文件")
                length = response.headers.get("Content-Length")
                if length is not None and int(length) != row["bytes"]:
                    raise DependencyError("服务器返回的文件大小与清单不符")
                while True:
                    progress.checkpoint()
                    if time.monotonic() > deadline:
                        raise TimeoutError("依赖下载超时")
                    block = response.read(min(CHUNK, row["bytes"] - received + 1))
                    if not block:
                        break
                    received += len(block)
                    if received > row["bytes"]:
                        raise DependencyError("下载文件超过清单大小")
                    stream.write(block)
                    digest.update(block)
                    progress.write("downloading", f"正在下载 {row['id']} · {received / 1048576:.1f} / {row['bytes'] / 1048576:.1f} MiB",
                                   received, row["bytes"], throttle=True)
                if received != row["bytes"] or digest.hexdigest() != row["sha256"]:
                    raise DependencyError("下载文件不完整或 SHA256 不匹配，未安装")
                stream.flush()
                os.fsync(stream.fileno())
            progress.checkpoint()
            os.replace(part, target)
            return target
        except Cancelled:
            raise
        except (OSError, ValueError, DependencyError) as error:
            last_error = error
            if attempt == 0:
                progress.write("downloading", f"{row['id']} 下载中断，正在重试…")
                for _ in range(5):
                    progress.checkpoint()
                    time.sleep(0.1)
        finally:
            part.unlink(missing_ok=True)
    raise DependencyError(f"下载 {row['id']} 失败：{last_error}。检查网络后再次预处理即可重试。")


WINDOWS_DEVICES = {"CON", "PRN", "AUX", "NUL"} | {
    prefix + digit for prefix in ("COM", "LPT") for digit in "123456789¹²³"
}


def safe_member(name: str) -> tuple[str, ...]:
    if not name or "\\" in name or name.startswith("/") or "\x00" in name:
        raise DependencyError("压缩包包含不安全路径")
    value = name[:-1] if name.endswith("/") else name
    parts = tuple(value.split("/"))
    for part in parts:
        if (part in ("", ".", "..") or part[-1:] in (" ", ".") or
                any(ord(c) < 32 or c in '<>:"|?*' for c in part) or
                part.split(".", 1)[0].upper() in WINDOWS_DEVICES):
            raise DependencyError("压缩包包含 Windows 不安全路径")
    return parts


def wheel_entries(archive: zipfile.ZipFile, row: dict):
    entries = archive.infolist()
    if len(entries) != row["members"] or sum(item.file_size for item in entries) != row["unpacked_bytes"]:
        raise DependencyError("依赖压缩包成员数或解压大小不符")
    roots = set(row["top_level"])
    seen, files, directories = set(), set(), set()
    for item in entries:
        parts = safe_member(item.filename)
        if parts[0] not in roots:
            raise DependencyError("依赖压缩包包含未声明的目录")
        mode = item.external_attr >> 16
        if (stat.S_ISLNK(mode) or (mode & 0o170000) not in (0, stat.S_IFREG, stat.S_IFDIR) or
                item.external_attr & 0x400 or item.flag_bits & 1):
            raise DependencyError("依赖压缩包包含链接、特殊文件或加密内容")
        key = "/".join(parts).casefold()
        parents = {"/".join(parts[:i]).casefold() for i in range(1, len(parts))}
        if key in seen or parents & files or (item.is_dir() and key in files) or (not item.is_dir() and key in directories):
            raise DependencyError("依赖压缩包路径重复或冲突")
        seen.add(key)
        directories.update(parents)
        (directories if item.is_dir() else files).add(key)
        if not 0 <= item.file_size <= row["unpacked_bytes"]:
            raise DependencyError("依赖成员大小无效")
    dist_info = [name for name in roots if name.endswith(".dist-info")]
    if len(dist_info) != 1:
        raise DependencyError("依赖缺少唯一的版本元数据")
    metadata = email.parser.Parser().parsestr(archive.read(dist_info[0] + "/METADATA").decode("utf-8"))
    normalized = lambda value: re.sub(r"[-_.]+", "-", value).lower()
    if normalized(metadata.get("Name", "")) != normalized(row["id"]) or metadata.get("Version") != row["version"]:
        raise DependencyError("依赖元数据版本不符")
    return entries


def extract_wheel(archive_path: Path, row: dict, stage: Path, progress: Progress) -> dict:
    data = verified_archive_bytes(archive_path, row, progress)
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        entries = wheel_entries(archive, row)
        total = 0
        for item in entries:
            progress.checkpoint()
            parts = safe_member(item.filename)
            parent = stage
            for part in parts[:-1]:
                parent = directory(parent / part)
            target = parent / parts[-1]
            if item.is_dir():
                directory(target)
                continue
            if linked(target):
                raise DependencyError("依赖临时目录包含链接")
            count = 0
            with archive.open(item) as source, target.open("xb") as output:
                while True:
                    progress.checkpoint()
                    block = source.read(CHUNK)
                    if not block:
                        break
                    count += len(block)
                    total += len(block)
                    if count > item.file_size or total > row["unpacked_bytes"]:
                        raise DependencyError("依赖解压大小超过清单")
                    output.write(block)
            if count != item.file_size:
                raise DependencyError("依赖成员被截断")
    if total != row["unpacked_bytes"]:
        raise DependencyError("依赖解压不完整")
    return {"members": len(entries), "unpacked_bytes": total}


class InstallLock:
    def __init__(self, path: Path, progress: Progress, timeout: float = 600,
                 message: str = "正在等待另一个工具箱完成依赖准备…"):
        self.path, self.progress, self.timeout = path, progress, timeout
        self.message = message
        self.handle = None

    def __enter__(self):
        if linked(self.path):
            raise DependencyError("依赖锁文件不能是链接")
        end = time.monotonic() + self.timeout
        while True:
            self.progress.checkpoint()
            if os.name == "nt":
                kernel = ctypes.WinDLL("kernel32", use_last_error=True)
                kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                              ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
                kernel.CreateFileW.restype = ctypes.c_void_p
                handle = kernel.CreateFileW(str(self.path), 0xC0000000, 0, None, 4, 0x80, None)
                if handle != ctypes.c_void_p(-1).value:
                    self.handle = handle
                    self.kernel = kernel
                    return self
                error = ctypes.get_last_error()
                if error not in (32, 33):
                    raise OSError(error, "无法取得依赖安装锁")
            else:
                # Portable tests exercise real lock ownership and process exit.
                import fcntl
                handle = self.path.open("ab")
                try:
                    fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    self.handle = handle
                    return self
                except BlockingIOError:
                    handle.close()
            if time.monotonic() >= end:
                raise DependencyError("另一个工具箱仍在准备依赖，请稍后重试")
            self.progress.write("dependency_wait", self.message, throttle=True)
            time.sleep(0.1)

    def __exit__(self, *args):
        if self.handle is not None:
            if os.name == "nt":
                self.kernel.CloseHandle.argtypes = [ctypes.c_void_p]
                self.kernel.CloseHandle(self.handle)
            else:
                self.handle.close()
            self.handle = None


def search_paths(paths: list[Path], legacy: Path) -> list[Path]:
    # A legacy site-packages contains *all* modules, including broken ones.
    # A repaired component must therefore precede it even if NumPy was reused.
    unique = list(dict.fromkeys(paths))
    return [path for path in unique if path != legacy] + ([legacy] if legacy in unique else [])


PROBE_CODE = r"""
import importlib,json,os,sys
paths,module,version,origin=json.loads(sys.argv[1])
sys.path[:]=paths+[p for p in sys.path if 'site-packages' not in p.replace('\\','/').lower()]
m=importlib.import_module(module)
assert m.__version__==version, (module,m.__version__,version)
assert os.path.commonpath([os.path.realpath(m.__file__),os.path.realpath(origin)])==os.path.realpath(origin), 'wrong module location'
import numpy as np
assert np.arange(4,dtype=np.uint8).sum()==6
if module=='numpy':
 assert np.allclose(np.linalg.solve(np.eye(2),np.ones(2)),np.ones(2))
elif module=='scipy':
 from scipy.spatial import cKDTree
 from scipy.ndimage import distance_transform_edt
 assert cKDTree([[0.,0.],[1.,1.]]).query([0.,0.])[1]==0
 assert distance_transform_edt(np.array([[0,1],[1,1]],dtype=np.uint8))[1,1]>1
elif module=='cv2':
 assert hasattr(m.DISOpticalFlow_create(m.DISOPTICAL_FLOW_PRESET_MEDIUM),'calc')
 image=np.zeros((4,4,3),dtype=np.uint8)
 assert m.resize(m.cvtColor(image,m.COLOR_RGB2GRAY),(8,8)).shape==(8,8)
 assert m.imencode('.png',image)[0]
print('RA2_DEPENDENCY_OK')
"""


def run_probe(python: Path, row: dict, paths: list[Path], origin: Path, progress: Progress) -> bool:
    progress.checkpoint()
    payload = json.dumps([[str(path) for path in paths], row["module"], row["module_version"], str(origin)])
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    # A descendant retaining a pipe's write end must not make a finished probe
    # wait forever for EOF. A temporary file has no pipe-drain dependency.
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen([str(python), "-I", "-S", "-c", PROBE_CODE, payload],
                                   stdin=subprocess.DEVNULL, stdout=output,
                                   stderr=subprocess.STDOUT, creationflags=flags)
        deadline = time.monotonic() + 60
        try:
            while process.poll() is None:
                progress.checkpoint()
                if time.monotonic() >= deadline:
                    return False
                time.sleep(0.1)
            output.seek(0)
            return process.returncode == 0 and b"RA2_DEPENDENCY_OK" in output.read(65536)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def component_from_index(index: dict, row: dict, components: Path) -> Path | None:
    item = index.get("components", {}).get(row["id"], {})
    if not isinstance(item, dict):
        return None
    name = item.get("directory", "")
    pattern = re.escape(row["id"]) + "-" + row["sha256"][:12] + r"-[0-9a-f]{32}"
    if (item.get("sha256") != row["sha256"] or item.get("version") != row["version"] or
            not isinstance(name, str) or re.fullmatch(pattern, name) is None):
        return None
    folder = components / name
    if not folder.is_dir() or linked(folder):
        return None
    try:
        marker = read_small_json(folder / ".component.json", 4096)
    except (OSError, ValueError, DependencyError):
        return None
    if any(marker.get(key) != row[key] for key in ("id", "version", "sha256")):
        return None
    return folder


def prepare_components(root: Path, python: Path, manifest: dict, progress: Progress,
                       *, probe=run_probe, fetch=download_archive) -> list[Path]:
    runtime = directory(root / "runtime")
    managed = directory(runtime / "downloaded")
    components = directory(managed / "packages")
    cache = directory(runtime / "download-cache")
    legacy = runtime / "Lib" / "site-packages"
    index_path = managed / "components.json"
    with InstallLock(managed / "packages.lock", progress):
        try:
            index = read_small_json(index_path)
            if index.get("schema_version") != 1 or not isinstance(index.get("components"), dict):
                index = {"schema_version": 1, "components": {}}
        except (OSError, ValueError, DependencyError):
            index = {"schema_version": 1, "components": {}}
        selected = []
        for row in manifest["packages"]:
            progress.checkpoint()
            progress.write("dependency_check", "正在检查预处理依赖：" + row["id"])
            active = component_from_index(index, row, components)
            candidate = None
            for folder in (active, legacy if legacy.is_dir() and not linked(legacy) else None):
                if folder is not None and probe(python, row, search_paths(selected + [folder], legacy), folder, progress):
                    candidate = folder
                    break
            if candidate is not None:
                selected.append(candidate)
                continue
            archive = fetch(row, cache, progress)
            progress.write("installing", "正在校验并安装 " + row["id"] + "，不会更改系统 Python…")
            stage = components / (".stage-" + uuid.uuid4().hex)
            stage.mkdir()
            final = components / (row["id"] + "-" + row["sha256"][:12] + "-" + uuid.uuid4().hex)
            published = False
            try:
                details = extract_wheel(archive, row, stage, progress)
                progress.checkpoint()
                if not probe(python, row, search_paths(selected + [stage], legacy), stage, progress):
                    raise DependencyError(row["id"] + " 解压后无法加载；已保留下载缓存，可重试。")
                marker = {key: row[key] for key in ("id", "version", "sha256")}
                marker.update(details)
                atomic_json(stage / ".component.json", marker)
                progress.checkpoint()
                os.rename(stage, final)
                updated = {"schema_version": 1, "components": dict(index["components"])}
                updated["components"][row["id"]] = {
                    "directory": final.name, "version": row["version"], "sha256": row["sha256"]
                }
                atomic_json(index_path, updated)
                published = True
                index = updated
                selected.append(final)
            finally:
                if stage.exists():
                    shutil.rmtree(stage)
                if not published and final.exists():
                    shutil.rmtree(final)
        progress.checkpoint()
        return search_paths(selected, legacy)


def run_precompute(root: Path, profile: Path, progress: Progress, paths: list[Path]) -> int:
    progress.checkpoint()
    progress.write("running", "依赖已就绪，正在本机预处理…")
    arguments = [sys.executable, "-I", "-S", str(Path(__file__).resolve()), "--run-precompute",
                 "--profile", str(profile), "--status", str(progress.status), "--cancel", str(progress.cancel),
                 "--paths", json.dumps([str(path) for path in paths])]
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    # Do not buffer native-extension output in a pipe while waiting for the child.
    log_path = progress.status.with_name(progress.status.name + ".runtime.log")
    with log_path.open("wb") as log:
        process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, stdout=log,
                                   stderr=subprocess.STDOUT, creationflags=flags, cwd=root)
        cancellation_time = None
        try:
            while process.poll() is None:
                if progress.cancel.exists():
                    cancellation_time = cancellation_time or time.monotonic()
                    if time.monotonic() - cancellation_time > 8:
                        process.kill()
                        process.wait()
                        progress.write("cancelled", "已取消预处理，已完成的分段仍会保留")
                        return 0
                time.sleep(0.1)
            if process.returncode and not (progress.status.parent / (progress.status.name + ".error.txt")).exists():
                progress.write("error", "预处理引擎退出；详情见 progress.ini.runtime.log")
            return process.returncode
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


def validate_task_paths(profile: Path, status: Path, cancel: Path) -> Path:
    profile = profile.resolve(strict=True)
    task_directory = status.parent.resolve(strict=True)
    if (not profile.is_dir() or status.resolve() != task_directory / "progress.ini" or
            cancel.resolve() != task_directory / "cancel.flag" or
            task_directory.parent != profile / ".preprocess" or
            linked(task_directory) or linked(task_directory.parent)):
        raise DependencyError("预处理路径无效")
    return profile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--status", type=Path, required=True)
    parser.add_argument("--cancel", type=Path, required=True)
    parser.add_argument("--run-precompute", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--paths", help=argparse.SUPPRESS)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    profile = validate_task_paths(args.profile, args.status, args.cancel)
    progress = Progress(args.status, args.cancel)
    try:
        if args.run_precompute:
            import runpy
            paths = [str(Path(path).resolve(strict=True)) for path in json.loads(args.paths)]
            sys.path[:] = [str(root / "engine")] + paths + [
                path for path in sys.path if "site-packages" not in path.replace("\\", "/").lower()
            ]
            sys.argv = [str(root / "engine" / "precompute.py"), "--profile", str(profile),
                        "--status", str(args.status), "--cancel", str(args.cancel)]
            runpy.run_path(sys.argv[0], run_name="__main__")
            return 0
        if sys.platform != "win32" or sys.version_info[:2] != (3, 12) or struct.calcsize("P") != 8:
            raise DependencyError("预处理需要工具箱自带的 64 位 Python 3.12")
        manifest = load_manifest(root / "engine" / "runtime-dependencies.json")
        # The task gets its own cancellation/status files; this profile lock
        # prevents two workers from writing the same generated timelines. It
        # remains owned until the actual precompute child has exited.
        with InstallLock(profile / ".preprocess.lock", progress,
                         message="正在等待同一光标包的上一项预处理结束…"):
            paths = prepare_components(root, Path(sys.executable), manifest, progress)
            return run_precompute(root, profile, progress, paths)
    except Cancelled:
        progress.write("cancelled", "已取消依赖准备；已验证的下载缓存保留，下次可重试")
        return 0
    except Exception as error:
        progress.write("error", str(error), error_kind="dependency")
        Path(str(args.status) + ".error.txt").write_text(traceback.format_exc(), encoding="utf-8")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
