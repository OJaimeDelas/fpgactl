#!/usr/bin/env python3
"""Vitis Unified (2024.1) build script - run with: vitis -s build_sw.py

Builds the firmware ELF against an exported XSA:
  1. Platform component (standalone BSP) - created once per XSA content and
     cached: the component name embeds the XSA hash, so switching FUNCTION/
     SCRIPT or editing sources never rebuilds the platform.
  2. App component - sources come from the staging directory (already
     resolved board-first by scripts/stage.py, including lscript.ld,
     run_selection.h and fpga_opts.h). Rebuilt incrementally by CMake.

Configuration comes from environment variables (set by the Makefile), since
'vitis -s' argument passing is unreliable across versions:
  FC_WORKSPACE  workspace directory
  FC_XSA        path to the XSA
  FC_SRC        staging directory with the resolved sources
  FC_ELF_OUT    where to copy the final ELF
  FC_PROC       target processor (psu_cortexa53_0)
  FC_STDIO      BSP stdin/stdout UART (psu_uart_1)
"""

import glob
import hashlib
import os
import shutil
import sys

import vitis  # provided by the vitis -s python environment


def env(name):
    v = os.environ.get(name, "")
    if not v:
        print("build_sw.py: ERROR: missing env var %s" % name)
        sys.exit(1)
    return v


def main():
    ws = env("FC_WORKSPACE")
    xsa = env("FC_XSA")
    src = env("FC_SRC")
    elf_out = env("FC_ELF_OUT")
    proc = env("FC_PROC")
    stdio = env("FC_STDIO")

    with open(xsa, "rb") as fh:
        xsa_hash = hashlib.sha1(fh.read()).hexdigest()[:8]

    plat_name = "plat_" + xsa_hash
    domain_name = "standalone_" + proc
    app_name = "app"

    os.makedirs(ws, exist_ok=True)
    client = vitis.create_client(workspace=ws)

    # ---- platform component (cached per XSA content) ----
    xpfm = os.path.join(ws, plat_name, "export", plat_name, plat_name + ".xpfm")
    if not os.path.exists(xpfm):
        if os.path.isdir(os.path.join(ws, plat_name)):
            # stale/partial platform: remove and recreate
            try:
                client.delete_component(name=plat_name)
            except Exception:
                shutil.rmtree(os.path.join(ws, plat_name), ignore_errors=True)

        print("build_sw.py: creating platform %s from %s" % (plat_name, xsa))
        platform = client.create_platform_component(
            name=plat_name,
            hw_design=xsa,
            os="standalone",
            cpu=proc,
        )

        try:
            domain = platform.get_domain(name=domain_name)
            domain.set_config(option="os", param="standalone_stdin", value=stdio)
            domain.set_config(option="os", param="standalone_stdout", value=stdio)
        except Exception as exc:
            print("build_sw.py: WARNING: stdio configuration failed: %s" % exc)

        platform.build()
        if not os.path.exists(xpfm):
            print("build_sw.py: ERROR: platform build produced no xpfm: %s" % xpfm)
            sys.exit(1)
    else:
        print("build_sw.py: reusing cached platform %s" % plat_name)

    # ---- app component (rebound if the platform changed) ----
    app_dir = os.path.join(ws, app_name)
    state_file = os.path.join(ws, ".app_platform")
    prev_plat = ""
    if os.path.isfile(state_file):
        with open(state_file) as fh:
            prev_plat = fh.read().strip()

    if prev_plat and prev_plat != plat_name and os.path.isdir(app_dir):
        print("build_sw.py: platform changed (%s -> %s), recreating app"
              % (prev_plat, plat_name))
        try:
            client.delete_component(name=app_name)
        except Exception:
            shutil.rmtree(app_dir, ignore_errors=True)

    if not os.path.isdir(app_dir):
        app = client.create_app_component(
            name=app_name,
            platform=xpfm,
            domain=domain_name,
        )
    else:
        app = client.get_component(name=app_name)

    with open(state_file, "w") as fh:
        fh.write(plat_name)

    # ---- sync staged sources into the app's src dir ----
    app_src = os.path.join(app_dir, "src")
    if not os.path.isdir(app_src):
        print("build_sw.py: ERROR: app src dir not found: %s" % app_src)
        sys.exit(1)

    staged = {f for f in os.listdir(src)
              if f.endswith((".c", ".h", ".ld", ".S"))}

    # remove sources we staged earlier that are no longer in the set
    # (never touch the CMake/yaml scaffolding the template generated)
    for f in os.listdir(app_src):
        if f.endswith((".c", ".h", ".ld", ".S")) and f not in staged:
            os.remove(os.path.join(app_src, f))

    for f in sorted(staged):
        s = os.path.join(src, f)
        d = os.path.join(app_src, f)
        if os.path.isfile(d):
            with open(s, "rb") as fs, open(d, "rb") as fd:
                if fs.read() == fd.read():
                    continue
        shutil.copyfile(s, d)

    app.build()

    # ---- locate and export the ELF ----
    elfs = glob.glob(os.path.join(app_dir, "build", "**", "*.elf"), recursive=True)
    elfs = [e for e in elfs if "CMakeFiles" not in e]
    if not elfs:
        print("build_sw.py: ERROR: no ELF produced under %s/build" % app_dir)
        sys.exit(1)
    elf = max(elfs, key=os.path.getmtime)

    os.makedirs(os.path.dirname(elf_out), exist_ok=True)
    shutil.copyfile(elf, elf_out)
    print("build_sw.py: ELF written to %s" % elf_out)

    vitis.dispose()


main()
