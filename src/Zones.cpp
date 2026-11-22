#include "Zones.h"
#include "Palette.h"

#include <algorithm>
#include <cctype>


namespace zones
{
uint8_t paletteColor(int hue, int shade)
{
    if (hue <= 0)
        return 0;
    return static_cast<uint8_t>(std::clamp(hue, 1, 17) * 4 + std::clamp(shade, 0, 3));
}

int colorFromText(const std::string &text)
{
    return palette::findColor(text);
}

int parseNote(const std::string &raw)
{
    std::string t;
    for (char c : raw)
        if (!std::isspace(static_cast<unsigned char>(c)))
            t += c;
    if (t.empty())
        return -1;

    if (std::isdigit(static_cast<unsigned char>(t[0])))
    {
        int v = 0;
        for (char c : t)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                break;
            v = v * 10 + (c - '0');
        }
        return v <= 127 ? v : -1;
    }

    static const int kPc[] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(t[0])));
    if (letter < 'A' || letter > 'G')
        return -1;
    int pc = kPc[letter - 'A'];
    size_t i = 1;
    if (i < t.size() && (t[i] == '#' || t[i] == 's'))
        ++pc, ++i;
    else if (i < t.size() && t[i] == 'b')
        --pc, ++i;
    if (i >= t.size())
        return -1;
    bool neg = false;
    if (t[i] == '-')
        neg = true, ++i;
    if (i >= t.size() || !std::isdigit(static_cast<unsigned char>(t[i])))
        return -1;
    int oct = t[i] - '0';
    if (neg)
        oct = -oct;
    const int note = (oct + 2) * 12 + pc; // GP : C3 = 60, C-2 = 0
    return (note >= 0 && note <= 127) ? note : -1;
}

std::array<uint8_t, 128> render(const std::vector<Zone> &zs, bool layersInWhite)
{
    std::array<uint8_t, 128> out{};
    std::array<int, 128> count{};
    for (const auto &z : zs)
        for (int n = std::max(0, z.low); n <= std::min(127, z.high); ++n)
        {
            ++count[n];
            out[n] = z.color;
        }
    if (layersInWhite)
        for (int n = 0; n < 128; ++n)
            if (count[n] > 1)
                out[n] = paletteColor(17, 2);
    return out;
}
} // namespace zones
