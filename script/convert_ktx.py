import os
import multiprocessing as mp

import utils

ROOT_DIR = utils.find_root_dir()
DOCS_DIR = os.path.expanduser("~/Documents")


def __do_once(src_path: str) -> str:
    src_path = src_path
    dst_path = os.path.splitext(src_path)[0] + ".ktx"

    commands = [
        "ktx",
        "create",
        "--format R8G8B8_SRGB",
        "--encode uastc",
        "--assign-oetf srgb",
        "--generate-mipmap",
        f'"{src_path}"',
        f'"{dst_path}"',
    ]
    cmd = " ".join(commands)
    os.system(cmd)
    return dst_path


def main():
    work_list = [
        os.path.join(ROOT_DIR, r"asset\textures\missing_texture.png"),
        os.path.join(DOCS_DIR, r"Mirinapp\ThinMatrix\Character Texture.png"),
    ]

    with mp.Pool(mp.cpu_count()) as pool:
        for dst_path in pool.imap_unordered(__do_once, work_list):
            print(f"Converted: {dst_path}")


if __name__ == "__main__":
    main()
