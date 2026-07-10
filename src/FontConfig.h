#pragma once
#include "imgui/imgui.h"

namespace ItemSearch
{
    // Glyph ranges for the embedded Menomonia: without an explicit range ImGui
    // only bakes Basic Latin + Latin-1, and GW2 texts render "?" for
    // typographic quotes/dashes and Latin-Extended characters in names.
    inline const ImWchar* FontGlyphRanges()
    {
        static const ImWchar ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
            0x0100, 0x017F, // Latin Extended-A (accented character names)
            0x2010, 0x2027, // dashes, typographic quotes, bullet, ellipsis...
            0x2030, 0x203A, // permille, prime, single guillemets
            0,
        };
        return ranges;
    }

    // ImFontConfig passed to Nexus' font API (as void*).
    inline void* FontLoadConfig()
    {
        static ImFontConfig cfg = []
        {
            ImFontConfig c;
            c.GlyphRanges = FontGlyphRanges();
            // Snap glyph advances to whole pixels: together with ImGui's
            // floored text origin every glyph lands on the pixel grid instead
            // of being bilinearly smeared at fractional x offsets.
            c.PixelSnapH  = true;
            return c;
        }();
        return &cfg;
    }
}
