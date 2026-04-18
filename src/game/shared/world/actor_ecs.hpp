#pragma once

#include "hi/hi/hi.hpp"

#include "../net/protocol.hpp"
#include "item.hpp"

namespace ge {
namespace ecs {
    enum class MobLogicState : io::u8 {
        Idle = 0,
        Walk = 1,
        Run = 2,
        Alert = 3,
        Attack = 4,
        Hurt = 5,
        Dead = 6,
        Flee = 7,
        Eat = 8,
        Look = 9
    };

    struct CActorIdentity {
        io::u16 actor_id = 0u;
        io::u8 model = ge::net::WORLD_ACTOR_MODEL_NONE;
        io::u8 mode = ge::net::WORLD_ACTOR_MODE_ENTITY;
    };

    struct CTransform {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct CVelocity {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct CHealth {
        io::u16 hp = 100u;
    };

    struct CMobSim {
        float dir_x = 0.f;
        float dir_y = 0.f;
        float dir_z = 0.f;
        bool grounded = false;
        io::i32 target_peer = -1;
        MobLogicState ai_state = MobLogicState::Idle;
    };

    // Logical mob/entity state and network-facing state/animation.
    struct CMobState {
        MobLogicState logical = MobLogicState::Idle;
        io::u8 net_state = ge::net::WORLD_ACTOR_STATE_ENTITY_STAY;
        io::u8 net_anim = ge::net::WORLD_ACTOR_ANIM_STAY;
    };

    // Client animator runtime state.
    struct CAnimator {
        io::u32 current_clip = 0xFFFFFFFFu;
        io::u32 next_clip = 0xFFFFFFFFu;
        float clip_time = 0.f;
        float blend_alpha = 0.f;
        float playback_speed = 1.f;
        bool loop = true;
    };

    // Client render model binding.
    struct CRenderModel {
        io::usize model_index = static_cast<io::usize>(-1);
        io::u32 bone_count = 0u;
        bool visible = true;
    };

    struct CAnchor {
        bool has_anchor = false;
        io::i32 wx = 0;
        io::i32 wy = 0;
        io::i32 wz = 0;
    };

    struct CItemDrop {
        ge::item::Stack stack{};
        io::u32 pile_count = 0u; // world pile amount (can exceed per-inventory max_stack)
        bool grounded = false;
        io::u32 despawn_ms = 0u;
    };

    struct CSpellFx {
        ge::item::Id spell = ge::item::Id::None;
        io::u8 owner_peer = 0xFFu;
        io::u8 flags = 0u;
        io::u16 _pad = 0u;
        io::u32 ttl_ms = 0u;
        float radius = 0.f;
        float power = 0.f;
    };

    // Transport sync state (server+client).
    struct CNetSync {
        bool active = false;
        bool dirty = false;
        io::u64 last_update_ms = 0u;
        io::u64 last_broadcast_ms = 0u;
    };

    template<io::u32 CAPACITY>
    struct ActorEcs {
        static constexpr io::u32 CAP = CAPACITY;
        static constexpr io::u16 INVALID_ACTOR_ID = 0u;

        io::u16 next_actor_id = 1u;
        io::u32 alive_count = 0u;
        io::u8 alive[CAP]{};
        CActorIdentity identity[CAP]{};
        CTransform transform[CAP]{};
        CVelocity velocity[CAP]{};
        CHealth health[CAP]{};
        CMobSim mob_sim[CAP]{};
        CMobState mob_state[CAP]{};
        CAnimator animator[CAP]{};
        CRenderModel render_model[CAP]{};
        CAnchor anchor[CAP]{};
        CItemDrop item_drop[CAP]{};
        CSpellFx spell_fx[CAP]{};
        CNetSync net_sync[CAP]{};

        inline void Reset() noexcept {
            next_actor_id = 1u;
            alive_count = 0u;
            for (io::u32 i = 0; i < CAP; ++i) {
                alive[i] = 0u;
                identity[i] = {};
                transform[i] = {};
                velocity[i] = {};
                health[i] = {};
                mob_sim[i] = {};
                mob_state[i] = {};
                animator[i] = {};
                render_model[i] = {};
                anchor[i] = {};
                item_drop[i] = {};
                spell_fx[i] = {};
                net_sync[i] = {};
            }
        }

        IO_NODISCARD inline io::i32 FindFree() const noexcept {
            for (io::u32 i = 0; i < CAP; ++i)
                if (alive[i] == 0u)
                    return static_cast<io::i32>(i);
            return -1;
        }

        IO_NODISCARD inline io::i32 FindByActorId(io::u16 actor_id) const noexcept {
            if (actor_id == INVALID_ACTOR_ID) return -1;
            for (io::u32 i = 0; i < CAP; ++i) {
                if (alive[i] == 0u) continue;
                if (identity[i].actor_id == actor_id) return static_cast<io::i32>(i);
            }
            return -1;
        }

        IO_NODISCARD inline io::i32 FindByAnchor(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            for (io::u32 i = 0; i < CAP; ++i) {
                if (alive[i] == 0u) continue;
                if (!anchor[i].has_anchor) continue;
                if (anchor[i].wx != wx || anchor[i].wy != wy || anchor[i].wz != wz) continue;
                return static_cast<io::i32>(i);
            }
            return -1;
        }

        inline io::u16 AllocActorId() noexcept {
            if (next_actor_id == 0u) next_actor_id = 1u;
            for (io::u32 k = 0u; k < 0x10000u; ++k) {
                const io::u16 id = next_actor_id++;
                if (next_actor_id == 0u) next_actor_id = 1u;
                if (id == INVALID_ACTOR_ID) continue;
                if (FindByActorId(id) < 0) return id;
            }
            return INVALID_ACTOR_ID;
        }

        IO_NODISCARD inline io::i32 Spawn(io::u8 model, io::u8 mode, io::u16 forced_actor_id = INVALID_ACTOR_ID) noexcept {
            const io::i32 slot = FindFree();
            if (slot < 0) return -1;
            const io::u32 i = static_cast<io::u32>(slot);
            io::u16 actor_id = forced_actor_id;
            if (actor_id == INVALID_ACTOR_ID)
                actor_id = AllocActorId();
            if (actor_id == INVALID_ACTOR_ID) return -1;

            alive[i] = 1u;
            ++alive_count;
            identity[i] = {};
            identity[i].actor_id = actor_id;
            identity[i].model = model;
            identity[i].mode = mode;
            transform[i] = {};
            velocity[i] = {};
            health[i] = {};
            mob_sim[i] = {};
            mob_state[i] = {};
            animator[i] = {};
            render_model[i] = {};
            anchor[i] = {};
            item_drop[i] = {};
            spell_fx[i] = {};
            net_sync[i] = {};
            net_sync[i].active = true;
            net_sync[i].dirty = true;
            return slot;
        }

        inline void MarkDirty(io::u32 i) noexcept {
            if (i >= CAP || alive[i] == 0u) return;
            net_sync[i].dirty = true;
            net_sync[i].last_broadcast_ms = 0u;
        }

        inline void MarkInactive(io::u32 i) noexcept {
            if (i >= CAP || alive[i] == 0u) return;
            net_sync[i].active = false;
            net_sync[i].dirty = true;
            net_sync[i].last_broadcast_ms = 0u;
        }

        inline void Erase(io::u32 i) noexcept {
            if (i >= CAP || alive[i] == 0u) return;
            alive[i] = 0u;
            if (alive_count > 0u) --alive_count;
            identity[i] = {};
            transform[i] = {};
            velocity[i] = {};
            health[i] = {};
            mob_sim[i] = {};
            mob_state[i] = {};
            animator[i] = {};
            render_model[i] = {};
            anchor[i] = {};
            item_drop[i] = {};
            spell_fx[i] = {};
            net_sync[i] = {};
        }
    };
}
}
