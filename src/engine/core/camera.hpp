#pragma once

#include "../3rd_party/linmath/linmath/vec.hpp"
#include "../3rd_party/linmath/linmath/mat.hpp"

namespace ge {
struct Camera {
    lm::vec3 position{ 0.0f, 0.0f, 0.0f };

    lm::vec3 front{ 0.0f, 0.0f, -1.0f };
    lm::vec3 right{ 1.0f, 0.0f, 0.0f };
    lm::vec3 up{ 0.0f, 1.0f, 0.0f };

    static constexpr lm::vec3 world_up{ 0.0f, 1.0f, 0.0f };

    float yaw = -90.0f;
    float pitch = 0.0f;

    float fov = 70.0f;
    float movement_speed = 5.0f;
    float mouse_sensitivity = 0.1f;

    inline Camera() noexcept = default;

    inline lm::mat4 view_matrix() const noexcept {
        return lm::mat4_look_at(position, position + front, up);
    }

    inline void process_mouse_movement(float dx, float dy) noexcept {
        dx *= mouse_sensitivity;
        dy *= mouse_sensitivity;

        yaw += dx;
        pitch += dy;

        if (pitch > 89.0f)  pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        update_vectors();
    }

    inline void move_forward(double dt) noexcept {
        position += flat_forward() * (movement_speed * static_cast<float>(dt));
    }
    inline void move_backward(double dt) noexcept {
        position -= flat_forward() * (movement_speed * static_cast<float>(dt));
    }
    inline void move_right(double dt) noexcept {
        position += flat_right() * (movement_speed * static_cast<float>(dt));
    }
    inline void move_left(double dt) noexcept {
        position -= flat_right() * (movement_speed * static_cast<float>(dt));
    }
    inline void move_up(double dt) noexcept {
        position += world_up * (movement_speed * static_cast<float>(dt));
    }
    inline void move_down(double dt) noexcept {
        position -= world_up * (movement_speed * static_cast<float>(dt));
    }

private:
    inline lm::vec3 flat_forward() const noexcept {
        lm::vec3 f{ front[0], 0.0f, front[2] };
        return lm::vec3_norm(f);
    }

    inline lm::vec3 flat_right() const noexcept {
        return lm::vec3_norm(lm::vec3_cross(flat_forward(), world_up));
    }

    inline void update_vectors() noexcept {
        const float yaw_rad =  lm::radians(yaw);
        const float pitch_rad = lm::radians(pitch);

        const float cos_pitch = lm::cosf(pitch_rad);

        front = lm::vec3_norm({
            lm::cosf(yaw_rad) * cos_pitch,
            lm::sinf(pitch_rad),
            lm::sinf(yaw_rad) * cos_pitch
            });

        right = lm::vec3_norm(lm::vec3_cross(front, world_up));
        up = lm::vec3_norm(lm::vec3_cross(right, front));
    }
}; // struct Camera
} // namespace hi