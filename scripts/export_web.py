#!/usr/bin/env python3
"""Build and serve the LeirEngine WebGPU demos (Emscripten).

Examples:
    python export_web.py                         # engine demo, serve 8001, open browser
    python export_web.py --demo demo             # M1 raw-RHI demo, serve 8000
    python export_web.py --no-serve              # build only
    python export_web.py --emsdk C:/emsdk        # override emsdk location

The Emscripten toolchain is taken from the fixed per-OS default path
(Windows: C:\\programs_dev\\emsdk6), overridable with --emsdk.
"""

import argparse
import glob
import http.server
import os
import shutil
import socket
import subprocess
import sys
import webbrowser

DEMOS = {
    "demo": {
        "dir": os.path.join("examples", "WebDemo"),
        "build_dir": os.path.join("examples", "WebDemo", "build", "emscripten-webdemo"),
        "port": 8000,
        "html": "LeirEngineWebDemo.html",
    },
    "engine": {
        "dir": os.path.join("examples", "WebEngineDemo"),
        "build_dir": os.path.join("examples", "WebEngineDemo", "build", "emscripten-webengine"),
        "port": 8001,
        "html": "WebEngineDemo.html",
    },
}

EMSDK_WIN_DEFAULT = r"C:\programs_dev\emsdk6"
EMSDK_POSIX_DEFAULT = os.path.expanduser("~/emsdk")
VS_CMAKE_DIR = r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
VS_NINJA_DIR = r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"


def log(msg):
    print(msg, flush=True)


def run(cmd, cwd=None, env=None):
    log("> " + subprocess.list2cmdline(cmd))
    return subprocess.run(cmd, cwd=cwd, env=env)


def find_emsdk(override):
    if override:
        return override
    return EMSDK_WIN_DEFAULT if os.name == "nt" else EMSDK_POSIX_DEFAULT


def emsdk_python(emsdk):
    if os.name == "nt":
        hits = sorted(glob.glob(os.path.join(emsdk, "python", "*", "python.exe")), reverse=True)
        return hits[0] if hits else None
    return None


def build_env(emsdk):
    env = dict(os.environ)
    paths = []
    emscripten = os.path.join(emsdk, "upstream", "emscripten")
    if os.path.isdir(emscripten):
        paths.append(emscripten)
    if os.name == "nt":
        if os.path.isdir(VS_CMAKE_DIR):
            paths.append(VS_CMAKE_DIR)
        if os.path.isdir(VS_NINJA_DIR):
            paths.append(VS_NINJA_DIR)
    if paths:
        path_key = next((k for k in env if k.upper() == "PATH"), "PATH")
        env[path_key] = os.pathsep.join(paths + [env.get(path_key, "")])
    py = emsdk_python(emsdk)
    if py:
        env["EMSDK_PYTHON"] = py
    return env


def check_port(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(0.3)
        try:
            s.connect(("127.0.0.1", port))
            return True
        except OSError:
            return False


def serve(build_dir, port):
    os.chdir(build_dir)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), http.server.SimpleHTTPRequestHandler)
    log(f"Serving {build_dir} at http://127.0.0.1:{port}  (Ctrl+C to stop)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log("Server stopped.")


def main():
    parser = argparse.ArgumentParser(description="Build and serve a LeirEngine WebGPU demo (Emscripten).")
    parser.add_argument("--demo", choices=DEMOS.keys(), default="engine")
    parser.add_argument("--serve", dest="serve", action="store_true", default=True)
    parser.add_argument("--no-serve", dest="serve", action="store_false")
    parser.add_argument("--port", type=int, default=None)
    parser.add_argument("--no-open", action="store_true")
    parser.add_argument("--emsdk", default=None)
    args = parser.parse_args()

    demo = DEMOS[args.demo]
    port = args.port or demo["port"]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    demo_dir = os.path.join(root, demo["dir"])
    build_dir = os.path.join(root, demo["build_dir"])
    url = f"http://127.0.0.1:{port}/{demo['html']}"

    emsdk = find_emsdk(args.emsdk)
    if not os.path.isdir(emsdk):
        log(f"ERROR: emsdk not found at {emsdk}")
        log("Pass --emsdk <path> to point at a valid emsdk install.")
        sys.exit(1)
    toolchain = os.path.join(emsdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake")
    if not os.path.isfile(toolchain):
        log(f"ERROR: Emscripten toolchain not found: {toolchain}")
        sys.exit(1)

    env = build_env(emsdk)
    # CreateProcess resolves bare exe names against the PARENT process PATH,
    # not env=, so resolve cmake to an absolute path against the built PATH.
    path_key = next((k for k in env if k.upper() == "PATH"), "PATH")
    cmake_exe = shutil.which("cmake", path=env[path_key]) or "cmake"

    log(f"== [{args.demo}] configure: {demo['dir']} (preset emscripten) ==")
    r = run([cmake_exe, "--preset=emscripten", f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"], cwd=demo_dir, env=env)
    if r.returncode != 0:
        sys.exit(r.returncode)

    log(f"== [{args.demo}] build -> {build_dir} ==")
    r = run([cmake_exe, "--build", build_dir], cwd=demo_dir, env=env)
    if r.returncode != 0:
        sys.exit(r.returncode)

    suffix = demo["html"].replace(".html", "")
    artifacts = [f"{suffix}.html", f"{suffix}.js", f"{suffix}.wasm", f"{suffix}.data"]
    for a in artifacts:
        p = os.path.join(build_dir, a)
        if not os.path.isfile(p):
            log(f"ERROR: missing artifact {p}")
            sys.exit(1)
        log(f"OK  {a}  ({os.path.getsize(p):,} bytes)")

    if not args.serve:
        log(f"Done. Serve {build_dir} and open {url}")
        return

    if check_port(port):
        log(f"Port {port} is already serving - opening {url}")
        if not args.no_open:
            webbrowser.open(url)
        return

    if not args.no_open:
        webbrowser.open(url)
    serve(build_dir, port)


if __name__ == "__main__":
    main()