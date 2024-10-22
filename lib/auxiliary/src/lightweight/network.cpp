#include "mirinae/lightweight/network.hpp"


namespace {

    class NullNetworkServer : public mirinae::INetworkServer {

    public:
        void do_frame() override {}
    };


    class NullNetworkClient : public mirinae::INetworkClient {

    public:
        void do_frame() override {}
        void send() override {}
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<INetworkServer> create_server() {
        return std::make_unique<NullNetworkServer>();
    }

    std::unique_ptr<INetworkClient> create_client() {
        return std::make_unique<NullNetworkClient>();
    }

}  // namespace mirinae
