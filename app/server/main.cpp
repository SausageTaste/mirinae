#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/network.hpp"
#include "mirinae/lightweight/script.hpp"

#include "dump.hpp"


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
    __try {
        return ::start();
    } __except (
        mirinae::windows::create_minidump(GetExceptionInformation()),
        EXCEPTION_EXECUTE_HANDLER
    ) {
        std::abort();
    }
}
