import os

import utils


ROOT_DIR = utils.find_root_dir()

EXCLUDE_FOLDERS = {
    "__pycache__",
    ".cxx",
    ".git",
    ".idea",
    ".vscode",
    "build",
}

INCLUDE_EXTENSIONS = {
    "c",
    "cpp",
    "h",
    "hpp",
    "glsl",
    "txt",
    "py",
    "vert",
    "frag",
}


def __is_code_file(file_path):
    loc, file_name_ext = os.path.split(file_path)
    file_name, file_ext = os.path.splitext(file_name_ext)

    if file_ext.strip(".") in INCLUDE_EXTENSIONS:
        return True
    if file_name_ext == "CMakeLists.txt":
        return True

    return False


def main():
    db = []

    for loc, folders, files in os.walk(ROOT_DIR):
        elements = os.path.normpath(loc).split(os.sep)
        if EXCLUDE_FOLDERS.intersection(set(elements)):
            continue

        for file_name_ext in files:
            file_path = os.path.join(loc, file_name_ext)
            if not __is_code_file(file_path):
                continue

            with open(file_path, "r", encoding="utf-8") as f:
                lines = f.readlines()
            db.append((len(lines), file_path))

    db.sort(key=lambda x: x[0], reverse=False)
    for line_count, file_path in db:
        print(f"{line_count:6d} {os.path.relpath(file_path, ROOT_DIR)}")
    print(f"\nTotal {len(db)} files.")
    print(f"Total {sum(x[0] for x in db)} lines.")


if "__main__" == __name__:
    main()
