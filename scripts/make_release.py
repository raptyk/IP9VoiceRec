# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#
# PlatformIO post-build step shared by all IP9 Cardputer apps (canonical copy: IP9Kit/tools;
# every app keeps an identical copy in scripts/). Produces in ./release:
#   <name>-<version>.bin       app image for M5Launcher (SD card -> Install)
#   <name>-<version>-full.bin  merged image (bootloader + partitions + app),
#                              flash at offset 0x0 with esptool or M5Burner
# <name> comes from `custom_release_name` in platformio.ini (e.g. IP9LGRemote), <version>
# from the APP_VERSION build flag.
import os
import re

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)


def _release_name():
    return env.GetProjectOption("custom_release_name", "IP9App")  # noqa: F821


def _app_version():
    for flag in env.get("BUILD_FLAGS", []):  # noqa: F821
        match = re.search(r'APP_VERSION=\\?"([^"\\]+)', flag)
        if match:
            return match.group(1)
    return "dev"


def _make_release(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    release_dir = os.path.join(project_dir, "release")
    os.makedirs(release_dir, exist_ok=True)

    version = _app_version()
    app_bin = os.path.join(build_dir, "firmware.bin")
    app_out = os.path.join(release_dir, f"{_release_name()}-{version}.bin")
    full_out = os.path.join(release_dir, f"{_release_name()}-{version}-full.bin")

    with open(app_bin, "rb") as src, open(app_out, "wb") as dst:
        dst.write(src.read())

    # FLASH_EXTRA_IMAGES holds bootloader, partition table and boot_app0 with their offsets.
    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), app_bin))
    image_args = " ".join(f"{offset} \"{path}\"" for offset, path in images)

    env.Execute(
        f"\"$PYTHONEXE\" \"$OBJCOPY\" --chip esp32s3 merge_bin -o \"{full_out}\" "
        f"--flash_mode dio --flash_freq 80m --flash_size 8MB {image_args}"
    )
    print(f"Release images:\n  {app_out}\n  {full_out}")


def _make_release_if_built(source, target, env):
    # "checkprogsize" runs on every build but, on a fresh build, before firmware.bin exists.
    if os.path.isfile(os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")):
        _make_release(source, target, env)


# Rebuilt firmware: after firmware.bin is written.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _make_release)  # noqa: F821
# Unchanged firmware (e.g. the release folder was deleted): on the always-run size check.
env.AddPostAction("checkprogsize", _make_release_if_built)  # noqa: F821
