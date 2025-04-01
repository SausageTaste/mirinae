# Mirinae

https://github.com/user-attachments/assets/794bda9c-f5d2-4ddf-a1aa-7d468bdc9fcb

A game engine powered by Vulkan.

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

Run a Python script at `<repo>/script/compile_shaders.py`.
It compiles GLSL shader sources in `<repo>/asset/glsl`, and saves SPR-V shader files in `<repo>/asset/spv`.

The script uses `glslc` program, which is bundled in LunarG Vulkan SDK.
If the SDK was installed in somewhere else than `C:/VulkanSDK`, the script might fail to find it.
The simplest solution is to add the folder containing `glslc` to `PATH` environment variable.
Please refer to `__find_glslc` function in the script for more detail as to finding `glslc`.

Once it's done, you should see a number of `.spv` files in `<repo>/asset/spv` directory.
And finally you can execute `mirinapp.exe`!

## It Might Not Be Enough Yet!

This project is under active development, and sometimes it crashes if there are not required assets.
If that happens, it's a bug so please make an issue.
All you need to run the app are *executable* files, *shared library* files, and `<repo>/asset` folder.
Oh, by the way, don't forget to compile shaders!

# Control

## Keyboard

|Key |Action
|- |-
|WASD |Move
|Arrow keys |Look around
|Space |Ascend
|Left shift |Descend
|` |ImGui console
|Alt + Enter |Toggle fullscreen

## Mouse

|Key |Action
|- |-
|Right mouse click |Look around
