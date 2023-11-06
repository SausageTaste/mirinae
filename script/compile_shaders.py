import os

import utils


def __try_mkdir(path: str):
    try:
        os.mkdir(path)
    except FileExistsError:
        pass


def __create_shader_folder(root_dir: str):
    __try_mkdir(os.path.join(root_dir, "asset"))
    __try_mkdir(os.path.join(root_dir, "asset", "spv"))


def main():
    vulkan_sdk_dir = utils.find_vulkan_sdk();
    root_dir = utils.find_root_dir()
    glsl_dir = os.path.join(root_dir, "asset", "glsl")
    compiler_path = os.path.join(vulkan_sdk_dir, "Bin", "glslc.exe")
    if not os.path.isfile(compiler_path):
        raise FileNotFoundError("GLSL compiler not found at '{}'".format(compiler_path))

    for glsl_filename_ext in os.listdir(glsl_dir):
        glsl_filename, glsl_ext = os.path.splitext(glsl_filename_ext)
        glsl_path = os.path.join(glsl_dir, glsl_filename_ext)

        if os.path.isfile(glsl_path):
            output_filename_ext = "{}_{}.spv".format(glsl_filename, glsl_ext.strip("."))
            output_path = os.path.join(root_dir, "resources", "shaders", output_filename_ext)
            cmd = f'{compiler_path} "{glsl_path}" -o "{output_path}"'

            print(cmd)
            __create_shader_folder(root_dir)
            os.system(cmd)


if "__main__" == __name__:
    main()
