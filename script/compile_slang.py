import multiprocessing as mp
import os
import shutil
import time

import utils


START_TIME = time.time()

ROOT_DIR = utils.find_root_dir()
ASSET_DIR = os.path.join(ROOT_DIR, "asset")
SLANG_DIR = os.path.join(ROOT_DIR, "asset", "slang")
INCLUDE_DIRECTIVE = "#include"

ENTRY_POINTS = {
    "vert_main",
    "frag_main",
}


def __find_slangc():
    if shutil.which("slangc"):
        return "slangc"

    path = os.path.join(utils.find_slang_sdk(), "bin", "slangc.exe")
    if shutil.which(path):
        return path

    raise FileNotFoundError("SLANG compiler not found.")

SLANGC_PATH = __find_slangc()


def __make_output_prefix(file_path: str, root_dir: str):
    loc, file_name_ext = os.path.split(file_path)
    file_name, file_ext = os.path.splitext(file_name_ext)

    rel_loc = os.path.relpath(loc, root_dir).strip(".")
    rel_loc = rel_loc.replace("/", "_").replace("\\", "_")
    if (rel_loc != ""):
        rel_loc += "_"

    output_filename_ext = rel_loc + file_name
    return os.path.join(ROOT_DIR, "asset", "spv", output_filename_ext)


def __gen_slang_files():
    for loc, folders, files in os.walk(SLANG_DIR):
        for file_name_ext in files:
            file_path = os.path.join(loc, file_name_ext)

            file_ext = os.path.splitext(file_name_ext)[-1]
            if file_ext != ".slang":
                continue

            yield file_path


def __compile_one_slang(file_path):
    if not os.path.isfile(file_path):
        return False

    output_prefix = __make_output_prefix(file_path, SLANG_DIR)
    os.makedirs(os.path.dirname(output_prefix), exist_ok=True)

    found_entry_points = []
    with open(file_path, "r") as f:
        content = f.read()
        for x in ENTRY_POINTS:
            if content.count(x):
                found_entry_points.append(x)

    for func_name in found_entry_points:
        suffix = func_name.strip("_main")
        out_glsl_path = f"{output_prefix}_{suffix}.glsl"
        if False and 0 != os.system(f'{SLANGC_PATH} "{file_path}" -capability glsl_spirv_1_0 -profile glsl_450 -target glsl -o "{out_glsl_path}" -entry {func_name}'):
            return False

        out_spv_path = f"{output_prefix}_{suffix}.spv"
        if 0 != os.system(f'{SLANGC_PATH} "{file_path}" -capability glsl_spirv_1_0 -profile glsl_450 -target spirv -o "{out_spv_path}" -entry {func_name}'):
            return False

    return True


def main():
    work_count = 0
    success_count = 0

    with mp.Pool() as pool:
        for result in pool.imap_unordered(__compile_one_slang, __gen_slang_files()):
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
