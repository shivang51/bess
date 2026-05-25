#pragma once

#include <cstdint>

namespace Bess {
    enum class MouseButton : uint8_t {
        unknown = 0,
        left = 1,
        right = 2,
        middle = 3,
        button4 = 4,
        button5 = 5,
        button6 = 6,
        button7 = 7,
        button8 = 8
    };

    enum class MouseButtonAction : uint8_t {
        unknown = 0,
        press = 1,
        release = 2,
        doubleClick = 3
    };

    enum class KeyAction : uint8_t {
        unknown = 0,
        press = 1,
        release = 2,
        repeat = 3
    };

    enum class KeyCode : uint16_t {
        unknown = 0,

        space = 32,
        apostrophe = 39, /* ' */
        comma = 44,      /* , */
        minus = 45,      /* - */
        period = 46,     /* . */
        slash = 47,      /* / */

        d0 = 48,
        d1 = 49,
        d2 = 50,
        d3 = 51,
        d4 = 52,
        d5 = 53,
        d6 = 54,
        d7 = 55,
        d8 = 56,
        d9 = 57,

        semicolon = 59, /* ; */
        equal = 61,     /* = */
        questionMark =
            63, /* ? (Note: usually shift+slash, but kept if needed) */

        a = 65,
        b = 66,
        c = 67,
        d = 68,
        e = 69,
        f = 70,
        g = 71,
        h = 72,
        i = 73,
        j = 74,
        k = 75,
        l = 76,
        m = 77,
        n = 78,
        o = 79,
        p = 80,
        q = 81,
        r = 82,
        s = 83,
        t = 84,
        u = 85,
        v = 86,
        w = 87,
        x = 88,
        y = 89,
        z = 90,

        leftBracket = 91,  /* [ */
        backslash = 92,    /* \ */
        rightBracket = 93, /* ] */
        graveAccent = 96,  /* ` */

        // Function keys
        escape = 256,
        enter = 257,
        tab = 258,       // Fixed conflict
        backspace = 259, // Fixed conflict
        insert = 260,
        del = 261,

        // Movement & Navigation
        arrowRight = 262,
        arrowLeft = 263,
        arrowDown = 264,
        arrowUp = 265,
        pageUp = 266,   // Added
        pageDown = 267, // Added
        home = 268,     // Added
        end = 269,      // Added

        // Lock & System keys
        capsLock = 280,
        scrollLock = 281,
        numLock = 282,
        printScreen = 283,
        pause = 284, // Added

        // F keys
        f1 = 290,
        f2 = 291,
        f3 = 292,
        f4 = 293,
        f5 = 294,
        f6 = 295,
        f7 = 296,
        f8 = 297,
        f9 = 298,
        f10 = 299,
        f11 = 300,
        f12 = 301,

        // Modifiers
        leftShift = 340,
        leftControl = 341,
        leftAlt = 342,
        leftSuper = 343, // Added (Windows/Command key)
        rightShift = 344,
        rightControl = 345,
        rightAlt = 346,
        rightSuper = 347, // Added
        menu = 348        // Added
    };
} // namespace Bess
