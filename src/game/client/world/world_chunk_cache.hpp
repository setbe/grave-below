    IO_NODISCARD inline io::usize ComputeRequestVirtualCount() const noexcept {
        const io::i32 rx = static_cast<io::i32>(render_distance_chunks);
        const io::i32 rz = static_cast<io::i32>(render_distance_chunks);
        const io::i32 ry = RequestYRadiusChunks();
        const io::u32 sx = static_cast<io::u32>(rx * 2 + 1);
        const io::u32 sy = static_cast<io::u32>(ry * 2 + 1);
        const io::u32 sz = static_cast<io::u32>(rz * 2 + 1);
        io::usize plane = 0;
        if (!TryMulUsize(static_cast<io::usize>(sx), static_cast<io::usize>(sz), plane))
            return 0u;
        io::usize out = 0;
        if (!TryMulUsize(plane, static_cast<io::usize>(sy), out))
            return 0u;
        return out;
    }

    inline void InvalidateChunkRequestOffsets() noexcept {
        chunk_request_offsets_rx = -1;
        chunk_request_offsets_ry = -1;
        chunk_request_offsets_rz = -1;
        chunk_request_offsets.clear();
        chunk_request_scan_cursor = 0u;
        chunk_request_radius_phase = 0;
    }

    IO_NODISCARD inline bool RebuildChunkRequestOffsets() noexcept {
        const io::i32 rx = static_cast<io::i32>(render_distance_chunks);
        const io::i32 rz = static_cast<io::i32>(render_distance_chunks);
        const io::i32 ry = RequestYRadiusChunks();
        if (ComputeRequestVirtualCount() == 0u) {
            InvalidateChunkRequestOffsets();
            return false;
        }
        if (!ge::client::chunk::BuildNearestOffsets(chunk_request_offsets, rx, ry, rz))
            return false;

        chunk_request_offsets_rx = rx;
        chunk_request_offsets_ry = ry;
        chunk_request_offsets_rz = rz;
        chunk_request_scan_cursor = 0u;
        chunk_request_radius_phase = 0;
        return true;
    }

    IO_NODISCARD inline bool EnsureChunkRequestOffsets() noexcept {
        const io::i32 rx = static_cast<io::i32>(render_distance_chunks);
        const io::i32 rz = static_cast<io::i32>(render_distance_chunks);
        const io::i32 ry = RequestYRadiusChunks();
        if (chunk_request_offsets_rx == rx &&
            chunk_request_offsets_ry == ry &&
            chunk_request_offsets_rz == rz &&
            !chunk_request_offsets.empty())
            return true;
        return RebuildChunkRequestOffsets();
    }

    IO_NODISCARD inline bool CoordInRequestBounds(const ge::voxel::ChunkCoord& coord,
                                                  const ge::voxel::ChunkCoord& center) const noexcept {
        const io::i32 rx = static_cast<io::i32>(render_distance_chunks);
        const io::i32 rz = static_cast<io::i32>(render_distance_chunks);
        const io::i32 ry = RequestYRadiusChunks();
        return coord.x >= center.x - rx && coord.x <= center.x + rx &&
               coord.y >= center.y - ry && coord.y <= center.y + ry &&
               coord.z >= center.z - rz && coord.z <= center.z + rz;
    }

    inline void InvalidateChunkMesh(ChunkRenderMesh& gpu, const ge::voxel::ChunkCoord& coord) noexcept {
        gpu.coord = coord;
        gpu.index_count = 0;
        gpu.built_version = 0;
        gpu.queued = false;
        gpu.uploaded = false;
    }

    IO_NODISCARD inline bool CoordInViewBounds(const ge::voxel::ChunkCoord& coord,
                                  const ge::voxel::ChunkCoord& center) const noexcept {
        const io::i32 rx = static_cast<io::i32>(render_distance_chunks);
        const io::i32 rz = static_cast<io::i32>(render_distance_chunks);
        const io::i32 ry = WorldYRadiusChunks();
        return coord.x >= center.x - rx && coord.x <= center.x + rx &&
               coord.y >= center.y - ry && coord.y <= center.y + ry &&
               coord.z >= center.z - rz && coord.z <= center.z + rz;
    }

    inline void RegenerateChunkSlot(io::usize index, const ge::voxel::ChunkCoord& coord) noexcept {
        ge::voxel::ChunkData& chunk = voxel_world.chunks[index];
        ChunkRenderMesh& gpu = chunk_meshes[index];
        const ge::voxel::ChunkCoord old_coord = chunk.coord;
        chunk.coord = coord;
        voxel_world.invalidate_lookup();
        chunk.clear();
        chunk.generated = false;
        chunk.dirty_mesh = false;
        chunk.dirty_neighbors = false;
        InvalidateChunkMesh(gpu, coord);
        ChunkSlotLookupErase(old_coord);
        if (!ChunkSlotLookupInsertOrUpdate(coord, index))
            InvalidateChunkSlotLookup();
    }

    inline void TouchChunkForNeighborUpdate(const ge::voxel::ChunkCoord& coord) noexcept {
        ge::voxel::ChunkData* chunk = voxel_world.find_chunk(coord);
        if (!chunk) return;
        chunk->touch(); // bump version so scheduler cannot skip rebuild
        chunk->dirty_neighbors = true;
    }

    inline void TouchNeighborsOf(const ge::voxel::ChunkCoord& coord) noexcept {
        TouchChunkForNeighborUpdate(ge::voxel::ChunkCoord{ coord.x + 1, coord.y, coord.z });
        TouchChunkForNeighborUpdate(ge::voxel::ChunkCoord{ coord.x - 1, coord.y, coord.z });
        TouchChunkForNeighborUpdate(ge::voxel::ChunkCoord{ coord.x, coord.y + 1, coord.z });
        TouchChunkForNeighborUpdate(ge::voxel::ChunkCoord{ coord.x, coord.y - 1, coord.z });
        TouchChunkForNeighborUpdate(ge::voxel::ChunkCoord{ coord.x, coord.y, coord.z + 1 });
        TouchChunkForNeighborUpdate(ge::voxel::ChunkCoord{ coord.x, coord.y, coord.z - 1 });
    }

    inline void InvalidateInflightLookup() noexcept {
        inflight_lookup_dirty = true;
    }

    inline void ClearInflightLookup() noexcept {
        inflight_lookup.clear();
        inflight_lookup_mask = 0u;
        inflight_lookup_dirty = true;
    }

    IO_NODISCARD inline bool EnsureInflightLookupCapacity(io::u32 wanted_items) noexcept {
        io::u32 cap = static_cast<io::u32>(inflight_lookup.size());
        if (cap != 0u && wanted_items * 2u <= cap)
            return true;

        cap = wanted_items * 2u;
        if (cap < 8u) cap = 8u;
        cap = NextPow2U32(cap);
        if (!inflight_lookup.resize(cap))
            return false;
        for (io::u32 i = 0u; i < cap; ++i) {
            inflight_lookup[i].state = 0u;
            inflight_lookup[i].index = io::npos;
        }
        inflight_lookup_mask = cap - 1u;
        for (io::usize i = 0; i < chunk_requests_inflight.size(); ++i) {
            const ge::voxel::ChunkCoord coord = chunk_requests_inflight[i].coord;
            io::u32 pos = HashChunkCoord(coord) & inflight_lookup_mask;
            bool placed = false;
            for (io::u32 probe = 0u; probe < cap; ++probe) {
                InflightLookupEntry& e = inflight_lookup[pos];
                if (e.state == 0u || ge::voxel::coord_eq(e.coord, coord)) {
                    e.coord = coord;
                    e.index = i;
                    e.state = 1u;
                    placed = true;
                    break;
                }
                pos = (pos + 1u) & inflight_lookup_mask;
            }
            if (!placed) {
                inflight_lookup_dirty = true;
                return false;
            }
        }
        inflight_lookup_dirty = false;
        return true;
    }

    IO_NODISCARD inline bool RebuildInflightLookup() noexcept {
        inflight_lookup.clear();
        inflight_lookup_mask = 0u;
        if (chunk_requests_inflight.empty()) {
            inflight_lookup_dirty = false;
            return true;
        }
        return EnsureInflightLookupCapacity(static_cast<io::u32>(chunk_requests_inflight.size()));
    }

    IO_NODISCARD inline io::usize LookupInflightChunkRequestIndex(const ge::voxel::ChunkCoord& coord) const noexcept {
        if (inflight_lookup.empty() || inflight_lookup_mask == 0u)
            return io::npos;
        const io::u32 cap = static_cast<io::u32>(inflight_lookup.size());
        io::u32 pos = HashChunkCoord(coord) & inflight_lookup_mask;
        for (io::u32 probe = 0u; probe < cap; ++probe) {
            const InflightLookupEntry& e = inflight_lookup[pos];
            if (e.state == 0u) return io::npos;
            if (e.state == 1u && ge::voxel::coord_eq(e.coord, coord))
                return e.index;
            pos = (pos + 1u) & inflight_lookup_mask;
        }
        return io::npos;
    }

    IO_NODISCARD inline bool InflightLookupInsertOrUpdate(const ge::voxel::ChunkCoord& coord, io::usize index) noexcept {
        if (!EnsureInflightLookupCapacity(static_cast<io::u32>(chunk_requests_inflight.size() + 1u)))
            return false;
        const io::u32 cap = static_cast<io::u32>(inflight_lookup.size());
        io::u32 pos = HashChunkCoord(coord) & inflight_lookup_mask;
        io::u32 first_tomb = static_cast<io::u32>(-1);
        for (io::u32 probe = 0u; probe < cap; ++probe) {
            InflightLookupEntry& e = inflight_lookup[pos];
            if (e.state == 0u) {
                if (first_tomb != static_cast<io::u32>(-1))
                    pos = first_tomb;
                inflight_lookup[pos].coord = coord;
                inflight_lookup[pos].index = index;
                inflight_lookup[pos].state = 1u;
                return true;
            }
            if (e.state == 2u) {
                if (first_tomb == static_cast<io::u32>(-1))
                    first_tomb = pos;
            } else if (ge::voxel::coord_eq(e.coord, coord)) {
                e.index = index;
                return true;
            }
            pos = (pos + 1u) & inflight_lookup_mask;
        }
        inflight_lookup_dirty = true;
        return false;
    }

    inline void InflightLookupErase(const ge::voxel::ChunkCoord& coord) noexcept {
        if (inflight_lookup.empty() || inflight_lookup_mask == 0u)
            return;
        const io::u32 cap = static_cast<io::u32>(inflight_lookup.size());
        io::u32 pos = HashChunkCoord(coord) & inflight_lookup_mask;
        for (io::u32 probe = 0u; probe < cap; ++probe) {
            InflightLookupEntry& e = inflight_lookup[pos];
            if (e.state == 0u)
                return;
            if (e.state == 1u && ge::voxel::coord_eq(e.coord, coord)) {
                e.state = 2u;
                e.index = io::npos;
                return;
            }
            pos = (pos + 1u) & inflight_lookup_mask;
        }
    }

    IO_NODISCARD inline io::usize FindInflightChunkRequestIndex(const ge::voxel::ChunkCoord& coord) noexcept {
        if (inflight_lookup_dirty)
            (void)RebuildInflightLookup();

        const io::usize idx = LookupInflightChunkRequestIndex(coord);
        if (idx != io::npos && idx < chunk_requests_inflight.size() &&
            ge::voxel::coord_eq(chunk_requests_inflight[idx].coord, coord))
            return idx;

        for (io::usize i = 0; i < chunk_requests_inflight.size(); ++i) {
            if (!ge::voxel::coord_eq(chunk_requests_inflight[i].coord, coord))
                continue;
            (void)InflightLookupInsertOrUpdate(coord, i);
            return i;
        }
        return io::npos;
    }

    inline void RemoveInflightChunkRequestByIndex(io::usize index) noexcept {
        if (index >= chunk_requests_inflight.size())
            return;
        const ge::voxel::ChunkCoord removed_coord = chunk_requests_inflight[index].coord;
        const io::usize last = chunk_requests_inflight.size() - 1u;
        if (index != last) {
            chunk_requests_inflight[index] = chunk_requests_inflight[last];
            (void)InflightLookupInsertOrUpdate(chunk_requests_inflight[index].coord, index);
        }
        chunk_requests_inflight.pop_back();
        InflightLookupErase(removed_coord);
        if (chunk_requests_inflight.empty())
            ClearInflightLookup();
    }

    inline void RemoveInflightChunkRequest(const ge::voxel::ChunkCoord& coord) noexcept {
        const io::usize i = FindInflightChunkRequestIndex(coord);
        if (i != io::npos)
            RemoveInflightChunkRequestByIndex(i);
    }

    inline void RemoveChunkSlotByIndex(io::usize index) noexcept {
        if (index >= voxel_world.chunks.size() || index >= chunk_meshes.size())
            return;
        const io::usize last = voxel_world.chunks.size() - 1u;
        const ge::voxel::ChunkCoord removed_coord = voxel_world.chunks[index].coord;
        if (index != last) {
            voxel_world.chunks[index] = io::move(voxel_world.chunks[last]);
            chunk_meshes[index] = io::move(chunk_meshes[last]);
            if (chunk_job_slots) {
                for (io::u32 i = 0; i < chunk_job_slots_active; ++i) {
                    ChunkMeshJobSlot& slot = chunk_job_slots[i];
                    const io::u32 s = slot.state.load();
                    if (s != static_cast<io::u32>(ChunkJobState::Queued) &&
                        s != static_cast<io::u32>(ChunkJobState::Building))
                        continue;
                    if (slot.world_chunk_index == last)
                        slot.world_chunk_index = index;
                }
            }
        }
        voxel_world.chunks.pop_back();
        chunk_meshes.pop_back();
        voxel_world.invalidate_lookup();
        RemoveInflightChunkRequest(removed_coord);
        ChunkSlotLookupErase(removed_coord);
        if (index != last && index < voxel_world.chunks.size()) {
            const ge::voxel::ChunkCoord moved_coord = voxel_world.chunks[index].coord;
            if (!ChunkSlotLookupInsertOrUpdate(moved_coord, index))
                InvalidateChunkSlotLookup();
        }
    }

    inline void RecenterChunkWorld(const ge::voxel::ChunkCoord& center, bool force_all) noexcept {
        if (!force_all) {
            for (io::usize i = voxel_world.chunks.size(); i > 0; --i) {
                const io::usize idx = i - 1u;
                if (!CoordInViewBounds(voxel_world.chunks[idx].coord, center))
                    RemoveChunkSlotByIndex(idx);
            }

            for (io::usize i = chunk_requests_inflight.size(); i > 0; --i) {
                const io::usize idx = i - 1u;
                if (!CoordInRequestBounds(chunk_requests_inflight[idx].coord, center))
                    RemoveInflightChunkRequestByIndex(idx);
            }
        } else {
            chunk_meshes.clear();
            voxel_world.clear();
            ClearChunkSlotLookup();
            chunk_requests_inflight.clear();
            ClearInflightLookup();
            chunk_request_scan_cursor = 0u;
            chunk_request_radius_phase = 0;
        }

        chunk_request_virtual_count = ComputeRequestVirtualCount();
        if (!EnsureChunkRequestOffsets())
            InvalidateChunkRequestOffsets();
        chunk_request_scan_cursor = 0u;
        chunk_request_radius_phase = 0;

        chunk_center_coord = center;
        chunk_center_valid = true;
    }

    inline void UpdateChunkRequestCenterOnly(const ge::voxel::ChunkCoord& center) noexcept {
        for (io::usize i = chunk_requests_inflight.size(); i > 0; --i) {
            const io::usize idx = i - 1u;
            if (!CoordInRequestBounds(chunk_requests_inflight[idx].coord, center))
                RemoveInflightChunkRequestByIndex(idx);
        }
        chunk_request_virtual_count = ComputeRequestVirtualCount();
        if (!EnsureChunkRequestOffsets())
            InvalidateChunkRequestOffsets();
        chunk_request_scan_cursor = 0u;
        chunk_request_radius_phase = 0;
        chunk_center_coord = center;
        chunk_center_valid = true;
    }

    inline void RefreshActiveChunkMarks() noexcept {
        chunk_slot_marks.clear();
        if (!chunk_slot_marks.resize(voxel_world.chunks.size()))
            return;
        for (io::usize i = 0; i < chunk_slot_marks.size(); ++i)
            chunk_slot_marks[i] = 0u;
        if (!chunk_job_slots) return;
        for (io::u32 i = 0; i < chunk_job_slots_active; ++i) {
            const ChunkMeshJobSlot& slot = chunk_job_slots[i];
            const io::u32 s = slot.state.load();
            if (s != static_cast<io::u32>(ChunkJobState::Queued) &&
                s != static_cast<io::u32>(ChunkJobState::Building))
                continue;
            if (slot.world_chunk_index < chunk_slot_marks.size())
                chunk_slot_marks[slot.world_chunk_index] = 1u;
        }
    }

    inline void PruneChunkWorldIncremental(const ge::voxel::ChunkCoord& center, io::u32 budget) noexcept {
        if (budget == 0u) return;
        if (voxel_world.chunks.empty()) return;

        RefreshActiveChunkMarks();
        io::u32 removed = 0u;
        for (io::usize i = voxel_world.chunks.size(); i > 0 && removed < budget; --i) {
            const io::usize idx = i - 1u;
            if (CoordInViewBounds(voxel_world.chunks[idx].coord, center))
                continue;

            bool active = false;
            if (idx < chunk_slot_marks.size())
                active = chunk_slot_marks[idx] != 0u;
            else
                active = HasActiveChunkJobForIndex(idx);
            if (active)
                continue;

            const io::usize last = voxel_world.chunks.size() - 1u;
            RemoveChunkSlotByIndex(idx);
            if (!chunk_slot_marks.empty() && idx < chunk_slot_marks.size()) {
                if (idx != last && last < chunk_slot_marks.size())
                    chunk_slot_marks[idx] = chunk_slot_marks[last];
                chunk_slot_marks.pop_back();
            }
            ++removed;
        }

        // If cache is still tight, evict farthest chunks (even if still in view bounds),
        // but never touch chunks that are currently being meshed.
        if (budget > removed && voxel_world.max_chunks > 0u) {
            io::usize soft_limit = voxel_world.max_chunks;
            if (soft_limit > 128u) soft_limit -= 128u;
            while (removed < budget && voxel_world.chunks.size() > soft_limit) {
                io::usize victim = io::npos;
                io::i32 best_d2 = -1;
                for (io::usize i = 0; i < voxel_world.chunks.size(); ++i) {
                    bool active = false;
                    if (i < chunk_slot_marks.size())
                        active = chunk_slot_marks[i] != 0u;
                    else
                        active = HasActiveChunkJobForIndex(i);
                    if (active) continue;

                    const ge::voxel::ChunkCoord c = voxel_world.chunks[i].coord;
                    const io::i32 dx = c.x - center.x;
                    const io::i32 dy = c.y - center.y;
                    const io::i32 dz = c.z - center.z;
                    const io::i32 d2 = dx * dx + dy * dy + dz * dz;
                    if (victim == io::npos || d2 > best_d2) {
                        victim = i;
                        best_d2 = d2;
                    }
                }
                if (victim == io::npos) break;

                const io::usize last = voxel_world.chunks.size() - 1u;
                RemoveChunkSlotByIndex(victim);
                if (!chunk_slot_marks.empty() && victim < chunk_slot_marks.size()) {
                    if (victim != last && last < chunk_slot_marks.size())
                        chunk_slot_marks[victim] = chunk_slot_marks[last];
                    chunk_slot_marks.pop_back();
                }
                ++removed;
            }
        }
    }

