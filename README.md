# Mirinae

<div align="left">
      <a href="https://www.youtube.com/watch?v=U9A9zl4I_Kk">
         <img src="https://img.youtube.com/vi/U9A9zl4I_Kk/maxresdefault.jpg">
      </a>
</div>

> Click above image to watch it on YouTube

A rendering engine powered by Vulkan.

# Points of Interest

If you want to read through source codes, here are some good starting points.

1. [Main entry point](/app/sdl/main.cpp)
1. [Render passes](/lib/vulkan/src/renderpass)
    - This is where the most complex rendering topics are actually implemented, such as atmospheric scattering, Tessendorf ocean, etc.
1. [GLSL shaders](/asset/glsl)
    - Note the directory structure resembles that of render passes. Hope that helps finding corresponding shader files.
1. [Slang shaders](/asset/slang)
    - GLSL shaders have slowly been converted to Slang. My goal is completely replacing GLSL sources with Slang.

# How to build

This app is mainly targeting Windows, but it's also frequently tested on Ubuntu and Android.
For those interested in Android version, there is [Android Studio project](https://github.com/SausageTaste/Mirinae-Android) that contains this repo as a submodule.
Here, I'm only describing how to build on Windows.

## Build with CMake

You need following softwares:

* [Git](https://git-scm.com/)
* [CMake](https://cmake.org/download/)
* C++ built tool
    * Preferably MSVC++, but GCC should work
* [LunarG Vulkan SDK](https://vulkan.lunarg.com/)
    * For Vulkan headers and `glslc`, `slangc` compilers
* [vcpkg](https://github.com/microsoft/vcpkg)
    * With an environment variable `VCPKG_ROOT` set
* [Python](https://www.python.org/downloads/)
    * There's a Python script to compile GLSL shader sources into SPR-V

Now that all these are ready, you can simply

```
git clone --recurse-submodules -j8 https://github.com/SausageTaste/mirinae
cd mirinae
cmake -S . -B ./build
cmake --build ./build
```

Make sure all submodules are properly cloned!

The output executable file is located at `<repo>/buildd/app/windows/Debug/mirinapp.exe`. You cannot run it yet, because shader files not ready.

## Compile Shaders

Run Python scripts `<repo>/script/compile_glsl.py` for GLSL and `<repo>/script/compile_slang.py` for Slang.
They compile shader sources in `<repo>/asset/glsl` and `<repo>/asset/slang`, and save SPIR-V shader files in `<repo>/asset/spv`.

The script uses `glslc` and `slangc` programs, which are bundled in LunarG Vulkan SDK.
If the SDK was installed in somewhere else than `C:/VulkanSDK`, the scripts might fail to find them.
The simplest solution is to add the folder containing `glslc` and `slangc` to `PATH` environment variable.

Once finished, you should see a number of `.spv` files in `<repo>/asset/spv` directory.
Now you are good to go!

## It Might Not Be Enough Yet!

This project is under active development, and it may crash if some assets are missing.
If that happens, it's a bug so please make an issue.
All you need to run the app are *executable* files, *shared library* files, and `<repo>/asset` folder, and SPIR-V shaders.

# Control

## Keyboard

|Key |Action
|- |-
|WASD |Move
|Arrow keys |Look around
|E |Ascend
|Q |Descend
|Left shift |Move faster
|` |ImGui console
|Alt + Enter |Toggle fullscreen

## Mouse

|Key |Action
|- |-
|Right mouse click |Look around
