import multiprocessing as mp
import os
import shutil
import subprocess
import time

import utils


START_TIME = time.time()
ROOT_DIR = os.path.join(os.path.dirname(__file__), "..")
ASSET_DIR = os.path.join(ROOT_DIR, "asset")
SLANG_DIR = os.path.join(ASSET_DIR, "slang")
IMPORT_DIRECTIVE = "import "
RELEASE_MODE = True
SKIP_UP_TO_DATE = True

ENTRY_POINTS = {
    "vert_main",
    "frag_main",
    "tesc_main",
    "tese_main",
    "comp_main",
}

OUTPUT_CONFIGS = (
    ("spirv", ".spv"),
    # ("glsl", ".glsl"),
)


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
    return os.path.join(ASSET_DIR, "spv", output_filename_ext)


def __is_slang_up_to_date(slang_file_path, output_path):
    if not os.path.isfile(output_path):
        return False

    out_mtime = os.path.getmtime(output_path)
    if os.path.getmtime(slang_file_path) > out_mtime:
        return False

    with open(slang_file_path, "r") as f:
        for line in f:
            idx = line.find(IMPORT_DIRECTIVE)
            if -1 != idx:
                imported_path = line[idx + len(IMPORT_DIRECTIVE):].strip().strip(";").strip('"').strip("<>").strip()
                imported_full_path = os.path.join(os.path.dirname(slang_file_path), imported_path) + ".slang"
                imported_full_path = os.path.normpath(imported_full_path)
                assert os.path.isfile(imported_full_path), f"Imported slang file not found: {imported_full_path} (from {slang_file_path})"

                if os.path.getmtime(imported_full_path) > out_mtime:
                    return False

    return True


def __gen_slang_files():
    for loc, folders, files in os.walk(SLANG_DIR):
        if loc.endswith("module"):
            continue

        for file_name_ext in files:
            file_path = os.path.join(loc, file_name_ext)

            file_ext = os.path.splitext(file_name_ext)[-1]
            if file_ext != ".slang":
                continue

            yield file_path


def __gen_one_slang_cmds(file_path):
    if not os.path.isfile(file_path):
        return

    output_prefix = __make_output_prefix(file_path, SLANG_DIR)
    os.makedirs(os.path.dirname(output_prefix), exist_ok=True)

    found_entry_points = []
    with open(file_path, "r") as f:
        content = f.read()
        for x in ENTRY_POINTS:
            if content.count(x):
                found_entry_points.append(x)

    if not found_entry_points:
        print(f"Warning: No entry points found in {file_path}.")
        return

    basic_cmd = [
        SLANGC_PATH,
        file_path,
        "-profile", "glsl_450",
    ]

    if RELEASE_MODE:
        basic_cmd.append("-O3")
        basic_cmd.append("-obfuscate")
    else:
        basic_cmd.append("-O0")
        basic_cmd.append("-g3")

    for func_name in found_entry_points:
        suffix = func_name.strip("_main")
        entry_cmd = basic_cmd + [
            "-entry", func_name,
            "-target",
        ]

        for target, ext in OUTPUT_CONFIGS:
            output_file_path = f"{output_prefix}_{suffix}{ext}"
            if SKIP_UP_TO_DATE and __is_slang_up_to_date(file_path, output_file_path):
                continue
            yield entry_cmd + [target, "-o", output_file_path]


def __gen_cmds():
    for file_path in __gen_slang_files():
        yield from __gen_one_slang_cmds(file_path)


def __exec_cmd(cmd: list[str]):
    return subprocess.run(cmd).returncode == 0


def main():
    work_count = 0
    success_count = 0

    with mp.Pool() as pool:
        for result in pool.imap_unordered(__exec_cmd, __gen_cmds()):
            if result is None:
                continue
            work_count += 1
            success_count += 1 if result else 0
            print(f"Compiled {success_count}/{work_count} shaders", end="\r")

    if (work_count == 0):
        print("Nothing to compile.")
    else:
        if (success_count == work_count):
            print(f"Compiled {success_count}/{work_count} (\033[96m{success_count / work_count:.0%}\033[0m) shaders")
        else:
            print(f"Compiled {success_count}/{work_count} (\033[91m{success_count / work_count:.0%}\033[0m) shaders")
    print(f"Time taken: {time.time() - START_TIME:.2f} seconds")


if "__main__" == __name__:
    main()
