# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#
# PlatformIO post-build step. Produces in ./release:
#   VoiceRecCardputer-<version>.bin       app image for M5Launcher (SD card -> Install)
#   VoiceRecCardputer-<version>-full.bin  merged image (bootloader + partitions + app)
import os
import re

Import("env")  # noqa: F821

PROJECT_NAME = "VoiceRecCardputer"


def _app_version():
    for flag in env.get("BUILD_FLAGS", []):  # noqa: F821
        match = re.search(r'APP_VERSION=\\?"([^"\\]+)', flag)
        if match:
            return match.group(1)
    return "1.0.0"


def _make_release(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    release_dir = os.path.join(project_dir, "release")
    os.makedirs(release_dir, exist_ok=True)

    version = _app_version()
    app_bin = os.path.join(build_dir, "firmware.bin")
    app_out = os.path.join(release_dir, f"{PROJECT_NAME}-{version}.bin")
    full_out = os.path.join(release_dir, f"{PROJECT_NAME}-{version}-full.bin")

    with open(app_bin, "rb") as src, open(app_out, "wb") as dst:
        dst.write(src.read())

    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), app_bin))
    image_args = " ".join(f"{offset} \"{path}\"" for offset, path in images)

    env.Execute(
        f"\"$PYTHONEXE\" \"$OBJCOPY\" --chip esp32s3 merge_bin -o \"{full_out}\" "
        f"--flash_mode dio --flash_freq 80m --flash_size 8MB {image_args}"
    )
    print(f"\n[Release Assets Created]:\n  App image:    {app_out}\n  Merged image: {full_out}\n")


def _make_release_if_built(source, target, env):
    if os.path.isfile(os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")):
        _make_release(source, target, env)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _make_release)  # noqa: F821
env.AddPostAction("checkprogsize", _make_release_if_built)  # noqa: F821
