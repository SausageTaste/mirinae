#include "mirinae/platform/filesys.hpp"


namespace mirinae {

    std::unique_ptr<IFilesys> create_filesys_std() {
        return nullptr;
    }

}
