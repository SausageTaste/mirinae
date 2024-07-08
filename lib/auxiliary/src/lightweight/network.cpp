#include "mirinae/lightweight/network.hpp"

#include <stdexcept>

#include <enet/enet.h>
#include <spdlog/spdlog.h>


namespace {

    class EnetLibInit {

    public:
        static EnetLibInit& inst() {
            static EnetLibInit inst;
            return inst;
        }

    private:
        EnetLibInit() {
            if (enet_initialize() != 0) {
                throw std::runtime_error("Failed to initialize ENet library");
            }
        }

        ~EnetLibInit() { enet_deinitialize(); }
    };


    class NetworkServer : public mirinae::INetworkServer {

    public:
        NetworkServer() {
            volatile auto& enet_init = ::EnetLibInit::inst();

            address_.host = ENET_HOST_ANY;
            address_.port = 1234;

            host_ = enet_host_create(&address_, 32, 2, 0, 0);
            if (host_ == nullptr) {
                throw std::runtime_error("Failed to create ENet server");
            }
        }

        ~NetworkServer() override {
            enet_host_destroy(host_);
            host_ = nullptr;
        }

        void do_frame() override {
            ENetEvent event;
            const auto result = enet_host_service(host_, &event, 0);

            if (result <= 0)
                return;

            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    spdlog::info(
                        "A new client connected from {}:{}",
                        event.peer->address.host,
                        event.peer->address.port
                    );

                    /* Store any relevant client information here. */
                    event.peer->data = nullptr;

                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    spdlog::info(
                        "A packet of length {} was received",
                        event.packet->dataLength
                    );

                    /* Clean up the packet now that we're done using it. */
                    enet_packet_destroy(event.packet);

                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    spdlog::info("{} disconnected.\n", event.peer->data);

                    /* Reset the peer's client information. */

                    event.peer->data = NULL;
            }
        }

    private:
        ENetAddress address_;
        ENetHost* host_;
    };


    class NetworkClient : public mirinae::INetworkClient {

    public:
        NetworkClient() {
            volatile auto& enet_init = ::EnetLibInit::inst();

            host_ = enet_host_create(nullptr, 1, 2, 0, 0);
            if (host_ == nullptr) {
                throw std::runtime_error("Failed to create ENet client");
            }

            ENetAddress address;
            enet_address_set_host(&address, "127.0.0.1");
            address.port = 1234;

            server_peer_ = enet_host_connect(host_, &address, 2, 0);
            if (nullptr == server_peer_) {
                throw std::runtime_error(
                    "No available peers for initiating an ENet connection"
                );
            }
        }

        ~NetworkClient() override {
            enet_peer_reset(server_peer_);
            enet_host_destroy(host_);
        }

        void do_frame() override {
            ENetEvent event;
            const auto result = enet_host_service(host_, &event, 0);

            if (result <= 0)
                return;

            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    spdlog::info(
                        "A new client connected from {}:{}",
                        event.peer->address.host,
                        event.peer->address.port
                    );

                    /* Store any relevant client information here. */
                    event.peer->data = nullptr;

                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    spdlog::info(
                        "A packet of length {} was received",
                        event.packet->dataLength
                    );

                    /* Clean up the packet now that we're done using it. */
                    enet_packet_destroy(event.packet);

                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    spdlog::info("{} disconnected.\n", event.peer->data);

                    /* Reset the peer's client information. */

                    event.peer->data = NULL;
            }
        }

        void send() override {
            ENetPacket* packet = enet_packet_create(
                "packet data",
                strlen("packet data") + 1,
                ENET_PACKET_FLAG_RELIABLE
            );

            if (0 != enet_peer_send(server_peer_, 0, packet)) {
                spdlog::warn("Failed to send packet");
            }
        }

    private:
        ENetHost* host_;
        ENetPeer* server_peer_;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<INetworkServer> create_server() {
        return std::make_unique<NetworkServer>();
    }

    std::unique_ptr<INetworkClient> create_client() {
        return std::make_unique<NetworkClient>();
    }

}  // namespace mirinae
