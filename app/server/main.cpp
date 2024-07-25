#include <sung/general/os_detect.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/network.hpp"
#include "mirinae/lightweight/script.hpp"

#ifdef SUNG_OS_WINDOWS
    #include "dump.hpp"
#endif


namespace {

    class Server {

    public:
        Server() {
            server_ = mirinae::create_server();
            script_ = std::make_shared<mirinae::ScriptEngine>();
            cosmos_ = std::make_shared<mirinae::CosmosSimulator>(*script_);
        }

        void do_frame() {
            server_->do_frame();
            cosmos_->do_frame();
        }

        std::unique_ptr<mirinae::INetworkServer> server_;
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
    };


    int start() {
        ::Server server;

        while (true) server.do_frame();

        return 0;
    }

}  // namespace


int main() {
#ifdef SUNG_OS_WINDOWS
    __try {
#endif

        return ::start();

#ifdef SUNG_OS_WINDOWS
    } __except (
        mirinae::windows::create_minidump(GetExceptionInformation()),
        EXCEPTION_EXECUTE_HANDLER
    ) {
        std::abort();
    }
#endif
}
