import os
import time
import multiprocessing as mp

import psutil

import utils


VULKAN_SDK_DIR = utils.find_vulkan_sdk();
COMPILER_PATH = os.path.join(VULKAN_SDK_DIR, "Bin", "glslc.exe")
ROOT_DIR = utils.find_root_dir()
ASSET_DIR = os.path.join(ROOT_DIR, "asset")
GLSL_DIR = os.path.join(ROOT_DIR, "asset", "glsl")
PROC_COUNT = psutil.cpu_count(logical=False) // 2


def __gen_glsl_file():
    for item_name_ext in os.listdir(GLSL_DIR):
        item_path = os.path.join(GLSL_DIR, item_name_ext)
        if os.path.isfile(item_path):
            yield item_name_ext


def __compile_one(glsl_filename_ext):
    glsl_filename, glsl_ext = os.path.splitext(glsl_filename_ext)
    glsl_path = os.path.join(GLSL_DIR, glsl_filename_ext)
    output_filename_ext = "{}_{}.spv".format(glsl_filename, glsl_ext.strip("."))
    output_path = os.path.join(ROOT_DIR, "asset", "spv", output_filename_ext)
    cmd = f'{COMPILER_PATH} "{glsl_path}" -o "{output_path}"'
    return 0 == os.system(cmd)


def main():
    if not os.path.isfile(COMPILER_PATH):
        raise FileNotFoundError("GLSL compiler not found at '{}'".format(COMPILER_PATH))

    work_count = 0
    success_count = 0
    utils.try_mkdir(ASSET_DIR)
    utils.try_mkdir(os.path.join(ASSET_DIR, "spv"))

    st = time.time()
    with mp.Pool(PROC_COUNT) as pool:
        for x in pool.imap_unordered(__compile_one, __gen_glsl_file()):
            work_count += 1
            success_count += 1 if x else 0
    et = time.time()

    print(f"\nCompiled {success_count}/{work_count} shaders successfully.")
    print(f"Time taken: {et - st:.2f} seconds.")


if "__main__" == __name__:
    main()
