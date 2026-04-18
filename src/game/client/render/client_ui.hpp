#pragma once

#include "hi/hi/hi.hpp"

#include "../../../engine/core/config.hpp"
#include "../../shared/world/item.hpp"

namespace ge {
namespace ui {
    static inline hi::UiColor Color(float r, float g, float b, float a) noexcept {
        hi::UiColor out{};
        out.r = r;
        out.g = g;
        out.b = b;
        out.a = a;
        return out;
    }

    IO_NODISCARD static inline float Wrap01(float v) noexcept {
        while (v < 0.f) v += 1.f;
        while (v >= 1.f) v -= 1.f;
        return v;
    }

    IO_NODISCARD static inline hi::UiColor HsvToRgb(float h, float s, float v, float a) noexcept {
        h = Wrap01(h);
        if (s <= 0.0001f)
            return Color(v, v, v, a);

        const float h6 = h * 6.f;
        const io::i32 sector = static_cast<io::i32>(h6);
        const float f = h6 - static_cast<float>(sector);
        const float p = v * (1.f - s);
        const float q = v * (1.f - s * f);
        const float t = v * (1.f - s * (1.f - f));

        switch (sector % 6) {
        case 0: return Color(v, t, p, a);
        case 1: return Color(q, v, p, a);
        case 2: return Color(p, v, t, a);
        case 3: return Color(p, q, v, a);
        case 4: return Color(t, p, v, a);
        default: return Color(v, p, q, a);
        }
    }

    static inline void ApplyButtonStyle(hi::ButtonDraw& button) noexcept {
        button.style.normal = ge::BUTTON_STYLE_NORMAL;
        button.style.hover = ge::BUTTON_STYLE_HOVER;
        button.style.active = ge::BUTTON_STYLE_ACTIVE;
        button.style.underscored = true;
    }

    static inline hi::TextDraw TextRegular(hi::AtlasId atlas, io::char_view text) noexcept {
        hi::TextDraw td{};
        td.atlas = atlas;
        td.text = text;
        td.scale = 0.82f;
        td.space_between = -0.25f;
        td.style.r = 1.f;
        td.style.g = 1.f;
        td.style.b = 1.f;
        td.style.a = 1.f;
        return td;
    }

    static inline hi::TextDraw TextHeader(hi::AtlasId atlas, io::char_view text) noexcept {
        hi::TextDraw td{};
        td.atlas = atlas;
        td.text = text;
        td.scale = 2.f;
        td.space_between = +0.25f;
        td.style.r = 1.f;
        td.style.g = 1.f;
        td.style.b = 1.f;
        td.style.a = 1.f;
        return td;
    }

    static inline hi::ButtonDraw ButtonRegular(hi::AtlasId atlas, io::char_view text) noexcept {
        hi::ButtonDraw button{};
        button.atlas = atlas;
        button.text = text;
        button.scale = 0.76f;
        button.space_between = -0.25f;
        button.style.pad_top = 4.f;
        button.style.pad_bottom = 4.f;
        button.style.pad_left = 8.f;
        button.style.pad_right = 8.f;
        ApplyButtonStyle(button);
        return button;
    }

    static inline hi::ButtonDraw ButtonHeader(hi::AtlasId atlas, io::char_view text) noexcept {
        hi::ButtonDraw button{};
        button.atlas = atlas;
        button.text = text;
        button.scale = 1.f;
        button.space_between = -0.25f;
        button.style.pad_top = 6.f;
        button.style.pad_bottom = 6.f;
        button.style.pad_left = 12.f;
        button.style.pad_right = 12.f;
        ApplyButtonStyle(button);
        return button;
    }

    static inline void ApplySliderStyle(hi::SliderDraw& slider) noexcept {
        slider.style.min_width = 420.f;
        slider.style.track_height = 6.f;
        slider.style.track_gap = 8.f;
        slider.style.handle_size = 12.f;
        slider.style.box.border = true;
        slider.style.box.border_radius = 8.f;
        slider.style.box.pad_top = 7.f;
        slider.style.box.pad_bottom = 7.f;
        slider.style.box.pad_left = 10.f;
        slider.style.box.pad_right = 10.f;
        slider.style.normal = ge::BUTTON_STYLE_NORMAL;
        slider.style.hover = ge::BUTTON_STYLE_HOVER;
        slider.style.active = ge::BUTTON_STYLE_ACTIVE;
    }

    static inline hi::SliderDraw SliderRegular(hi::AtlasId atlas, io::char_view text, float* value,
                                                float min_value, float max_value, float step,
                                                io::u32 id) noexcept {
        hi::SliderDraw slider{};
        slider.atlas = atlas;
        slider.text = text;
        slider.value = value;
        slider.min_value = min_value;
        slider.max_value = max_value;
        slider.step = step;
        slider.id = id;
        slider.scale = 0.9f;
        ApplySliderStyle(slider);
        return slider;
    }

    static inline hi::SliderDraw SliderHeader(hi::AtlasId atlas, io::char_view text, float* value,
                                               float min_value, float max_value, float step,
                                               io::u32 id) noexcept {
        hi::SliderDraw slider{};
        slider.atlas = atlas;
        slider.text = text;
        slider.value = value;
        slider.min_value = min_value;
        slider.max_value = max_value;
        slider.step = step;
        slider.id = id;
        slider.scale = 1.2f;
        ApplySliderStyle(slider);
        return slider;
    }

    static inline hi::TextFieldDraw TextInputRegular(hi::AtlasId atlas,
                                                      io::char_view_mut text,
                                                      io::usize* text_len,
                                                      io::u32 id) noexcept {
        hi::TextFieldDraw field{};
        field.atlas = atlas;
        field.text = text;
        field.text_len = text_len;
        field.id = id;
        field.style.box.border = true;
        field.style.box.border_radius = 8.f;
        return field;
    }

    static inline hi::TextFieldDraw TextInputHeader(hi::AtlasId atlas,
                                                     io::char_view_mut text,
                                                     io::usize* text_len,
                                                     io::u32 id) noexcept {
        hi::TextFieldDraw field{};
        field.atlas = atlas;
        field.text = text;
        field.text_len = text_len;
        field.id = id;
        field.style.box.border = true;
        field.style.box.border_radius = 0.f;
        return field;
    }

    static inline hi::PanelDraw PanelCard(bool dark_theme) noexcept {
        hi::PanelDraw panel{};
        panel.style.border = true;
        panel.style.border_radius = 18.f;
        panel.style.pad_top = 0.f;
        panel.style.pad_left = 0.f;
        panel.style.pad_right = 0.f;
        panel.style.pad_bottom = 0.f;
        if (dark_theme) {
            panel.fill = Color(0.15f, 0.15f, 0.16f, 0.26f);
            panel.border = Color(0.03f, 0.18f, 0.12f, 0.06f);
        } else {
            panel.fill = Color(0.36f, 0.31f, 0.20f, 0.74f);
            panel.border = Color(0.50f, 0.58f, 0.62f, 0.26f);
        }
        panel.border_px = 1.25f;
        return panel;
    }

    static inline hi::PanelButtonDraw SlotButton(bool selected, bool picked, bool dark_theme) noexcept {
        hi::PanelButtonDraw button{};
        button.style.border = true;
        button.style.border_radius = 14.f;
        button.style.pad_top = 0.f;
        button.style.pad_left = 0.f;
        button.style.pad_right = 0.f;
        button.style.pad_bottom = 0.f;
        if (dark_theme) {
            button.fill_normal = Color(0.00f, 0.00f, 0.00f, selected ? 0.58f : 0.46f);
            button.fill_hover = Color(0.00f, 0.00f, 0.00f, selected ? 0.68f : 0.56f);
            button.fill_active = Color(0.00f, 0.00f, 0.00f, picked ? 0.78f : 0.66f);
            button.border_normal = selected ? Color(0.42f, 0.50f, 0.56f, 0.30f) : Color(0.34f, 0.40f, 0.44f, 0.14f);
            button.border_hover = selected ? Color(0.50f, 0.58f, 0.62f, 0.40f) : Color(0.42f, 0.48f, 0.52f, 0.22f);
            button.border_active = picked ? Color(0.58f, 0.66f, 0.70f, 0.48f) : Color(0.48f, 0.56f, 0.60f, 0.30f);
        } else {
            button.fill_normal = Color(0.00f, 0.00f, 0.00f, selected ? 0.84f : 0.78f);
            button.fill_hover = Color(0.00f, 0.00f, 0.00f, selected ? 0.90f : 0.86f);
            button.fill_active = Color(0.00f, 0.00f, 0.00f, picked ? 0.95f : 0.92f);
            button.border_normal = selected ? Color(0.62f, 0.67f, 0.72f, 0.46f) : Color(0.48f, 0.54f, 0.58f, 0.24f);
            button.border_hover = selected ? Color(0.68f, 0.74f, 0.78f, 0.58f) : Color(0.56f, 0.62f, 0.66f, 0.34f);
            button.border_active = picked ? Color(0.72f, 0.78f, 0.82f, 0.66f) : Color(0.62f, 0.68f, 0.72f, 0.44f);
        }
        button.border_px = selected ? 2.0f : 1.25f;
        return button;
    }

    static inline void TintSlotButtonForStack(hi::PanelButtonDraw& button,
                                              const ge::item::Stack& stack,
                                              bool dark_theme) noexcept {
        if (ge::item::is_empty(stack)) {
            if (dark_theme) {
                button.fill_normal = Color(0.f, 0.f, 0.f, button.fill_normal.a);
                button.fill_hover = Color(0.f, 0.f, 0.f, button.fill_hover.a);
                button.fill_active = Color(0.f, 0.f, 0.f, button.fill_active.a);
            } else {
                button.fill_normal = Color(0.f, 0.f, 0.f, button.fill_normal.a);
                button.fill_hover = Color(0.f, 0.f, 0.f, button.fill_hover.a);
                button.fill_active = Color(0.f, 0.f, 0.f, button.fill_active.a);
            }
            return;
        }

        hi::UiColor base = Color(0.18f, 0.15f, 0.34f, 0.22f);
        const ge::item::Category category = ge::item::def(stack.id).category;
        if (category == ge::item::Category::Consumables) {
            switch (ge::item::freshness_band(stack)) {
            case ge::item::FreshnessBand::Fresh: base = Color(0.16f, 0.74f, 0.28f, 0.20f); break;
            case ge::item::FreshnessBand::Stale: base = Color(0.92f, 0.78f, 0.18f, 0.20f); break;
            default: base = Color(0.84f, 0.18f, 0.16f, 0.20f); break;
            }
        } else if (category == ge::item::Category::Spells) {
            base = Color(0.52f, 0.28f, 0.92f, 0.20f);
        } else if (category == ge::item::Category::SpellingWards) {
            base = Color(0.10f, 0.48f, 0.52f, 0.20f);
        } else if (category == ge::item::Category::Materials) {
            base = dark_theme
                ? Color(0.22f, 0.18f, 0.18f, 0.20f)
                : Color(0.24f, 0.18f, 0.16f, 0.18f);
        } else {
            base = dark_theme
                ? Color(0.18f, 0.14f, 0.34f, 0.22f)
                : Color(0.16f, 0.13f, 0.32f, 0.20f);
        }

        const auto bump = [](hi::UiColor c, float delta_a) noexcept {
            c.a += delta_a;
            if (c.a < 0.f) c.a = 0.f;
            if (c.a > 1.f) c.a = 1.f;
            return c;
        };
        button.fill_normal = base;
        button.fill_hover = bump(base, 0.08f);
        button.fill_active = bump(base, 0.14f);
    }

    static inline void TintSlotButtonSpellRainbow(hi::PanelButtonDraw& button, float time_sec) noexcept {
        const hi::UiColor base = HsvToRgb(time_sec * 0.20f, 0.65f, 0.95f, 0.20f);
        const auto bump = [](hi::UiColor c, float delta_a) noexcept {
            c.a += delta_a;
            if (c.a < 0.f) c.a = 0.f;
            if (c.a > 1.f) c.a = 1.f;
            return c;
        };
        button.fill_normal = base;
        button.fill_hover = bump(base, 0.08f);
        button.fill_active = bump(base, 0.14f);
    }

    static inline hi::UiColor ItemAccentColor(const ge::item::Stack& stack) noexcept {
        if (ge::item::is_empty(stack))
            return Color(0.82f, 0.84f, 0.88f, 0.94f);

        const ge::item::Category category = ge::item::def(stack.id).category;
        if (category == ge::item::Category::Consumables) {
            switch (ge::item::freshness_band(stack)) {
            case ge::item::FreshnessBand::Fresh: return Color(0.36f, 0.92f, 0.48f, 0.98f);
            case ge::item::FreshnessBand::Stale: return Color(0.96f, 0.84f, 0.28f, 0.98f);
            default: return Color(0.96f, 0.34f, 0.34f, 0.98f);
            }
        }
        if (category == ge::item::Category::SpellingWards)
            return Color(0.44f, 0.88f, 0.90f, 0.98f);
        if (category == ge::item::Category::Spells)
            return Color(0.86f, 0.60f, 0.98f, 0.98f);
        if (category == ge::item::Category::Materials)
            return Color(0.90f, 0.78f, 0.64f, 0.98f);
        return Color(0.78f, 0.72f, 1.00f, 0.98f);
    }

    static inline void ApplyTextColor(hi::TextDraw& td, hi::UiColor color) noexcept {
        td.style.r = color.r;
        td.style.g = color.g;
        td.style.b = color.b;
        td.style.a = color.a;
    }
} // namespace ui
} // namespace ge
