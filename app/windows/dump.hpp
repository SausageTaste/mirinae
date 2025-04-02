#pragma once

#include <sung/basic/os_detect.hpp>

#if defined(SUNG_OS_WINDOWS)

#include <Windows.h>

namespace mirinae { namespace windows {

    void create_minidump(EXCEPTION_POINTERS* pep);

}}  // namespace mirinae::windows

#endif


#if true

#define MIRINAE_WIN_TRY __try
#define MIRINAE_WIN_CATCH                                             \
    __except (                                                        \
        mirinae::windows::create_minidump(GetExceptionInformation()), \
        EXCEPTION_EXECUTE_HANDLER                                     \
    )


#else

#define MIRINAE_WIN_TRY try
#define MIRINAE_WIN_CATCH catch (...)

#endif
