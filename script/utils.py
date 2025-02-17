import os
from typing import Optional


def _is_dir_root(path: str):
    if not os.path.isdir(path):
        return False

    required_contents = {
        "asset",
        ".gitignore",
        "README.md",
    }
    contents = set(os.listdir(path));
    diff = required_contents.difference(contents)
    return 0 == len(diff)


def find_root_dir() -> Optional[str]:
    cur_dir = "."
    for i in range(10):
        if _is_dir_root(cur_dir):
            return os.path.abspath(cur_dir)
        else:
            cur_dir = os.path.join(cur_dir, "..")

    return None


def find_vulkan_sdk() -> str:
    VULKAN_SDK_DIR = "C:/VulkanSDK"

    if not os.path.isdir(VULKAN_SDK_DIR):
        raise FileNotFoundError(f"Vulkan SDK not found")

    found = []
    for version_folder_name in reversed(os.listdir(VULKAN_SDK_DIR)):
        sdk_version_folder = os.path.join(VULKAN_SDK_DIR, version_folder_name)
        if not os.path.isdir(sdk_version_folder):
            continue
        if not os.path.isfile(os.path.join(sdk_version_folder, "Bin", "vkcube.exe")):
            continue
        found.append(os.path.normpath(sdk_version_folder))

    if not found:
        raise FileNotFoundError(f"Vulkan SDK not found")

    return found[-1]


def try_mkdir(path: str):
    try:
        os.mkdir(path)
    except FileExistsError:
        pass
