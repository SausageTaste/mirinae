#pragma once

#include <Windows.h>


namespace mirinae { namespace windows {

    void create_minidump(EXCEPTION_POINTERS* pep);

}}  // namespace mirinae::windows
