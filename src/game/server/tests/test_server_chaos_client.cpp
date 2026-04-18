#define IO_IMPLEMENTATION
#include "hi/hi/socket.hpp"
#include "../../shared/net/protocol.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
    static constexpr io::u32 MAX_PACKET_BYTES = 1600u;

    struct Config {
        std::string host = "127.0.0.1";
        io::u16 port = ge::net::GAME_UDP_PORT;
        io::u32 duration_sec = 120u;
        io::u32 chaos_workers = 4u;
        io::u32 chaos_burst = 96u;
        io::u32 chaos_sleep_ms = 1u;
        io::u32 stable_ping_ms = 200u;
        io::u32 seed = 0xC0FFEEu;
    };

    struct Metrics {
        std::atomic<io::u64> chaos_sent{ 0u };
        std::atomic<io::u64> chaos_send_fail{ 0u };
        std::atomic<io::u64> stable_handshake_ok{ 0u };
        std::atomic<io::u64> stable_handshake_drop{ 0u };
        std::atomic<io::u64> stable_disconnect{ 0u };
        std::atomic<io::u64> stable_pong_rx{ 0u };
        std::atomic<io::u64> stable_send_ok{ 0u };
        std::atomic<io::u64> stable_send_fail{ 0u };
        std::atomic<io::u64> stable_runtime_fail{ 0u };
    };

    template<typename... Args>
    static inline void log_line(const char* fmt, Args... args) {
        std::printf(fmt, args...);
        std::fflush(stdout);
    }

    struct XorShift32 {
        io::u32 s = 0x12345678u;

        IO_NODISCARD inline io::u32 next_u32() noexcept {
            io::u32 x = s;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            s = (x == 0u) ? 0xA341316Cu : x;
            return s;
        }

        IO_NODISCARD inline io::u32 range(io::u32 min_v, io::u32 max_v) noexcept {
            if (max_v <= min_v) return min_v;
            const io::u32 span = max_v - min_v + 1u;
            return min_v + (next_u32() % span);
        }
    };

    IO_NODISCARD static bool parse_u32(const char* s, io::u32& out) noexcept {
        if (!s || !*s) return false;
        char* end = nullptr;
        const unsigned long v = std::strtoul(s, &end, 10);
        if (!end || *end != '\0') return false;
        if (v > 0xFFFFFFFFul) return false;
        out = static_cast<io::u32>(v);
        return true;
    }

    IO_NODISCARD static bool parse_u16(const char* s, io::u16& out) noexcept {
        io::u32 v = 0u;
        if (!parse_u32(s, v)) return false;
        if (v > 65535u) return false;
        out = static_cast<io::u16>(v);
        return true;
    }

    static void print_usage() {
        log_line(
            "test_server_chaos_client usage:\n"
            "  --host <ip>            (default 127.0.0.1)\n"
            "  --port <u16>           (default 25565)\n"
            "  --seconds <u32>        (default 120)\n"
            "  --chaos-workers <u32>  (default 4)\n"
            "  --chaos-burst <u32>    (default 96 packets per loop)\n"
            "  --chaos-sleep-ms <u32> (default 1)\n"
            "  --stable-ping-ms <u32> (default 200)\n"
            "  --seed <u32>           (default 0xC0FFEE)\n");
    }

    IO_NODISCARD static bool parse_args(int argc, char** argv, Config& cfg) noexcept {
        for (int i = 1; i < argc; ++i) {
            const char* arg = argv[i];
            if (!arg) continue;
            if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
                print_usage();
                return false;
            }
            if (i + 1 >= argc) {
                log_line("[chaos] missing value for '%s'\n", arg);
                return false;
            }
            const char* val = argv[++i];
            if (std::strcmp(arg, "--host") == 0) {
                cfg.host = val;
                continue;
            }
            if (std::strcmp(arg, "--port") == 0) {
                io::u16 v = 0u;
                if (!parse_u16(val, v)) return false;
                cfg.port = v;
                continue;
            }
            if (std::strcmp(arg, "--seconds") == 0) {
                if (!parse_u32(val, cfg.duration_sec)) return false;
                continue;
            }
            if (std::strcmp(arg, "--chaos-workers") == 0) {
                if (!parse_u32(val, cfg.chaos_workers)) return false;
                continue;
            }
            if (std::strcmp(arg, "--chaos-burst") == 0) {
                if (!parse_u32(val, cfg.chaos_burst)) return false;
                continue;
            }
            if (std::strcmp(arg, "--chaos-sleep-ms") == 0) {
                if (!parse_u32(val, cfg.chaos_sleep_ms)) return false;
                continue;
            }
            if (std::strcmp(arg, "--stable-ping-ms") == 0) {
                if (!parse_u32(val, cfg.stable_ping_ms)) return false;
                continue;
            }
            if (std::strcmp(arg, "--seed") == 0) {
                if (!parse_u32(val, cfg.seed)) return false;
                continue;
            }
            log_line("[chaos] unknown argument '%s'\n", arg);
            return false;
        }
        if (cfg.chaos_workers == 0u) cfg.chaos_workers = 1u;
        return true;
    }

    static void fill_random_bytes(XorShift32& rng, io::u8* dst, io::u32 len) noexcept {
        for (io::u32 i = 0u; i < len; ++i)
            dst[i] = static_cast<io::u8>(rng.next_u32() & 0xFFu);
    }

    IO_NODISCARD static io::u32 build_udp_packet(io::u8* out,
                                                 io::u32 cap,
                                                 io::u8 type,
                                                 io::UdpChan chan,
                                                 io::u32 seq,
                                                 io::u32 ack,
                                                 io::u64 ack_bits,
                                                 const io::u8* payload,
                                                 io::u16 payload_len_header,
                                                 io::u32 payload_len_actual,
                                                 io::u32 magic = io::UDP_MAGIC,
                                                 io::u16 version = io::UDP_VERSION) noexcept {
        if (!out || cap < sizeof(io::UdpHeader)) return 0u;
        if (payload_len_actual > (cap - static_cast<io::u32>(sizeof(io::UdpHeader)))) return 0u;

        io::UdpHeader h{};
        h.magic = magic;
        h.version = version;
        h.chan = static_cast<io::u8>(chan);
        h.type = type;
        h.seq = seq;
        h.ack = ack;
        h.ack_bits = ack_bits;
        h.payload_len = payload_len_header;
        io::udp_header_host_to_wire(h);

        const io::u8* hs = reinterpret_cast<const io::u8*>(&h);
        for (io::u32 i = 0u; i < sizeof(io::UdpHeader); ++i) out[i] = hs[i];
        for (io::u32 i = 0u; i < payload_len_actual; ++i) out[sizeof(io::UdpHeader) + i] = payload ? payload[i] : 0u;
        return static_cast<io::u32>(sizeof(io::UdpHeader)) + payload_len_actual;
    }

    static void send_one_chaos_packet(io::Socket& sock,
                                      const io::Endpoint& server,
                                      XorShift32& rng,
                                      Metrics& metrics) noexcept {
        io::u8 buf[MAX_PACKET_BYTES]{};
        io::u8 payload[1400]{};
        io::u32 n = 0u;
        const io::u32 kind = rng.range(0u, 10u);
        const io::u32 random_payload_n = rng.range(1u, 1024u);
        fill_random_bytes(rng, payload, random_payload_n);

        if (kind == 0u) {
            // Pure random garbage.
            n = rng.range(1u, 1400u);
            fill_random_bytes(rng, buf, n);
        } else if (kind == 1u) {
            // Too-small pseudo datagram.
            n = rng.range(1u, 20u);
            fill_random_bytes(rng, buf, n);
        } else if (kind == 2u) {
            // Bad magic with otherwise valid layout.
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 ge::net::PK_C2S_PING, io::UdpChan::Unreliable,
                                 rng.next_u32(), rng.next_u32(), static_cast<io::u64>(rng.next_u32()),
                                 payload, static_cast<io::u16>(random_payload_n), random_payload_n,
                                 io::UDP_MAGIC ^ 0xA55AA55Au, io::UDP_VERSION);
        } else if (kind == 3u) {
            // Bad protocol version.
            const io::u16 bad_ver = static_cast<io::u16>(io::UDP_VERSION + 7u);
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 ge::net::PK_C2S_CHAT, io::UdpChan::Reliable,
                                 rng.next_u32(), rng.next_u32(), static_cast<io::u64>(rng.next_u32()),
                                 payload, static_cast<io::u16>(random_payload_n), random_payload_n,
                                 io::UDP_MAGIC, bad_ver);
        } else if (kind == 4u) {
            // Header says bigger payload than real bytes.
            const io::u16 declared = static_cast<io::u16>(random_payload_n + 64u);
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 ge::net::PK_C2S_REQUEST_CHUNK, io::UdpChan::Reliable,
                                 rng.next_u32(), rng.next_u32(), static_cast<io::u64>(rng.next_u32()),
                                 payload, declared, random_payload_n);
        } else if (kind == 5u) {
            // Header says smaller payload than actual bytes.
            io::u16 declared = static_cast<io::u16>(random_payload_n / 2u);
            if (declared == 0u) declared = 1u;
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 ge::net::PK_C2S_PLAYER_POSITION, io::UdpChan::Unreliable,
                                 rng.next_u32(), rng.next_u32(), static_cast<io::u64>(rng.next_u32()),
                                 payload, declared, random_payload_n);
        } else if (kind == 6u) {
            // Handshake HELLO with wrong payload length.
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 io::MSG_HELLO, io::UdpChan::Unreliable,
                                 rng.next_u32(), 0u, 0u,
                                 payload, 2u, 2u);
        } else if (kind == 7u) {
            // Handshake HELLO2 with random invalid cookie payload.
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 io::MSG_HELLO2, io::UdpChan::Reliable,
                                 rng.next_u32(), 0u, 0u,
                                 payload, static_cast<io::u16>(sizeof(io::msg_hello2)), 7u);
        } else if (kind == 8u) {
            // Valid HELLO storm to pressure handshake path.
            io::msg_hello h{};
            h.mtu = io::h2ns(io::DEFAULT_MTU);
            h.features = io::h2ns(io::FEATURE_COOKIE);
            h.client_nonce = io::h2nl(rng.next_u32() | 1u);
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 io::MSG_HELLO, io::UdpChan::Unreliable,
                                 rng.next_u32(), 0u, 0u,
                                 reinterpret_cast<const io::u8*>(&h),
                                 static_cast<io::u16>(sizeof(h)),
                                 static_cast<io::u32>(sizeof(h)));
        } else if (kind == 9u) {
            // Unknown message type with "valid" envelope.
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 250u, io::UdpChan::Reliable,
                                 rng.next_u32(), rng.next_u32(), static_cast<io::u64>(rng.next_u32()),
                                 payload, static_cast<io::u16>(random_payload_n), random_payload_n);
        } else {
            // Replay-like fixed sequence/ack.
            n = build_udp_packet(buf, MAX_PACKET_BYTES,
                                 ge::net::PK_C2S_PING, io::UdpChan::Reliable,
                                 42u, 41u, 0xFFFFFFFFFFFFFFFFull,
                                 payload, 4u, 4u);
        }

        if (n == 0u) return;
        const int sent = sock.send_to(server, io::byte_view{ buf, n });
        if (sent == static_cast<int>(n)) metrics.chaos_sent.fetch_add(1u, std::memory_order_relaxed);
        else metrics.chaos_send_fail.fetch_add(1u, std::memory_order_relaxed);
    }

    static void run_chaos_worker(io::u32 worker_id,
                                 const Config cfg,
                                 const io::Endpoint server,
                                 std::atomic<bool>& stop_flag,
                                 Metrics& metrics) noexcept {
        io::Socket sock{};
        if (!sock.open(io::Protocol::UDP)) {
            metrics.stable_runtime_fail.fetch_add(1u, std::memory_order_relaxed);
            return;
        }
        io::Endpoint local{};
        local.addr_be = 0u;
        local.port_be = io::h2ns(0u);
        if (!sock.bind(local)) {
            metrics.stable_runtime_fail.fetch_add(1u, std::memory_order_relaxed);
            return;
        }
        (void)sock.set_blocking(false);

        XorShift32 rng{};
        rng.s = cfg.seed ^ (0x9E3779B9u * (worker_id + 1u));
        if (rng.s == 0u) rng.s = 0xCAFEBABEu ^ worker_id;

        while (!stop_flag.load(std::memory_order_relaxed)) {
            for (io::u32 i = 0u; i < cfg.chaos_burst; ++i)
                send_one_chaos_packet(sock, server, rng, metrics);
            if (cfg.chaos_sleep_ms > 0u)
                io::sleep_ms(static_cast<int>(cfg.chaos_sleep_ms));
        }
    }

    struct StableClient {
        io::Socket udp{};
        io::EventLoop<1200, 4096> loop{};
        io::u8 recv_buf[4096]{};
        io::Endpoint server{};
        std::atomic<bool>* stop_flag = nullptr;
        Metrics* metrics = nullptr;
        io::u64 start_ms = 0u;
        io::u64 next_ping_ms = 0u;
        io::u64 next_retry_ms = 0u;
        io::u32 ping_interval_ms = 200u;
        bool established = false;

        IO_NODISCARD bool send_ping(io::u64 now_ms) noexcept {
            ge::net::C2S_Ping ping{};
            ping.client_ms_be = io::h2nl(static_cast<io::u32>(now_ms - start_ms));
            const bool ok = loop.send_to_peer(
                server, ge::net::PK_C2S_PING, io::UdpChan::Unreliable,
                io::byte_view{ reinterpret_cast<const io::u8*>(&ping), sizeof(ping) },
                now_ms);
            if (metrics) {
                if (ok) metrics->stable_send_ok.fetch_add(1u, std::memory_order_relaxed);
                else metrics->stable_send_fail.fetch_add(1u, std::memory_order_relaxed);
            }
            return ok;
        }

        static void on_packet(void* ud, io::Endpoint, io::u8 type, io::UdpChan, io::byte_view payload) noexcept {
            StableClient* self = reinterpret_cast<StableClient*>(ud);
            if (!self || !self->metrics) return;
            if (type == ge::net::PK_S2C_PONG && payload.size() == sizeof(ge::net::S2C_Pong))
                self->metrics->stable_pong_rx.fetch_add(1u, std::memory_order_relaxed);
        }

        static void on_established(void* ud, io::Endpoint, io::u32) noexcept {
            StableClient* self = reinterpret_cast<StableClient*>(ud);
            if (!self) return;
            self->established = true;
            if (self->metrics) self->metrics->stable_handshake_ok.fetch_add(1u, std::memory_order_relaxed);
            const io::u64 now_ms = io::monotonic_ms();
            self->next_ping_ms = now_ms;
            self->next_retry_ms = now_ms + 1000u;
        }

        static void on_drop(void* ud, io::Endpoint, io::Error, io::DropReason) noexcept {
            StableClient* self = reinterpret_cast<StableClient*>(ud);
            if (!self || !self->metrics) return;
            self->metrics->stable_handshake_drop.fetch_add(1u, std::memory_order_relaxed);
        }

        static void on_disconnect(void* ud, io::Endpoint, io::u32, io::DisconnectReason) noexcept {
            StableClient* self = reinterpret_cast<StableClient*>(ud);
            if (!self) return;
            self->established = false;
            if (self->metrics) self->metrics->stable_disconnect.fetch_add(1u, std::memory_order_relaxed);
            self->next_retry_ms = io::monotonic_ms() + 300u;
        }

        static void on_tick(void* ud, io::u64 now_ms) noexcept {
            StableClient* self = reinterpret_cast<StableClient*>(ud);
            if (!self || !self->stop_flag) return;
            if (self->stop_flag->load(std::memory_order_relaxed)) {
                self->loop.stop();
                return;
            }
            if (self->established) {
                if (now_ms >= self->next_ping_ms) {
                    (void)self->send_ping(now_ms);
                    self->next_ping_ms = now_ms + self->ping_interval_ms;
                }
                return;
            }
            if (now_ms >= self->next_retry_ms) {
                (void)self->loop.start_client_handshake(self->server, io::DEFAULT_MTU, io::FEATURE_COOKIE, now_ms);
                self->next_retry_ms = now_ms + 1000u;
            }
        }

        IO_NODISCARD bool run(const Config& cfg, const io::Endpoint& endpoint,
                              std::atomic<bool>& stop, Metrics& out_metrics) noexcept {
            server = endpoint;
            stop_flag = &stop;
            metrics = &out_metrics;
            established = false;
            start_ms = io::monotonic_ms();
            next_ping_ms = start_ms;
            next_retry_ms = start_ms + 100u;
            ping_interval_ms = (cfg.stable_ping_ms == 0u) ? 200u : cfg.stable_ping_ms;

            if (!udp.open(io::Protocol::UDP)) return false;
            io::Endpoint local{};
            local.addr_be = 0u;
            local.port_be = io::h2ns(0u);
            if (!udp.bind(local)) return false;
            (void)udp.set_blocking(false);

            if (!loop.init(/*is_server=*/false)) return false;
            if (!loop.get_peer_create(server)) return false;
            if (!loop.start_client_handshake(server, io::DEFAULT_MTU, io::FEATURE_COOKIE, io::monotonic_ms()))
                return false;

            io::UdpCallbacks cb{};
            cb.ud = this;
            cb.on_packet = &StableClient::on_packet;
            cb.on_established = &StableClient::on_established;
            cb.on_drop = &StableClient::on_drop;
            cb.on_disconnect = &StableClient::on_disconnect;
            cb.on_tick = &StableClient::on_tick;

            (void)cfg;
            loop.run_udp(udp, cb, recv_buf, static_cast<int>(sizeof(recv_buf)));
            return true;
        }
    };
}

int main(int argc, char** argv) {
    Config cfg{};
    if (!parse_args(argc, argv, cfg)) {
        if (argc > 1) return 2;
        return 0;
    }

    io::Endpoint server{};
    server.addr_be = io::IP::from_string(cfg.host.c_str());
    server.port_be = io::h2ns(cfg.port);
    if (server.addr_be == 0u) {
        log_line("[chaos] invalid --host '%s'\n", cfg.host.c_str());
        return 3;
    }

    log_line("[chaos] target=%s:%u duration=%us workers=%u burst=%u sleep_ms=%u seed=%u\n",
                cfg.host.c_str(),
                static_cast<unsigned>(cfg.port),
                static_cast<unsigned>(cfg.duration_sec),
                static_cast<unsigned>(cfg.chaos_workers),
                static_cast<unsigned>(cfg.chaos_burst),
                static_cast<unsigned>(cfg.chaos_sleep_ms),
                static_cast<unsigned>(cfg.seed));

    Metrics metrics{};
    std::atomic<bool> stop_flag{ false };

    StableClient stable{};
    std::thread stable_thread([&]() {
        if (!stable.run(cfg, server, stop_flag, metrics))
            metrics.stable_runtime_fail.fetch_add(1u, std::memory_order_relaxed);
    });

    std::vector<std::thread> workers;
    workers.reserve(cfg.chaos_workers);
    for (io::u32 i = 0u; i < cfg.chaos_workers; ++i) {
        workers.emplace_back([&, i]() {
            run_chaos_worker(i, cfg, server, stop_flag, metrics);
        });
    }

    io::u64 last_chaos = 0u;
    io::u64 last_pong = 0u;
    io::u64 last_hs_ok = 0u;
    io::u64 last_hs_drop = 0u;
    for (io::u32 sec = 0u; sec < cfg.duration_sec; ++sec) {
        io::sleep_ms(1000);
        const io::u64 chaos_now = metrics.chaos_sent.load(std::memory_order_relaxed);
        const io::u64 pong_now = metrics.stable_pong_rx.load(std::memory_order_relaxed);
        const io::u64 hs_ok_now = metrics.stable_handshake_ok.load(std::memory_order_relaxed);
        const io::u64 hs_drop_now = metrics.stable_handshake_drop.load(std::memory_order_relaxed);
        const io::u64 send_fail_now = metrics.chaos_send_fail.load(std::memory_order_relaxed);
        const io::u64 stable_fail_now = metrics.stable_runtime_fail.load(std::memory_order_relaxed);
        const io::u64 disc_now = metrics.stable_disconnect.load(std::memory_order_relaxed);

        log_line("[chaos] t=%us sent=%llu (+%llu/s) send_fail=%llu hs_ok=%llu (+%llu) hs_drop=%llu (+%llu) pong=%llu (+%llu) disconnect=%llu stable_fail=%llu\n",
                    static_cast<unsigned>(sec + 1u),
                    static_cast<unsigned long long>(chaos_now),
                    static_cast<unsigned long long>(chaos_now - last_chaos),
                    static_cast<unsigned long long>(send_fail_now),
                    static_cast<unsigned long long>(hs_ok_now),
                    static_cast<unsigned long long>(hs_ok_now - last_hs_ok),
                    static_cast<unsigned long long>(hs_drop_now),
                    static_cast<unsigned long long>(hs_drop_now - last_hs_drop),
                    static_cast<unsigned long long>(pong_now),
                    static_cast<unsigned long long>(pong_now - last_pong),
                    static_cast<unsigned long long>(disc_now),
                    static_cast<unsigned long long>(stable_fail_now));

        last_chaos = chaos_now;
        last_pong = pong_now;
        last_hs_ok = hs_ok_now;
        last_hs_drop = hs_drop_now;
    }

    stop_flag.store(true, std::memory_order_relaxed);
    for (std::size_t i = 0; i < workers.size(); ++i)
        workers[i].join();
    stable_thread.join();

    const io::u64 total_hs_ok = metrics.stable_handshake_ok.load(std::memory_order_relaxed);
    const io::u64 total_pong = metrics.stable_pong_rx.load(std::memory_order_relaxed);
    const io::u64 runtime_fail = metrics.stable_runtime_fail.load(std::memory_order_relaxed);

    log_line("[chaos] done: sent=%llu fail=%llu hs_ok=%llu hs_drop=%llu pong=%llu disconnect=%llu\n",
                static_cast<unsigned long long>(metrics.chaos_sent.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(metrics.chaos_send_fail.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(total_hs_ok),
                static_cast<unsigned long long>(metrics.stable_handshake_drop.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(total_pong),
                static_cast<unsigned long long>(metrics.stable_disconnect.load(std::memory_order_relaxed)));

    if (runtime_fail != 0u) {
        log_line("[chaos] FAIL: stable client runtime init failed\n");
        return 10;
    }
    if (total_hs_ok == 0u) {
        log_line("[chaos] FAIL: no successful handshake observed\n");
        return 11;
    }
    if (total_pong == 0u) {
        log_line("[chaos] FAIL: no pong received from server\n");
        return 12;
    }

    log_line("[chaos] PASS: server stayed responsive under chaos load\n");
    return 0;
}
