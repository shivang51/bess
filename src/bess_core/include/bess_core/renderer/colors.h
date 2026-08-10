#pragma once

#include "renderer_types.h"

namespace Bess::Core::Renderer::Colors {
    constexpr Color red = Color::fromHex(0xFF0000FF);
    constexpr Color green = Color::fromHex(0x00FF00FF);
    constexpr Color blue = Color::fromHex(0x0000FFFF);
    constexpr Color white = Color::fromHex(0xFFFFFFFF);
    constexpr Color black = Color::fromHex(0x000000FF);
    constexpr Color transparent = Color::fromHex(0x00000000);

    constexpr Color yellow = Color::fromHex(0xFFFF00FF);
    constexpr Color cyan = Color::fromHex(0x00FFFFFF);
    constexpr Color magenta = Color::fromHex(0xFF00FFFF);
    constexpr Color orange = Color::fromHex(0xFFA500FF);
    constexpr Color purple = Color::fromHex(0x800080FF);
    constexpr Color pink = Color::fromHex(0xFFC0CBFF);
    constexpr Color gold = Color::fromHex(0xFFD700FF);
    constexpr Color brown = Color::fromHex(0x8B4513FF);

    constexpr Color royalBlue = Color::fromHex(0x4169E1FF);
    constexpr Color deepCyan = Color::fromHex(0x008B8BFF);

    // Note: Following are generated with help of llm

    //
    constexpr Color slate900 =
        Color::fromHex(0x0F172AFF); // Deep background / Dark mode main
    constexpr Color slate700 = Color::fromHex(0x334155FF); // Cards / Borders
    constexpr Color slate500 =
        Color::fromHex(0x64748BFF); // Muted text / Secondary UI
    constexpr Color slate300 = Color::fromHex(0xCBD5E1FF); // Borders / Elements
    constexpr Color slate100 =
        Color::fromHex(0xF1F5F9FF); // Off-white / Light mode main

    // Standard Grays
    constexpr Color lightGray = Color::fromHex(0xC0C0C0FF);
    constexpr Color gray = Color::fromHex(0x808080FF);
    constexpr Color darkGray = Color::fromHex(0x404040FF);
    constexpr Color charcoal = Color::fromHex(0x1F1F1FFF);

    // High-Visibility
    constexpr Color tomato = Color::fromHex(0xFF6347FF);
    constexpr Color limeGreen = Color::fromHex(0x32CD32FF);
    constexpr Color dodgerBlue = Color::fromHex(0x1E90FFFF);
    constexpr Color hotPink = Color::fromHex(0xFF69B4FF);
    constexpr Color deepPurple = Color::fromHex(0x6A0DADFF);
    constexpr Color amber = Color::fromHex(0xFFBF00FF);
    constexpr Color teal = Color::fromHex(0x008080FF);
    constexpr Color navy = Color::fromHex(0x000080FF);
    constexpr Color maroon = Color::fromHex(0x800000FF);

    // Soft Pastels
    constexpr Color pastelRed = Color::fromHex(0xFFB3BAFF);
    constexpr Color pastelGreen = Color::fromHex(0xBFFFCEFF);
    constexpr Color pastelBlue = Color::fromHex(0xBAE1FFFF);
    constexpr Color pastelYellow = Color::fromHex(0xFFFFBAFF);
} // namespace Bess::Core::Renderer::Colors
