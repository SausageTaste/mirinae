#pragma once

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <mirinae/platform/filesys.hpp>


namespace mirinapp {

    std::unique_ptr<mirinae::IFilesys> create_filesys_android_asset(AAssetManager* mgr);

}
