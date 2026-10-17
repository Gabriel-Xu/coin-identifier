// Shared code for both coin detectors: coin types, scale-invariant
// classification, result reporting, and a small command-line parser.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace coin
{

// ---------------------------------------------------------------- coin types

enum class Type
{
    Penny = 0,
    Nickel,
    Dime,
    Quarter,
    HalfDollar,
    Dollar,
    Count
};
constexpr int kNumTypes = static_cast<int>(Type::Count);

struct TypeInfo
{
    const char *name;
    const char *plural;
    int cents;
    std::array<unsigned char, 3> rgb; // overlay colour
};

inline const TypeInfo &info(Type t)
{
    static const std::array<TypeInfo, kNumTypes> table = {{
        {"penny", "pennies", 1, {255, 0, 0}},
        {"nickel", "nickels", 5, {255, 255, 0}},
        {"dime", "dimes", 10, {0, 0, 255}},
        {"quarter", "quarters", 25, {255, 0, 255}},
        {"half dollar", "half dollars", 50, {0, 255, 0}},
        {"dollar coin", "dollar coins", 100, {255, 105, 180}},
    }};
    return table[static_cast<int>(t)];
}

// Pennies and golden dollars are "warm" (copper/brass), everything else is
// silver-coloured. Each (type, colour) pair has a known diameter relative to
// a penny (19.05 mm). Silver dollars are Eisenhower dollars (38.1 mm).
enum class Colour
{
    Silver = 0,
    Warm
};

struct SizeRatio
{
    Type type;
    Colour colour;
    double ratio; // diameter / penny diameter
};

inline const std::vector<SizeRatio> &sizeRatios()
{
    static const std::vector<SizeRatio> r = {
        {Type::Penny, Colour::Warm, 19.05 / 19.05},
        {Type::Dollar, Colour::Warm, 26.49 / 19.05},
        {Type::Dime, Colour::Silver, 17.91 / 19.05},
        {Type::Nickel, Colour::Silver, 21.21 / 19.05},
        {Type::Quarter, Colour::Silver, 24.26 / 19.05},
        {Type::HalfDollar, Colour::Silver, 30.61 / 19.05},
        {Type::Dollar, Colour::Silver, 38.10 / 19.05},
    };
    return r;
}

// ------------------------------------------------------------------ detections

struct Detection
{
    double cx = 0, cy = 0, r = 0; // full-resolution pixels
    double chroma = 0;            // red-minus-green feature, see classifyColour
    double meanR = 0, meanG = 0, meanB = 0; // mean colour of the coin centre
    double support = 0;           // detector confidence, 0..1
    Colour colour = Colour::Silver;
    Type type = Type::Penny;
};

// Colour features of a coin's centre, each normalised by the brightest
// channel so dark and bright coins compare alike. Copper shows up as a red
// excess over green, which warm lighting barely affects; brass (golden
// dollars) shows up as red over blue once the image tint is removed.
inline double redMinusGreen(double meanR, double meanG, double meanB)
{
    return (meanR - meanG) / std::max({meanR, meanG, meanB, 1.0});
}
inline double redMinusBlue(double meanR, double meanG, double meanB)
{
    return (meanR - meanB) / std::max({meanR, meanG, meanB, 1.0});
}

struct ColourThresholds
{
    double copper = 0.06; // red-minus-green at or above this is copper
    double gold = 0.15;   // tint-corrected red-minus-blue at or above this is brass
};

// Label each detection Silver or Warm. The silver coins double as a neutral
// reference: their median red-minus-blue is the scene tint, which is removed
// before testing for brass.
inline void classifyColour(std::vector<Detection> &dets, ColourThresholds th)
{
    std::vector<double> silverRB;
    for (auto &d : dets)
    {
        d.chroma = redMinusGreen(d.meanR, d.meanG, d.meanB);
        if (d.chroma < th.copper)
            silverRB.push_back(redMinusBlue(d.meanR, d.meanG, d.meanB));
    }
    double tint = 0;
    if (silverRB.size() >= 3)
    {
        std::sort(silverRB.begin(), silverRB.end());
        tint = silverRB[silverRB.size() / 2];
    }
    for (auto &d : dets)
    {
        bool copper = d.chroma >= th.copper;
        bool brass = redMinusBlue(d.meanR, d.meanG, d.meanB) - tint >= th.gold;
        d.colour = (copper || brass) ? Colour::Warm : Colour::Silver;
    }
}

// Find the penny radius (in pixels) that best explains all detected radii
// given the known size ratios. Each coin is matched to the nearest allowed
// ratio for its colour; residuals are capped so false detections cannot pull
// the fit. Returns 0 if there is nothing to fit.
inline double fitPennyRadius(const std::vector<Detection> &dets)
{
    if (dets.empty())
        return 0;
    double rMin = 1e9, rMax = 0;
    for (const auto &d : dets)
    {
        rMin = std::min(rMin, d.r);
        rMax = std::max(rMax, d.r);
    }
    const double cap = 0.2 * 0.2; // residuals beyond 20% count as outliers
    double bestS = rMin, bestCost = 1e300;
    for (double s = rMin / 2.1; s <= rMax / 0.9; s *= 1.0025)
    {
        double cost = 0;
        for (const auto &d : dets)
        {
            double best = cap;
            for (const auto &sr : sizeRatios())
            {
                if (sr.colour != d.colour)
                    continue;
                double e = d.r / (sr.ratio * s) - 1.0;
                best = std::min(best, e * e);
            }
            cost += best;
        }
        if (cost < bestCost)
        {
            bestCost = cost;
            bestS = s;
        }
    }
    return bestS;
}

inline void assignTypes(std::vector<Detection> &dets, double pennyRadius)
{
    for (auto &d : dets)
    {
        double bestErr = 1e300;
        for (const auto &sr : sizeRatios())
        {
            if (sr.colour != d.colour)
                continue;
            double e = std::fabs(d.r / (sr.ratio * pennyRadius) - 1.0);
            if (e < bestErr)
            {
                bestErr = e;
                d.type = sr.type;
            }
        }
    }
}

// Assign a colour class and coin type to each detection. If pennyRadius is 0
// the reference scale is fitted from the data, then re-estimated from the
// coins labelled penny (the most reliable anchor) and types are reassigned.
// Returns the reference used.
inline double classify(std::vector<Detection> &dets, double pennyRadius, ColourThresholds th)
{
    classifyColour(dets, th);
    if (dets.empty())
        return pennyRadius;
    if (pennyRadius <= 0)
    {
        pennyRadius = fitPennyRadius(dets);
        assignTypes(dets, pennyRadius);
        std::vector<double> pennies;
        for (const auto &d : dets)
            if (d.type == Type::Penny)
                pennies.push_back(d.r);
        if (pennies.size() >= 3)
        {
            std::sort(pennies.begin(), pennies.end());
            pennyRadius = pennies[pennies.size() / 2];
        }
    }
    assignTypes(dets, pennyRadius);
    return pennyRadius;
}

// ------------------------------------------------------------------ reporting

inline std::array<int, kNumTypes> countTypes(const std::vector<Detection> &dets)
{
    std::array<int, kNumTypes> c{};
    for (const auto &d : dets)
        c[static_cast<int>(d.type)]++;
    return c;
}

inline int totalCents(const std::vector<Detection> &dets)
{
    int total = 0;
    for (const auto &d : dets)
        total += info(d.type).cents;
    return total;
}

inline void writeResults(std::ostream &os, const std::vector<Detection> &dets, double pennyRadius)
{
    auto counts = countTypes(dets);
    for (int i = 0; i < kNumTypes; i++)
        os << counts[i] << " " << info(static_cast<Type>(i)).plural << "\n";
    int total = totalCents(dets);
    os << "Detected " << dets.size() << " coins (penny reference radius "
       << std::fixed << std::setprecision(1) << pennyRadius << " px)\n";
    os << "Total sum: $" << total / 100 << "." << std::setw(2) << std::setfill('0') << total % 100
       << std::setfill(' ') << "\n";
}

// One line per coin (full-resolution pixels). The type name has spaces
// replaced by underscores so the file splits cleanly on whitespace.
inline void writeCircles(std::ostream &os, const std::vector<Detection> &dets)
{
    os << "# x y r type chroma support meanR meanG meanB\n";
    for (const auto &d : dets)
    {
        std::string name = info(d.type).name;
        std::replace(name.begin(), name.end(), ' ', '_');
        os << std::fixed << std::setprecision(1) << d.cx << " " << d.cy << " " << d.r << " " << name << " "
           << std::setprecision(3) << d.chroma << " " << d.support << " " << std::setprecision(1) << d.meanR
           << " " << d.meanG << " " << d.meanB << "\n";
    }
}

// --------------------------------------------------------------- command line

struct Option
{
    std::string flag;
    std::string def; // default shown in help ("" = none)
    std::string help;
};

class Args
{
public:
    Args(int argc, char **argv, const std::vector<Option> &opts) : opts_(opts)
    {
        prog_ = argv[0];
        for (int i = 1; i < argc; i++)
        {
            std::string s = argv[i];
            if (s == "-h" || s == "--help")
            {
                usage(stdout);
                std::exit(0);
            }
            auto it = std::find_if(opts.begin(), opts.end(), [&](const Option &o) { return o.flag == s; });
            if (it == opts.end())
            {
                std::fprintf(stderr, "Unknown option '%s'\n", s.c_str());
                usage(stderr);
                std::exit(2);
            }
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Option '%s' needs a value\n", s.c_str());
                std::exit(2);
            }
            values_[s] = argv[++i];
        }
    }

    bool has(const std::string &flag) const { return values_.count(flag) > 0; }

    std::string str(const std::string &flag, const std::string &def) const
    {
        auto it = values_.find(flag);
        return it == values_.end() ? def : it->second;
    }

    int integer(const std::string &flag, int def) const
    {
        auto it = values_.find(flag);
        if (it == values_.end())
            return def;
        try
        {
            size_t pos;
            int v = std::stoi(it->second, &pos);
            if (pos != it->second.size())
                throw std::invalid_argument("trailing characters");
            return v;
        }
        catch (const std::exception &)
        {
            std::fprintf(stderr, "Option '%s' expects an integer, got '%s'\n", flag.c_str(), it->second.c_str());
            std::exit(2);
        }
    }

    double real(const std::string &flag, double def) const
    {
        auto it = values_.find(flag);
        if (it == values_.end())
            return def;
        try
        {
            size_t pos;
            double v = std::stod(it->second, &pos);
            if (pos != it->second.size())
                throw std::invalid_argument("trailing characters");
            return v;
        }
        catch (const std::exception &)
        {
            std::fprintf(stderr, "Option '%s' expects a number, got '%s'\n", flag.c_str(), it->second.c_str());
            std::exit(2);
        }
    }

    bool flagOn(const std::string &flag) const
    {
        std::string v = str(flag, "0");
        return v == "1" || v == "true" || v == "yes" || v == "on";
    }

    void usage(FILE *out) const
    {
        std::fprintf(out, "Usage: %s [options]\n", prog_.c_str());
        for (const auto &o : opts_)
        {
            std::string left = o.flag + " <v>";
            std::fprintf(out, "  %-14s %s", left.c_str(), o.help.c_str());
            if (!o.def.empty())
                std::fprintf(out, " [default: %s]", o.def.c_str());
            std::fprintf(out, "\n");
        }
    }

private:
    std::string prog_;
    std::vector<Option> opts_;
    std::map<std::string, std::string> values_;
};

// Downscale factor so that the shorter image side is roughly `target` pixels.
inline int autoDownscale(int w, int h, int target = 800)
{
    int ds = static_cast<int>(std::lround(static_cast<double>(std::min(w, h)) / target));
    return std::max(1, std::min(ds, 8));
}

} // namespace coin
