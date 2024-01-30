#include "dump.hpp"

#include <tchar.h>
#include <dbghelp.h>
#include <stdio.h>
#include <crtdbg.h>
#include <cstring>


namespace {

    class WindowsFileHandle {

    public:
        WindowsFileHandle(const char* file_name) {
            handle_ = CreateFileA(file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }

        ~WindowsFileHandle() {
            if (INVALID_HANDLE_VALUE != handle_) {
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
            }
        }

        bool operator!() const {
            return this->is_ready();
        }

        HANDLE operator*() {
            return handle_;
        }

        bool is_ready() const {
            return INVALID_HANDLE_VALUE == handle_;
        }

    private:
        HANDLE handle_ = INVALID_HANDLE_VALUE;

    };

}


namespace mirinae { namespace windows {

    void create_minidump(EXCEPTION_POINTERS* pep) {
        WindowsFileHandle file{ "dying_msg.dmp" };
        if (!file) {
            _tprintf(_T("Minidump CreateFile failed: %u \n"), GetLastError());
            return;
        }

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = pep;
        mdei.ClientPointers = FALSE;

        const auto mdt = (MINIDUMP_TYPE)(
            MiniDumpWithFullMemory
            | MiniDumpWithFullMemoryInfo
            | MiniDumpWithHandleData
            | MiniDumpWithThreadInfo
            | MiniDumpWithUnloadedModules
            );

        if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), *file, mdt, (pep != 0) ? &mdei : 0, 0, 0))
            _tprintf(_T("Minidump 'dying_msg.dmp' created.\n"));
        else
            _tprintf(_T("MiniDumpWriteDump failed. Error: %u \n"), GetLastError());
    }

}}
