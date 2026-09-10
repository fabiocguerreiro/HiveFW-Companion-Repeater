Import("env")

import os
import subprocess
from pathlib import Path


project_dir = Path(
    env.subst("$PROJECT_DIR")
)


platformio_flags = os.environ.get(
    "PLATFORMIO_BUILD_FLAGS",
    ""
)


# build.sh já injeta FIRMWARE_VERSION.
if (
    "-DFIRMWARE_VERSION=" in platformio_flags or
    "-D FIRMWARE_VERSION=" in platformio_flags
):
    print(
        "HiveFW version: supplied by build.sh"
    )

else:

    version_file = (
        project_dir /
        "VERSION"
    )

    if not version_file.exists():
        raise RuntimeError(
            "HiveFW VERSION file not found"
        )


    version = (
        version_file
        .read_text(encoding="utf-8")
        .strip()
    )


    if not version:
        raise RuntimeError(
            "HiveFW VERSION is empty"
        )


    try:
        commit_hash = (
            subprocess.check_output(
                [
                    "git",
                    "rev-parse",
                    "--short",
                    "HEAD"
                ],
                cwd=str(project_dir),
                text=True
            )
            .strip()
        )

    except Exception:
        commit_hash = "nogit"


    firmware_version = (
        version +
        "-" +
        commit_hash
    )


    env.Append(
        CPPDEFINES=[
            (
                "FIRMWARE_VERSION",
                '\\"' +
                firmware_version +
                '\\"'
            )
        ]
    )


    print(
        "HiveFW firmware version:",
        firmware_version
    )
