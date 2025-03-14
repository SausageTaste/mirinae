import multiprocessing as mp
import os
import shutil
import time

import utils


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


def __find_glslc():
    if shutil.which("glslc"):
        return "glslc"

    path = os.path.join(utils.find_vulkan_sdk(), "Bin", "glslc.exe")
    if shutil.which(path):
        return path

    raise FileNotFoundError("GLSL compiler not found.")

COMPILER_PATH = __find_glslc()


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
    st = time.time()

    work_count = 0
    success_count = 0
    utils.try_mkdir(ASSET_DIR)
    utils.try_mkdir(os.path.join(ASSET_DIR, "spv"))

    with mp.Pool(PROC_COUNT) as pool:
        for result in pool.imap_unordered(__compile_one, __gen_glsl_file()):
            file_path, success, time_taken = result
            work_count += 1
            success_count += 1 if success else 0

    if (work_count == 0):
        print("Nothing to compile.")
    else:
        success_rate = success_count / work_count
        if (success_count == work_count):
            print(f"\nCompiled {success_count}/{work_count} (\033[96m{success_rate:.0%}\033[0m) shaders")
        else:
            print(f"\nCompiled {success_count}/{work_count} (\033[91m{success_rate:.0%}\033[0m) shaders")
    print(f"Time taken: {time.time() - st:.2f} seconds.")


if "__main__" == __name__:
    main()
