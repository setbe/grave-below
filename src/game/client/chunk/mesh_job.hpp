#pragma once

    inline bool InitChunkJobSlots() noexcept {
        const io::u32 max_slots = static_cast<io::u32>(chunk_job_slots_active);
        if (max_slots == 0u || !chunk_job_slots || !chunk_job_tasks) return false;
        for (io::u32 i = 0; i < max_slots; ++i) {
            ChunkMeshJobSlot& slot = chunk_job_slots[i];
            slot.state.store(static_cast<io::u32>(ChunkJobState::Free));
            slot.vertices.clear();
            slot.indices.clear();
            slot.vertices.reserve(CHUNK_JOB_VERTEX_CAP);
            slot.indices.reserve(CHUNK_JOB_INDEX_CAP);
            if (!slot.vertices.resize(CHUNK_JOB_VERTEX_CAP))
                return false;
            if (!slot.indices.resize(CHUNK_JOB_INDEX_CAP))
                return false;
            slot.world_chunk_index = io::npos;
            slot.chunk_version = 0;
            slot.vertex_count = 0;
            slot.index_count = 0;
            slot.failed = 0;
            slot.render_mask = 0u;
            slot.stats = {};
            chunk_job_tasks[i].owner = this;
            chunk_job_tasks[i].slot_index = i;
        }
        return true;
    }

    inline void CollectChunkJobs() noexcept {
        if (!chunk_world_ready || !chunk_job_slots) return;
        for (io::u32 i = 0; i < chunk_job_slots_active; ++i) {
            ChunkMeshJobSlot& slot = chunk_job_slots[i];
            if (slot.state.load() != static_cast<io::u32>(ChunkJobState::Done)) continue;
            const io::usize world_index = slot.world_chunk_index;
            if (world_index >= voxel_world.chunks.size() || world_index >= chunk_meshes.size()) {
                slot.state.store(static_cast<io::u32>(ChunkJobState::Free));
                continue;
            }

            ge::voxel::ChunkData& chunk = voxel_world.chunks[world_index];
            ChunkRenderMesh& gpu = chunk_meshes[world_index];

            if (slot.failed || slot.chunk_version != chunk.version) {
                gpu.queued = false;
                slot.state.store(static_cast<io::u32>(ChunkJobState::Free));
                ++chunk_jobs_failed;
                continue;
            }

            gpu.vao.bind();
            gpu.vbo.bind();
            gpu.vbo.data(slot.vertices.data(),
                         static_cast<io::usize>(slot.vertex_count) * sizeof(ge::voxel::MeshVertex),
                         gl::BufferUsage::StaticDraw);
            gpu.ebo.bind();
            gpu.ebo.data(slot.indices.data(),
                         static_cast<io::usize>(slot.index_count) * sizeof(io::u32),
                         gl::BufferUsage::StaticDraw);
            gl::VertexAttribPointer(0, 3, gl::DrawElementsType::Float, false,
                                    static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                    reinterpret_cast<void*>(0));
            gl::EnableVertexAttribArray(0);
            gl::VertexAttribPointer(1, 3, gl::DrawElementsType::Float, false,
                                    static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                    reinterpret_cast<void*>(sizeof(float) * 3u));
            gl::EnableVertexAttribArray(1);
            gl::VertexAttribPointer(2, 2, gl::DrawElementsType::Float, false,
                                    static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                    reinterpret_cast<void*>(sizeof(float) * 6u));
            gl::EnableVertexAttribArray(2);
            gl::VertexAttribPointer(3, 4, gl::DrawElementsType::Float, false,
                                    static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                    reinterpret_cast<void*>(sizeof(float) * 8u));
            gl::EnableVertexAttribArray(3);
            gl::VertexAttribPointer(4, 1, gl::DrawElementsType::UnsignedByte, true,
                                    static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                    reinterpret_cast<void*>(sizeof(float) * 12u + sizeof(io::u16) + sizeof(io::u8)));
            gl::EnableVertexAttribArray(4);
            gl::VertexAttribIPointer(5, 1, gl::DrawElementsType::UnsignedShort,
                                     static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                     reinterpret_cast<void*>(sizeof(float) * 12u));
            gl::EnableVertexAttribArray(5);
            gpu.index_count = slot.index_count;
            gpu.built_version = slot.chunk_version;
            gpu.render_mask = slot.render_mask;
            gpu.uploaded = true;
            gpu.queued = false;
            chunk.dirty_mesh = false;
            if (chunk.dirty_neighbors) chunk.dirty_neighbors = false;
            chunk_faces_last = slot.stats.faces_emitted;
            chunk_vertices_last = slot.vertex_count;
            ++chunk_jobs_completed;
            slot.state.store(static_cast<io::u32>(ChunkJobState::Free));
        }
    }

    inline void WaitChunkJobsIdle() noexcept {
        if (!chunk_job_slots) return;
        bool pending = true;
        while (pending) {
            pending = false;
            for (io::u32 i = 0; i < chunk_job_slots_active; ++i) {
                const io::u32 s = chunk_job_slots[i].state.load();
                if (s == static_cast<io::u32>(ChunkJobState::Queued) ||
                    s == static_cast<io::u32>(ChunkJobState::Building) ||
                    s == static_cast<io::u32>(ChunkJobState::Done)) {
                    pending = true;
                    break;
                }
            }
            if (!pending) break;
            CollectChunkJobs();
            io::sleep_ms(1);
        }
    }

    inline io::i32 FindFreeChunkJobSlot() const noexcept {
        if (!chunk_job_slots) return -1;
        for (io::u32 i = 0; i < chunk_job_slots_active; ++i)
            if (chunk_job_slots[i].state.load() == static_cast<io::u32>(ChunkJobState::Free))
                return static_cast<io::i32>(i);
        return -1;
    }

    inline void RunChunkBuildTask(io::u32 slot_index) noexcept {
        if (!chunk_job_slots) return;
        if (slot_index >= chunk_job_slots_active) return;
        ChunkMeshJobSlot& slot = chunk_job_slots[slot_index];
        slot.state.store(static_cast<io::u32>(ChunkJobState::Building));

        if (!chunk_world_ready || slot.world_chunk_index >= voxel_world.chunks.size()) {
            slot.failed = 1;
            slot.vertex_count = 0;
            slot.index_count = 0;
            slot.render_mask = 0u;
            slot.state.store(static_cast<io::u32>(ChunkJobState::Done));
            return;
        }

        const ge::voxel::ChunkData& chunk = voxel_world.chunks[slot.world_chunk_index];
        ge::voxel::MeshBuffers mesh{};
        mesh.vertices = slot.vertices.data();
        mesh.vertex_capacity = static_cast<io::u32>(slot.vertices.size());
        mesh.indices = slot.indices.data();
        mesh.index_capacity = static_cast<io::u32>(slot.indices.size());
        mesh.reset_counts();

        ge::voxel::MeshBuildOptions opt{};
        opt.perception = perception_level;
        opt.include_transparent = true;
        slot.stats = {};

        bool ok = ge::voxel::build_chunk_mesh(&voxel_world, chunk, mesh, opt, &slot.stats,
                                              slot.rows_by_block_scratch, slot.rects_scratch);
        if (!ok) {
            // Keep the chunk renderable even if transparent-heavy geometry exceeds buffers.
            mesh.reset_counts();
            slot.stats = {};
            opt.include_transparent = false;
            ok = ge::voxel::build_chunk_mesh(&voxel_world, chunk, mesh, opt, &slot.stats,
                                             slot.rows_by_block_scratch, slot.rects_scratch);
        }
        slot.vertex_count = mesh.vertex_count;
        slot.index_count = mesh.index_count;
        slot.failed = ok ? 0u : 1u;
        slot.render_mask = 0u;

        if (slot.vertex_count > slot.vertices.size() || slot.index_count > slot.indices.size()) {
            slot.vertex_count = 0;
            slot.index_count = 0;
            slot.failed = 1u;
            slot.render_mask = 0u;
        }

        if (ok && !slot.failed) {
            const BlockFaceUv* uv_table = block_face_uv;
            for (io::u32 i = 0; i < slot.vertex_count; ++i) {
                ge::voxel::MeshVertex& v = slot.vertices[i];
                const io::u16 bid = v.block_id < ge::voxel::BLOCK_COUNT ? v.block_id : 0u;
                const io::u8 face = v.face < FACE_INDEX_COUNT ? v.face : 0u;
                const io::usize uv_i = static_cast<io::usize>(bid) * static_cast<io::usize>(FACE_INDEX_COUNT) + static_cast<io::usize>(face);
                const BlockFaceUv uv = uv_table ? uv_table[uv_i] : BlockFaceUv{};
                if (!uv.valid) {
                    v.atlas_u0 = 0.f;
                    v.atlas_v0 = 0.f;
                    v.atlas_u1 = 1.f;
                    v.atlas_v1 = 1.f;
                    continue;
                }
                v.atlas_u0 = uv.u0;
                v.atlas_v0 = uv.v0;
                v.atlas_u1 = uv.u1;
                v.atlas_v1 = uv.v1;

                if (ge::voxel::is_liquid(static_cast<ge::voxel::BlockId>(bid)))
                    slot.render_mask |= CHUNK_RENDER_MASK_LIQUID;
                else
                    slot.render_mask |= CHUNK_RENDER_MASK_SOLID;
            }
        }

        slot.state.store(static_cast<io::u32>(ChunkJobState::Done));
    }

    inline void ScheduleChunkJobs(const lm::vec3& camera_pos) noexcept {
        if (!chunk_world_ready) return;
        (void)camera_pos;
        io::u32 scheduled = 0u;
        io::u32 schedule_budget = 2u;
        if (render_distance_chunks <= 6u) schedule_budget = 4u;
        else if (render_distance_chunks <= 12u) schedule_budget = 3u;

        for (;;) {
            if (scheduled >= schedule_budget) return;
            const io::i32 slot_index = FindFreeChunkJobSlot();
            if (slot_index < 0) return;

            io::usize best_index = io::npos;
            const io::usize total = voxel_world.chunks.size();
            if (total == 0u) return;
            if (chunk_schedule_cursor >= total)
                chunk_schedule_cursor = 0u;
            for (io::usize probe = 0; probe < total; ++probe) {
                const io::usize i = (chunk_schedule_cursor + probe) % total;
                ge::voxel::ChunkData& chunk = voxel_world.chunks[i];
                ChunkRenderMesh& gpu = chunk_meshes[i];
                if (!chunk.generated) continue;
                if (!chunk.dirty_mesh) continue;
                if (gpu.queued) continue;
                if (gpu.uploaded && gpu.built_version == chunk.version) {
                    chunk.dirty_mesh = false;
                    continue;
                }
                best_index = i;
                chunk_schedule_cursor = (i + 1u >= total) ? 0u : (i + 1u);
                break;
            }

            if (best_index == io::npos) return;

            ge::voxel::ChunkData& chunk = voxel_world.chunks[best_index];
            ChunkRenderMesh& gpu = chunk_meshes[best_index];
            ChunkMeshJobSlot& slot = chunk_job_slots[static_cast<io::u32>(slot_index)];
            slot.world_chunk_index = best_index;
            slot.chunk_version = chunk.version;
            slot.vertex_count = 0;
            slot.index_count = 0;
            slot.failed = 0;
            slot.render_mask = 0u;
            slot.stats = {};
            slot.state.store(static_cast<io::u32>(ChunkJobState::Queued));

            gpu.queued = true;
            if (!worker_pool || !worker_pool->submit(&Window::ChunkBuildTaskEntry, &chunk_job_tasks[slot_index])) {
                gpu.queued = false;
                slot.state.store(static_cast<io::u32>(ChunkJobState::Free));
                return;
            }

            ++chunk_jobs_submitted;
            ++scheduled;
        }
    }

    inline void UpdateChunkPipeline(const lm::vec3& camera_pos) noexcept {
        CollectChunkJobs();
        UpdateChunkStreaming();
        QueueMissingChunkRequests();
        PumpIncomingChunks();
        ScheduleChunkJobs(camera_pos);
    }
