import os
import time
import multiprocessing as mp

import utils


VULKAN_SDK_DIR = utils.find_vulkan_sdk();
COMPILER_PATH = os.path.join(VULKAN_SDK_DIR, "Bin", "glslc.exe")
ROOT_DIR = utils.find_root_dir()
ASSET_DIR = os.path.join(ROOT_DIR, "asset")
GLSL_DIR = os.path.join(ROOT_DIR, "asset", "glsl")
PROC_COUNT = 8

INPUT_EXTENSIONS = {
    ".vert",
    ".frag",
    ".comp",
    ".geom",
    ".tesc",
    ".tese",
}


def __make_output_path(file_path: str):
    loc, file_name_ext = os.path.split(file_path)
    file_name, file_ext = os.path.splitext(file_name_ext)

    rel_loc = os.path.relpath(loc, GLSL_DIR).strip(".")
    rel_loc = rel_loc.replace("/", "_").replace("\\", "_")
    if (rel_loc != ""):
        rel_loc += "_"

    output_filename_ext = "{}{}_{}.spv".format(rel_loc, file_name, file_ext.strip("."))
    return os.path.join(ROOT_DIR, "asset", "spv", output_filename_ext)


def __gen_glsl_file():
    for loc, folders, files in os.walk(GLSL_DIR):
        for file_name_ext in files:
            file_path = os.path.join(loc, file_name_ext)
            if not os.path.isfile(file_path):
                continue

            file_ext = os.path.splitext(file_name_ext)[-1]
            if file_ext not in INPUT_EXTENSIONS:
                continue

            output_path = __make_output_path(file_path)
            if os.path.isfile(output_path):
                src_mtime = os.path.getmtime(file_path)
                out_mtime = os.path.getmtime(output_path)
                if src_mtime <= out_mtime:
                    continue

            yield file_path


def __compile_one(file_path):
    st = time.time()
    output_path = __make_output_path(file_path)
    cmd = f'{COMPILER_PATH} "{file_path}" -o "{output_path}"'
    return file_path, 0 == os.system(cmd), time.time() - st


def main():
    if not os.path.isfile(COMPILER_PATH):
        raise FileNotFoundError("GLSL compiler not found at '{}'".format(COMPILER_PATH))

    work_count = 0
    success_count = 0
    utils.try_mkdir(ASSET_DIR)
    utils.try_mkdir(os.path.join(ASSET_DIR, "spv"))

    st = time.time()
    with mp.Pool(PROC_COUNT) as pool:
        for result in pool.imap_unordered(__compile_one, __gen_glsl_file()):
            file_path, success, time_taken = result
            work_count += 1
            success_count += 1 if success else 0
            print(f"{'Success' if success else 'Failed'} ({time_taken:.2f} sec): {file_path}")
    et = time.time()

    if (work_count == 0):
        print("No shaders to compile.")
    else:
        success_rate = success_count / work_count
        print(f"\nCompiled {success_count}/{work_count} ({success_rate:.2%}) shaders successfully.")
        print(f"Time taken: {et - st:.2f} seconds.")


if "__main__" == __name__:
    main()
