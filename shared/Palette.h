#pragma once
// Charte de couleurs du light guide Komplete Kontrol MK2, partagee par l'extension et le plugin.
//
// Index de couleur envoye au clavier = teinte * 4 + nuance
//   teinte : 1..16 (couleurs) et 17 (blanc)
//   nuance : 0 = sombre, 1 = moyen, 2 = vif (par defaut), 3 = clair
// 0 = eteint.
//
// Noms reconnus (sans tenir compte des majuscules ni des accents) :
//   « bleu », « bleu clair », « bleu sombre », « blue light », « bleu 3 »...
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

namespace palette
{
struct Hue
{
    int index;
    const char *fr; // nom principal en francais (sans accent dans le code)
    const char *en; // nom en anglais
    uint32_t rgb;   // couleur approximative a l'ecran (nuance claire)
};

constexpr Hue kHues[] = {
    {1, "rouge", "red", 0xFF2A2A},       {2, "orange", "orange", 0xFF7A1A},
    {3, "ambre", "amber", 0xFFA040},     {4, "dore", "gold", 0xFFC030},
    {5, "jaune", "yellow", 0xFFEE22},    {6, "lime", "lime", 0xA8F020},
    {7, "vert", "green", 0x30E040},      {8, "menthe", "mint", 0x30F0A0},
    {9, "cyan", "cyan", 0x20E8E8},       {10, "turquoise", "turquoise", 0x20C0F0},
    {11, "bleu", "blue", 0x2A60FF},      {12, "prune", "plum", 0x8050FF},
    {13, "violet", "violet", 0xA040FF},  {14, "pourpre", "purple", 0xD030FF},
    {15, "magenta", "magenta", 0xFF30D0}, {16, "fuchsia", "fuchsia", 0xFF4090},
    {17, "blanc", "white", 0xFFFFFF},
};
constexpr int kHueCount = 17;

// Autres mots acceptes pour une teinte
struct Alias
{
    const char *word;
    int hue;
};
constexpr Alias kAliases[] = {{"rose", 16}, {"pink", 16}, {"mauve", 13}, {"indigo", 12}, {"aqua", 9},
                              {"marine", 11}};

// Mots de nuance, dans l'ordre 0..3
constexpr const char *kShadeFr[] = {"sombre", "moyen", "vif", "clair"};
constexpr const char *kShadeEn[] = {"dark", "medium", "bright", "light"};
constexpr int kDefaultShade = 2;

constexpr int colorIndex(int hue, int shade)
{
    return hue <= 0 ? 0 : (hue > 17 ? 17 : hue) * 4 + (shade < 0 ? 0 : (shade > 3 ? 3 : shade));
}

// Nom lisible d'un index : « bleu clair », « rouge » (nuance par defaut), « eteint »
inline std::string nameFr(int idx)
{
    if (idx < 4)
        return idx == 0 ? "eteint" : "transparent";
    const int hue = idx / 4, shade = idx % 4;
    if (hue < 1 || hue > kHueCount)
        return "?";
    std::string s = kHues[hue - 1].fr;
    if (shade != kDefaultShade)
        s += std::string(" ") + kShadeFr[shade];
    return s;
}

// Comme nameFr, avec les accents (UTF-8), pour l'affichage dans le plugin
inline std::string labelFr(int idx)
{
    std::string s = nameFr(idx);
    if (s.rfind("dore", 0) == 0)
        s.replace(0, 4, "dor\xC3\xA9");
    if (s == "eteint")
        s = "\xC3\xA9teint";
    return s;
}

// Minuscules + accents francais retires (entree en UTF-8)
inline std::string fold(const std::string &in)
{
    std::string out;
    for (size_t i = 0; i < in.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if (c == 0xC3 && i + 1 < in.size())
        {
            const unsigned char d = static_cast<unsigned char>(in[++i]) | 0x20; // majuscule -> minuscule
            if (d >= 0xA0 && d <= 0xA5) out += 'a';
            else if (d == 0xA7) out += 'c';
            else if (d >= 0xA8 && d <= 0xAB) out += 'e';
            else if (d >= 0xAC && d <= 0xAF) out += 'i';
            else if (d >= 0xB2 && d <= 0xB6) out += 'o';
            else if (d >= 0xB9 && d <= 0xBC) out += 'u';
            else out += ' ';
        }
        else
            out += static_cast<char>(std::tolower(c));
    }
    return out;
}

namespace detail
{
inline bool isWordChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

// Cherche `word` comme mot entier dans `t` a partir de `from` ; renvoie la position ou npos.
inline size_t findWord(const std::string &t, const std::string &word, size_t from = 0)
{
    for (size_t p = t.find(word, from); p != std::string::npos; p = t.find(word, p + 1))
    {
        const bool startOk = p == 0 || !isWordChar(t[p - 1]);
        const bool endOk = p + word.size() >= t.size() || !isWordChar(t[p + word.size()]);
        if (startOk && endOk)
            return p;
    }
    return std::string::npos;
}

// Nuance donnee juste apres la teinte (« clair », « light », « 0 ».. « 3 »), ou la nuance par defaut.
inline int shadeAfter(const std::string &t, size_t pos)
{
    while (pos < t.size() && !isWordChar(t[pos]))
        ++pos;
    size_t end = pos;
    while (end < t.size() && isWordChar(t[end]))
        ++end;
    const std::string w = t.substr(pos, end - pos);
    if (w.size() == 1 && w[0] >= '0' && w[0] <= '3')
        return w[0] - '0';
    for (int s = 0; s < 4; ++s)
        if (w == kShadeFr[s] || w == kShadeEn[s])
            return s;
    if (w == "fonce" || w == "deep")
        return 0;
    if (w == "pale" || w == "pastel")
        return 3;
    return kDefaultShade;
}
} // namespace detail

// Trouve une couleur nommee dans un texte libre (nom de bloc...). Renvoie l'index, ou -1.
inline int findColor(const std::string &text)
{
    const std::string t = fold(text);
    size_t bestPos = std::string::npos, bestLen = 0;
    int bestHue = -1;
    auto consider = [&](const char *word, int hue) {
        const size_t p = detail::findWord(t, word);
        const size_t len = std::strlen(word);
        if (p != std::string::npos && (bestHue < 0 || p < bestPos || (p == bestPos && len > bestLen)))
            bestPos = p, bestLen = len, bestHue = hue;
    };
    for (const auto &h : kHues)
    {
        consider(h.fr, h.index);
        consider(h.en, h.index);
    }
    for (const auto &a : kAliases)
        consider(a.word, a.hue);
    if (bestHue < 0)
        return -1;
    return colorIndex(bestHue, detail::shadeAfter(t, bestPos + bestLen));
}
} // namespace palette
