#pragma once

#include <memory>


namespace mirinae {

    class INetworkServer {

    public:
        virtual ~INetworkServer() = default;
        virtual void do_frame() = 0;

    };


    class INetworkClient {

    public:
        virtual ~INetworkClient() = default;
        virtual void do_frame() = 0;

        virtual void send() = 0;

    };


    std::unique_ptr<INetworkServer> create_server();
    std::unique_ptr<INetworkClient> create_client();

}
