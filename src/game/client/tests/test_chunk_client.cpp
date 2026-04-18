#define IO_IMPLEMENTATION
#include "hi/hi/socket.hpp"

#include "../../../engine/voxel/mesh_builder.hpp"
#include "../../shared/net/protocol.hpp"
#include "../../shared/world/chunk_stream.hpp"

namespace {
    template<class T>
    static bool ReadPayloadExact(io::byte_view payload, T& out) noexcept {
        if (payload.size() != sizeof(T)) return false;
        io::u8* dst = reinterpret_cast<io::u8*>(&out);
        for (io::usize i = 0; i < sizeof(T); ++i) dst[i] = payload[i];
        return true;
    }

    struct ClientApp {
        io::Socket udp{};
        io::EventLoop<1200, 4096> loop{};
        io::u8 recv_buf[2048]{};

        io::Endpoint server{};
        ge::net::ChunkAssembly assembly{};
        io::u32 session_id{};
        io::u32 request_id{ 1u };
        io::u64 boot_ms{};

        bool established{};
        bool request_sent{};
        bool pong_received{};
        bool chunk_received{};
        bool failed{};

        inline bool SendPing() noexcept {
            ge::net::C2S_Ping ping{};
            ping.client_ms_be = io::h2nl(static_cast<io::u32>(io::monotonic_ms() - boot_ms));
            return loop.send_to_peer(server, ge::net::PK_C2S_PING, io::UdpChan::Unreliable,
                                     io::byte_view{ reinterpret_cast<const io::u8*>(&ping), sizeof(ping) },
                                     io::monotonic_ms());
        }

        inline bool SendChunkRequest(io::i32 cx, io::i32 cy, io::i32 cz) noexcept {
            ge::net::ChunkRequest req{};
            req.request_id = request_id++;
            req.cx = cx;
            req.cy = cy;
            req.cz = cz;
            req.lod = 0u;

            ge::net::C2S_RequestChunk wire{};
            ge::net::encode_request(req, wire);
            return loop.send_to_peer(server, ge::net::PK_C2S_REQUEST_CHUNK, io::UdpChan::Reliable,
                                     io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                                     io::monotonic_ms());
        }

        static void OnPacket(void* ud, io::Endpoint from, io::u8 type, io::UdpChan chan, io::byte_view payload) noexcept {
            (void)from;
            (void)chan;
            ClientApp* app = reinterpret_cast<ClientApp*>(ud);
            if (!app) return;

            if (type == ge::net::PK_S2C_PONG) {
                ge::net::S2C_Pong pong{};
                if (!ReadPayloadExact(payload, pong)) return;
                app->pong_received = true;
                const io::u32 client_ms = io::n2hl(pong.client_ms_be);
                const io::u32 server_ms = io::n2hl(pong.server_uptime_ms_be);
                io::out << "[client] pong client_ms=" << client_ms
                        << " server_uptime_ms=" << server_ms << '\n';
                return;
            }

            if (type == ge::net::PK_S2C_CHUNK_BEGIN) {
                ge::net::S2C_ChunkBegin wire{};
                if (!ReadPayloadExact(payload, wire)) return;
                const ge::net::ChunkBegin begin = ge::net::decode_chunk_begin(wire);
                const bool ok = app->assembly.begin(begin);
                io::out << "[client] chunk_begin req=" << begin.request_id
                        << " coord(" << begin.coord.x << "," << begin.coord.y << "," << begin.coord.z
                        << ") parts=" << begin.part_count
                        << " total=" << begin.total_bytes
                        << " ok=" << ok << '\n';
                if (!ok) {
                    app->failed = true;
                    app->loop.stop();
                }
                return;
            }

            if (type == ge::net::PK_S2C_CHUNK_PART) {
                if (payload.size() < sizeof(ge::net::S2C_ChunkPartHeader)) return;

                ge::net::S2C_ChunkPartHeader wire{};
                for (io::usize i = 0; i < sizeof(wire); ++i)
                    reinterpret_cast<io::u8*>(&wire)[i] = payload[i];
                const ge::net::ChunkPart part = ge::net::decode_chunk_part(wire);
                const io::byte_view bytes = payload.slice(sizeof(wire), payload.size() - sizeof(wire));
                const bool ok = app->assembly.add_part(part, bytes);
                if (!ok) {
                    io::out << "[client] chunk_part failed idx=" << part.part_index
                            << " size=" << part.part_size << '\n';
                    app->failed = true;
                    app->loop.stop();
                }
                return;
            }

            if (type == ge::net::PK_S2C_CHUNK_END) {
                ge::net::S2C_ChunkEnd wire{};
                if (!ReadPayloadExact(payload, wire)) return;
                const ge::net::ChunkEnd end = ge::net::decode_chunk_end(wire);
                const bool ok = app->assembly.end(end);
                io::out << "[client] chunk_end req=" << end.request_id
                        << " hash=" << end.hash
                        << " complete=" << app->assembly.is_complete()
                        << " ok=" << ok << '\n';
                if (!ok) {
                    app->failed = true;
                    app->loop.stop();
                    return;
                }

                app->chunk_received = true;
                io::out << "[client] chunk non_air=" << app->assembly.chunk.non_air_count << '\n';
                app->loop.stop();
            }
        }

        static void OnEstablished(void* ud, io::Endpoint peer, io::u32 session_id) noexcept {
            ClientApp* app = reinterpret_cast<ClientApp*>(ud);
            if (!app) return;

            app->established = true;
            app->session_id = session_id;
            app->server = peer;

            io::out << "[client] established session=" << session_id
                    << " server_port=" << io::n2hs(peer.port_be) << '\n';

            if (!app->SendPing()) {
                app->failed = true;
                app->loop.stop();
                return;
            }

            app->request_sent = app->SendChunkRequest(0, 0, 0);
            if (!app->request_sent) {
                app->failed = true;
                app->loop.stop();
            }
        }

        static void OnDrop(void* ud, io::Endpoint from, io::Error err, io::DropReason why) noexcept {
            ClientApp* app = reinterpret_cast<ClientApp*>(ud);
#ifdef _DEBUG
            io::out << "[client] drop from port=" << io::n2hs(from.port_be)
                    << " err=" << err
                    << " reason=" << io::drop_reason_str(why) << '\n';
#else
            (void)from;
            (void)err;
            (void)why;
#endif
            if (!app) return;
            if (!app->established) {
                app->failed = true;
                app->loop.stop();
            }
        }

        static void OnDisconnect(void* ud, io::Endpoint peer, io::u32 session_id, io::DisconnectReason why) noexcept {
            ClientApp* app = reinterpret_cast<ClientApp*>(ud);
            io::out << "[client] disconnect session=" << session_id
                    << " peer_port=" << io::n2hs(peer.port_be)
                    << " reason=" << io::disconnect_reason_str(why) << '\n';
            if (!app) return;
            if (!app->chunk_received) {
                app->failed = true;
                app->loop.stop();
            }
        }
    };
}

int main() {
    ClientApp app{};
    app.boot_ms = io::monotonic_ms();

    if (!app.udp.open(io::Protocol::UDP)) {
        io::out << "[client] udp.open failed\n";
        return -1;
    }

    io::Endpoint local{};
    local.addr_be = 0;
    local.port_be = io::h2ns(0u);
    if (!app.udp.bind(local)) {
        io::out << "[client] udp.bind failed\n";
        return -2;
    }
    (void)app.udp.set_blocking(false);

    if (!app.loop.init(/*is_server=*/false)) {
        io::out << "[client] loop.init failed\n";
        return -3;
    }

    app.server.addr_be = io::IP::from_string("127.0.0.1");
    app.server.port_be = io::h2ns(ge::net::GAME_UDP_PORT);
    if (app.server.addr_be == 0) {
        io::out << "[client] invalid server ip\n";
        return -4;
    }

    if (!app.loop.get_peer_create(app.server)) {
        io::out << "[client] get_peer_create failed\n";
        return -5;
    }

    if (!app.loop.start_client_handshake(app.server, io::DEFAULT_MTU, io::FEATURE_COOKIE, io::monotonic_ms())) {
        io::out << "[client] start_client_handshake failed\n";
        return -6;
    }

    io::UdpCallbacks cb{};
    cb.ud = &app;
    cb.on_packet = &ClientApp::OnPacket;
    cb.on_established = &ClientApp::OnEstablished;
    cb.on_drop = &ClientApp::OnDrop;
    cb.on_disconnect = &ClientApp::OnDisconnect;

    io::out << "[client] waiting for server 127.0.0.1:" << ge::net::GAME_UDP_PORT << '\n';
    app.loop.run_udp(app.udp, cb, app.recv_buf, static_cast<int>(sizeof(app.recv_buf)));

    if (app.failed) return -7;
    if (!app.established) return -8;
    if (!app.request_sent) return -9;
    if (!app.chunk_received) return -10;

    io::out << "test_chunk_client: ok\n";
    return 0;
}
