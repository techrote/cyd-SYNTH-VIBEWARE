#pragma once
#include "core.hpp"
namespace xenobio {
// Compact original 3x5 uppercase bitmap, five octal row digits per glyph.
constexpr unsigned glyph(char c) {
    switch (c) {
    case 'A': return 025755; case 'B': return 065656; case 'C': return 034443;
    case 'D': return 065556; case 'E': return 074647; case 'F': return 074644;
    case 'G': return 034553; case 'H': return 055755; case 'I': return 072227;
    case 'J': return 011552; case 'K': return 055655; case 'L': return 044447;
    case 'M': return 057555; case 'N': return 057755; case 'O': return 025552;
    case 'P': return 065644; case 'Q': return 025573; case 'R': return 065655;
    case 'S': return 034216; case 'T': return 072222; case 'U': return 055557;
    case 'V': return 055552; case 'W': return 055755; case 'X': return 055255;
    case 'Y': return 055222; case 'Z': return 071247; default: return 0;
    }
}
inline void text_row(int y, int top, int left, int right, const char* s, uint16_t* out) {
    if (y < top || y >= top + 10) return;
    int len = 0; while (s[len]) ++len;
    int x = left + (right - left - (8 * len - 2)) / 2;
    for (int i = 0; i < len; ++i, x += 8) {
        const auto bits = (glyph(s[i]) >> (3 * (4 - ((y - top)/2)))) & 7u;
        for (int j = 0; j < 6; ++j) if ((bits & (4u >> (j/2))) && x+j >= left && x+j < right && x+j >= 0 && x+j < 320)
            out[x+j] = 0xffff;
    }
}
inline void labeled_row(const Engine& e, int y, uint16_t* out) {
    if (y < 0 || y >= 240) return;
    e.render(y, out);
    constexpr const char* names[] = {"SPARSE", "DENSE", "BAL", "CYCLE", "DIST", "RECV"};
    for (int i = 0; i < 6; ++i) text_row(y, 5, (i*320+5)/6, ((i+1)*320+5)/6, names[i], out);
    constexpr const char* modes[] = {"FEED", "ATTRACT", "DISTURB"};
    for (int i = 0; i < 3; ++i) text_row(y, 225, i*80, (i+1)*80, modes[i], out);
    text_row(y, 225, 240, 280, "RESET", out);
    text_row(y, 225, 280, 320, "PANIC", out);
}
}
