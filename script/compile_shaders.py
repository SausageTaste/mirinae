import multiprocessing as mp
import os
import shutil
import time

import utils


START_TIME = time.time()

ROOT_DIR = utils.find_root_dir()
ASSET_DIR = os.path.join(ROOT_DIR, "asset")
GLSL_DIR = os.path.join(ROOT_DIR, "asset", "glsl")
INCLUDE_DIRECTIVE = "#include"
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

            file_ext = os.path.splitext(file_name_ext)[-1]
            if file_ext not in INPUT_EXTENSIONS:
                continue

            yield file_path


def __is_glsl_up_to_date(glsl_file_path, output_path):
    if not os.path.isfile(output_path):
        return False

    out_mtime = os.path.getmtime(output_path)
    if os.path.getmtime(glsl_file_path) > out_mtime:
        return False

    with open(glsl_file_path, "r") as f:
        for line in f:
            if line.count(INCLUDE_DIRECTIVE):
                loc = line.find(INCLUDE_DIRECTIVE)
                line = line[loc + len(INCLUDE_DIRECTIVE):]
                line = line.strip().strip("\"").strip("<").strip(">")
                line = os.path.join(os.path.dirname(glsl_file_path), line)
                dep_mtime = os.path.getmtime(line)
                if dep_mtime > out_mtime:
                    return False

    return True


def __compile_one(file_path):
    if not os.path.isfile(file_path):
        return None

    output_path = __make_output_path(file_path)
    if __is_glsl_up_to_date(file_path, output_path):
        return None

    cmd = f'{COMPILER_PATH} "{file_path}" -o "{output_path}"'
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    return 0 == os.system(cmd)


def main():
    work_count = 0
    success_count = 0

    with mp.Pool(PROC_COUNT) as pool:
        for result in pool.imap_unordered(__compile_one, __gen_glsl_file()):
            if result is None:
                continue
            work_count += 1
            success_count += 1 if result else 0

    if (work_count == 0):
        print("Nothing to compile.")
    else:
        if (success_count == work_count):
            print(f"Compiled {success_count}/{work_count} (\033[96m{success_count / work_count:.0%}\033[0m) shaders")
        else:
            print(f"\nCompiled {success_count}/{work_count} (\033[91m{success_count / work_count:.0%}\033[0m) shaders")
    print(f"Time taken: {time.time() - START_TIME:.2f} seconds")


if "__main__" == __name__:
    main()
