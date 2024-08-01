#pragma once

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <daltools/filesys/filesys.hpp>


namespace mirinapp {

    std::unique_ptr<dal::IFileSubsys> create_filesubsys_android_asset(
        AAssetManager* mgr, dal::Filesystem& filesys
    );

}
