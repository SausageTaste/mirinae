# Mirinae

<div align="left">
      <a href="https://www.youtube.com/watch?v=RZYguhi-I4U">
         <img src="https://img.youtube.com/vi/RZYguhi-I4U/maxresdefault.jpg">
      </a>
</div>

A game engine powered by Vulkan.

# Points of Interest

If you want to read though sources, here are some good starting points.

1. [Main entry point](/app/sdl/main.cpp)
1. [Render passes](/lib/vulkan/src/renderpass)
    - This is where the most complex rendering topics are actually implemented, such as atmospheric scattering, Tessendorf ocean, etc.
1. [Shader sources](/asset/glsl)
    - Note the directory structure resembles that of render passes, which will ease associating with each other

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
    * For Vulkan headers and glslc compiler
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

Run a Python script at `<repo>/script/compile_glsl.py`.
It compiles GLSL shader sources in `<repo>/asset/glsl`, and saves SPR-V shader files in `<repo>/asset/spv`.

The script uses `glslc` program, which is bundled in LunarG Vulkan SDK.
If the SDK was installed in somewhere else than `C:/VulkanSDK`, the script might fail to find it.
The simplest solution is to add the folder containing `glslc` to `PATH` environment variable.
Please refer to `__find_glslc` function in the script for more detail as to finding `glslc`.

I'm currently slowly migrating GLSL shaders to [Slang](https://github.com/shader-slang/slang).
Please run `<repo>/script/compile_slang.py` similarly.
The compilation time is way longer with Slang, but the rich language features are so awesome that it's worth it.

Once it's done, you should see a number of `.spv` files in `<repo>/asset/spv` directory.
And finally you can execute `mirinapp.exe`!

## It Might Not Be Enough Yet!

This project is under active development, and sometimes it crashes if there are not required assets.
If that happens, it's a bug so please make an issue.
All you need to run the app are *executable* files, *shared library* files, and `<repo>/asset` folder.
Don't forget to compile shaders, though.

# Control

## Keyboard

|Key |Action
|- |-
|WASD |Move
|Arrow keys |Look around
|Space |Ascend
|Left alt |Descend
|` |ImGui console
|Alt + Enter |Toggle fullscreen

## Mouse

|Key |Action
|- |-
|Right mouse click |Look around
