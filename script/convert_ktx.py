import multiprocessing as mp
import subprocess
import os
import pathlib

import utils

ROOT_DIR = pathlib.Path(utils.find_root_dir())
DOCS_DIR = pathlib.Path(os.path.expanduser("~/Documents"))


def __do_once(src_path: str, srgb: bool = True, flip_y: bool = False) -> str:
    dst_path = os.path.splitext(src_path)[0] + ".ktx"

    commands = [
        "ktx",
        "create",
        "--encode", "uastc",
        "--generate-mipmap",
    ]

    if srgb:
        commands.append("--assign-tf")
        commands.append("srgb")
        commands.append("--convert-tf")
        commands.append("srgb")
        commands.append("--format")
        commands.append("R8G8B8_SRGB")
    else:
        commands.append("--assign-tf")
        commands.append("linear")
        commands.append("--convert-tf")
        commands.append("linear")
        commands.append("--format")
        commands.append("R8G8B8_UNORM")

    if flip_y:
        commands.append("--convert-texcoord-origin")
        commands.append("bottom-left")

    commands.append(src_path)
    commands.append(dst_path)

    result = subprocess.run(commands)
    if result.returncode != 0:
        raise RuntimeError(f"Failed to convert {src_path} to KTX: {result.stderr}")

    return dst_path


def main():
    img_dir = pathlib.Path(r"C:\Users\user\Documents\GitHub\KorSimGL\asset\image")
    work_list = [
       img_dir / "half_transparent_red.png",
       img_dir / "earth_albedo.jpg",
       img_dir / "8k_stars_milky_way.jpg",
    ]

    with mp.Pool() as pool:
        for dst_path in pool.imap_unordered(__do_once, work_list):
            print(f"Converted: {dst_path}")


if __name__ == "__main__":
    main()
