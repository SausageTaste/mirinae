#pragma once

#include <Windows.h>

#if true

#include <Windows.h>

#define MIRINAE_WIN_TRY __try
#define MIRINAE_WIN_CATCH                                             \
    __except (                                                        \
        mirinae::windows::create_minidump(GetExceptionInformation()), \
        EXCEPTION_EXECUTE_HANDLER                                     \
    )


namespace mirinae { namespace windows {

    void create_minidump(EXCEPTION_POINTERS* pep);

}}  // namespace mirinae::windows

#else

#define MIRINAE_WIN_TRY try
#define MIRINAE_WIN_CATCH catch (...)

#endif
