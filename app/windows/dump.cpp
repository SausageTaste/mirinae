#include "dump.hpp"

#ifdef _MSC_VER

#include <array>
#include <string>

#include <crtdbg.h>
#include <dbghelp.h>
#include <stdio.h>
#include <tchar.h>


namespace {

    class WindowsFileHandle {

    public:
        WindowsFileHandle(const char* file_name) {
            handle_ = CreateFileA(
                file_name,
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
        }

        ~WindowsFileHandle() {
            if (INVALID_HANDLE_VALUE != handle_) {
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
            }
        }

        bool operator!() const { return this->is_ready(); }
        HANDLE operator*() { return handle_; }
        bool is_ready() const { return INVALID_HANDLE_VALUE == handle_; }

        std::string get_full_path() {
            std::array<TCHAR, 1024> path;
            const auto res = GetFinalPathNameByHandleA(
                handle_, path.data(), path.size(), 0
            );
            return std::string{ path.data(), path.data() + res };
        }

    private:
        HANDLE handle_ = INVALID_HANDLE_VALUE;
    };

}  // namespace


namespace mirinae { namespace windows {

    void create_minidump(EXCEPTION_POINTERS* pep) {
        WindowsFileHandle file{ "dying_msg.dmp" };
        if (!file) {
            _tprintf(_T("Minidump CreateFile failed (%u)\n"), GetLastError());
            return;
        }

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = pep;
        mdei.ClientPointers = FALSE;

        const auto mdt = (MINIDUMP_TYPE)(MiniDumpWithFullMemory |
                                         MiniDumpWithFullMemoryInfo |
                                         MiniDumpWithHandleData |
                                         MiniDumpWithThreadInfo |
                                         MiniDumpWithUnloadedModules);

        const auto result = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            *file,
            mdt,
            (pep != 0) ? &mdei : 0,
            0,
            0
        );

        if (result) {
            const auto full_path = file.get_full_path();
            _tprintf(_T("Minidump '%s' created\n"), full_path.c_str());
        } else {
            _tprintf(_T("MiniDumpWriteDump failed (%u)\n"), GetLastError());
        }
    }

}}  // namespace mirinae::windows

#endif
