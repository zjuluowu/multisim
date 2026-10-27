#!/usr/bin/env python3
# Copyright (C) 2026 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Prepare the pinned dependency cache for the Linux SIM guard (no root required).

Everything is placed under --deps-root; nothing is written into the source tree.
Exit code is non-zero if any pinned input cannot be reproduced exactly.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile

HERE = os.path.dirname(os.path.abspath(__file__))
LOCK = os.path.join(os.path.dirname(HERE), "deps.lock.json")

# Large repositories are checked out sparsely; only interface headers and the listed sources are needed.
SPARSE = {
    "ability_ability_runtime": ["interfaces"],
    "bundlemanager_bundle_framework": ["interfaces"],
    "communication_ipc": ["interfaces", "ipc/native/src/core/framework", "utils"],
    "systemabilitymgr_samgr": ["interfaces", "frameworks/native"],
    "security_access_token": ["interfaces"],
    "security_huks": ["interfaces"],
    "startup_init": ["interfaces"],
    "distributeddatamgr_data_share": ["interfaces", "frameworks/native/common", "frameworks/native/consumer"],
    "distributeddatamgr_preferences": ["interfaces"],
    "telephony_cellular_data": ["interfaces", "frameworks/native", "services/include"],
    "telephony_telephony_data": ["interfaces", "sim"],
    "hiviewdfx_hilog": ["interfaces"],
    "hiviewdfx_hisysevent": ["interfaces"],
    "hiviewdfx_hitrace": ["interfaces"],
}


def run(cmd, cwd=None):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd, check=True)


def head(path):
    try:
        return subprocess.run(["git", "-C", path, "rev-parse", "HEAD"], capture_output=True, text=True,
                              check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def fetch_repo(root, name, url, sha):
    path = os.path.join(root, name)
    if head(path) == sha:
        return
    if os.path.exists(path):
        shutil.rmtree(path)
    os.makedirs(path)
    run(["git", "init", "-q"], cwd=path)
    run(["git", "remote", "add", "origin", url], cwd=path)
    if name in SPARSE:
        run(["git", "sparse-checkout", "set", "--cone"] + SPARSE[name], cwd=path)
    run(["git", "fetch", "-q", "--depth", "1", "--filter=blob:none", "origin", sha], cwd=path)
    run(["git", "checkout", "-q", "FETCH_HEAD"], cwd=path)
    if head(path) != sha:
        raise RuntimeError(f"{name}: checked out {head(path)} != pinned {sha}")


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def extract_archives(root, archives):
    for name, spec in archives.items():
        src = os.path.join(root, spec["from"])
        if sha256(src) != spec["sha256"]:
            raise RuntimeError(f"{name}: sha256 mismatch for {src}")
        dst = os.path.join(root, spec["extract_to"])
        if not os.path.isdir(dst):
            os.makedirs(dst)
            with tarfile.open(src) as tar:
                tar.extractall(dst)


def fetch_debs(root, debs):
    debdir = os.path.join(root, "debs")
    os.makedirs(debdir, exist_ok=True)
    for name, spec in debs.items():
        deb = os.path.join(debdir, f"{name}_{spec['version']}_amd64.deb")
        if not os.path.exists(deb):
            run(["apt-get", "download", f"{name}={spec['version']}"], cwd=debdir)
        if sha256(deb) != spec["sha256"]:
            raise RuntimeError(f"{name}: sha256 mismatch for {deb}")
        run(["dpkg-deb", "-x", deb, os.path.join(root, spec["extract_to"])])


def build_idl_tool(root):
    tool = os.path.join(root, "idl_build", "idl")
    if os.path.exists(tool):
        return tool
    build = os.path.join(root, "idl_build")
    securec = os.path.join(root, "third_party_bounds_checking_function")
    src_dir = os.path.join(root, "ability_idl_tool", "idl_tool_2")
    subprocess.run(["make", "-f", "MakefileLinux", f"BOUNDS_CHECK_DIR={securec}", f"BUILD_DIR={build}",
                    f"TARGET={tool}", "CXX=clang++-18", "-j8"], cwd=src_dir)
    objs = [os.path.join(d, f) for d, _, fs in os.walk(build) for f in fs
            if f.endswith(".o") and "bounds_checking" not in d]
    sec_objs = []
    for c in sorted(os.listdir(os.path.join(securec, "src"))):
        if c.endswith(".c"):
            obj = os.path.join(build, "securec_" + c + ".o")
            run(["clang-18", "-c", "-O1", "-I", os.path.join(securec, "include"),
                 os.path.join(securec, "src", c), "-o", obj])
            sec_objs.append(obj)
    run(["clang++-18"] + objs + sec_objs + ["-o", tool])
    return tool


def generate_idl(root, tool, items):
    for item in items:
        src = os.path.join(root, item["repo"], item["file"])
        out = os.path.join(root, item["out"])
        os.makedirs(out, exist_ok=True)
        run([tool, "-c", os.path.basename(src), "--gen-cpp", "-d", out], cwd=os.path.dirname(src))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--deps-root", required=True)
    parser.add_argument("--production-archive", help="also export the pinned upstream production tree here",
                        default=None)
    parser.add_argument("--git-dir", help="git repository that contains the pinned upstream commit", default=None)
    args = parser.parse_args()
    lock = json.load(open(LOCK))
    root = os.path.abspath(args.deps_root)
    os.makedirs(root, exist_ok=True)
    for name, (url, sha) in lock["repos"].items():
        fetch_repo(root, name, url, sha)
    extract_archives(root, lock["archives"])
    fetch_debs(root, lock["debs"])
    generate_idl(root, build_idl_tool(root), lock["idl"])
    if args.production_archive and args.git_dir:
        dst = os.path.abspath(args.production_archive)
        if not os.path.isdir(dst):
            os.makedirs(dst)
            commit = lock["production"]["upstream_commit"]
            archive = subprocess.run(["git", "-C", args.git_dir, "archive", commit], capture_output=True, check=True)
            subprocess.run(["tar", "-x", "-C", dst], input=archive.stdout, check=True)
    print("deps ready:", root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
