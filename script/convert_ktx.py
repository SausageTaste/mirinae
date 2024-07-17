import os

import utils

ROOT_DIR = utils.find_root_dir()


def main():
    src_path = os.path.join(ROOT_DIR, r"asset\textures\missing_texture.png")
    dst_path = os.path.join(ROOT_DIR, r"asset\textures\missing_texture.ktx")

    commands = [
        "ktx",
        "create",
        "--format R8G8B8_SRGB",
        "--encode uastc",
        "--assign-oetf srgb",
        src_path,
        dst_path,
    ]
    cmd = " ".join(commands)
    print(cmd)
    os.system(cmd)


if __name__ == "__main__":
    main()
