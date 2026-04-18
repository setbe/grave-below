#pragma once

#include "server_app.hpp"

constexpr io::u32 WORKERS = 1;
constexpr io::u32 DISTANCE_CHUNKS = 10;
constexpr io::u32 HOT_CHUNKS = 2;
constexpr io::u32 DAY_TIME_MULTIPLY = 20;

static io::u32 not_zero_config_var(io::i32 value_from_config,
                                   io::u32 default_value,
                                   io::u32 min,
                                   io::u32 max) noexcept {
    return 
          (static_cast<io::u32>(value_from_config) < min) ? default_value
        : (static_cast<io::u32>(value_from_config) > max) ? default_value
        : static_cast<io::u32>(value_from_config);
}

int main() {
    using namespace io;
    ge::ServerTextConfig cfg{};
    if (!ge::ensure_server_text_config(cfg)) {
        out << "[server] ensure_server_text_config failed\n";
        return -1;
    }

    u32 worker_threads  = not_zero_config_var(cfg.job_threads, WORKERS, 1, static_cast<u32>(Thread::max_workers()));
    u32 distance_chunks = not_zero_config_var(cfg.distance_chunks, DISTANCE_CHUNKS, 1, 32);
    u32 hot_chunks      = not_zero_config_var(cfg.hot_chunks, HOT_CHUNKS, 1, 5);
    u32 day_time_multiply = not_zero_config_var(cfg.day_time_multiply, DAY_TIME_MULTIPLY, 1, 240);

    const u32 hot_side = hot_chunks * 2u + 1u;
    const u32 hot_per_peer = hot_side * hot_side * hot_side;
    const u32 hot_total_max = hot_per_peer * static_cast<u32>(MAX_PEERS);
    const u32 chunk_kb = static_cast<u32>((sizeof(ge::voxel::ChunkData) + 1023u) / 1024u);
    const u32 hot_ram_mb_max = (hot_total_max * chunk_kb + 1023u) / 1024u;
    out << "+-------------------------------------------------------------+\n";
    out << "| HOST RAM BUDGET (MAX)                                      |\n";
    out << "+-------------------------------------------------------------+\n";
    out << "| " << hot_ram_mb_max << " RAM MB MAX - CAN BE USED DURING HOSTING.\n";
    out << "+-------------------------------------------------------------+\n";

    ServerApp app{};
    app.boot_ms = monotonic_ms();
    if (!app.init(worker_threads, distance_chunks, hot_chunks, day_time_multiply)) {
        out << "[server] init failed\n";
        return -2;
    }

    if (!app.udp.open(Protocol::UDP)) {
        out << "[server] udp.open failed\n";
        return -3;
    }

    Endpoint bind_ep{};
    bind_ep.addr_be = 0u;
    bind_ep.port_be = h2ns(ge::net::GAME_UDP_PORT);
    if (!app.udp.bind(bind_ep)) {
        out << "[server] udp.bind failed on port " << ge::net::GAME_UDP_PORT << '\n';
        return -4;
    }
    (void)app.udp.set_blocking(false);

    if (!app.loop.init(/*is_server=*/true)) {
        out << "[server] loop.init failed\n";
        return -5;
    }

    UdpCallbacks cb{};
    cb.ud = &app;
    cb.on_packet = &ServerApp::on_packet;
    cb.on_established = &ServerApp::on_established;
    cb.on_drop = &ServerApp::on_drop;
    cb.on_disconnect = &ServerApp::on_disconnect;
    cb.on_tick = &ServerApp::on_tick;

    out << "[server] started\n";
    out << "  +----------------------+----------------------+\n";
    out << "  | udp_port             | " << ge::net::GAME_UDP_PORT << '\n';
    out << "  | config               | " << ge::server_text_config_state_to_string(cfg.state) << '\n';
    out << "  | job_threads(cfg)     | " << cfg.job_threads << '\n';
    out << "  | workers(active)      | " << worker_threads << '\n';
    out << "  | max_threads(hw)      | " << Thread::max_workers() << '\n';
    out << "  | distance_chunks      | " << distance_chunks << '\n';
    out << "  | hot_chunks           | " << hot_chunks << '\n';
    out << "  | day-time-multiply    | " << day_time_multiply << '\n';
    out << "  +----------------------+----------------------+\n";

    app.loop.run_udp(app.udp, cb, app.recv_buf, static_cast<int>(ServerApp::RECV_BUF_CAP));

    out << "[server] stopped\n";
    out << "  jobs(sub/ok/inflight): " << app.stats.jobs_submitted << "/" << app.stats.jobs_completed << "/" << app.stats.jobs_inflight << '\n';
    out << "  send(ok/fail): " << app.stats.send_ok << "/" << app.stats.send_fail << "  drop: " << app.stats.dropped << '\n';
    out << "  edit(ok/reject): " << app.stats.block_edits_ok << "/" << app.stats.block_edits_reject << '\n'
        << "  pos(pkt/reject): " << app.stats.pos_packets << "/" << app.stats.pos_reject << '\n';
    return 0;
}

