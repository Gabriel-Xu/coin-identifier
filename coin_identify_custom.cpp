// From-scratch coin detector: box downscale -> Gaussian blur -> Sobel ->
// Canny (NMS, then hysteresis) -> gradient-direction Hough voting over the
// plausible radius band -> accumulator non-maximum suppression -> radius fit
// from radially aligned edge pixels -> scale-invariant classification.
#include "coin_common.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

namespace
{

constexpr double kPi = 3.14159265358979323846;

// --------------------------------------------------------------- image types

// Row-major image, pixel (x, y) at index y * w + x; (x, y) = (column, row).
template <typename T>
struct Image
{
    int w = 0, h = 0, ch = 1;
    vector<T> px;
    Image() = default;
    Image(int w_, int h_, int ch_ = 1, T fill = T()) : w(w_), h(h_), ch(ch_), px((size_t)w_ * h_ * ch_, fill) {}
    T &at(int x, int y, int c = 0) { return px[((size_t)y * w + x) * ch + c]; }
    const T &at(int x, int y, int c = 0) const { return px[((size_t)y * w + x) * ch + c]; }
    bool inside(int x, int y) const { return x >= 0 && x < w && y >= 0 && y < h; }
};

using Image8 = Image<uint8_t>;
using Image16 = Image<int16_t>;
using Image32 = Image<int32_t>;

// ------------------------------------------------------------------ PPM I/O

// Reads a P3 (text) or P6 (binary) PPM with maxval <= 255.
Image8 readPPM(const string &path)
{
    ifstream in(path, ios::binary);
    if (!in)
        throw runtime_error("could not open " + path);
    vector<char> buf((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    size_t pos = 0, n = buf.size();

    auto skipSpace = [&]() {
        while (pos < n)
        {
            if (buf[pos] == '#')
                while (pos < n && buf[pos] != '\n')
                    pos++;
            else if (isspace((unsigned char)buf[pos]))
                pos++;
            else
                break;
        }
    };
    auto token = [&]() {
        skipSpace();
        size_t start = pos;
        while (pos < n && !isspace((unsigned char)buf[pos]))
            pos++;
        return string(buf.begin() + start, buf.begin() + pos);
    };
    auto number = [&]() {
        skipSpace();
        if (pos >= n || !isdigit((unsigned char)buf[pos]))
            throw runtime_error("malformed PPM header/data in " + path);
        int v = 0;
        while (pos < n && isdigit((unsigned char)buf[pos]))
            v = v * 10 + (buf[pos++] - '0');
        return v;
    };

    string magic = token();
    if (magic != "P3" && magic != "P6")
        throw runtime_error(path + ": unsupported format '" + magic + "' (need P3 or P6)");
    int w = number(), h = number(), maxval = number();
    if (w <= 0 || h <= 0 || maxval <= 0 || maxval > 255)
        throw runtime_error(path + ": unsupported dimensions or maxval");

    Image8 img(w, h, 3);
    size_t count = (size_t)w * h * 3;
    if (magic == "P6")
    {
        pos++; // single whitespace after maxval
        if (n - pos < count)
            throw runtime_error(path + ": truncated pixel data");
        memcpy(img.px.data(), buf.data() + pos, count);
    }
    else
    {
        for (size_t i = 0; i < count; i++)
            img.px[i] = (uint8_t)number();
    }
    return img;
}

void writePPM(const string &path, const Image8 &img)
{
    ofstream out(path, ios::binary);
    if (!out)
        throw runtime_error("could not write " + path);
    out << "P6\n" << img.w << " " << img.h << "\n255\n";
    if (img.ch == 3)
        out.write((const char *)img.px.data(), (streamsize)img.px.size());
    else
        for (uint8_t v : img.px)
            out.put((char)v).put((char)v).put((char)v);
}

// ---------------------------------------------------------------- processing

Image8 downscale(const Image8 &src, int ds)
{
    if (ds == 1)
        return src;
    Image8 dst(src.w / ds, src.h / ds, src.ch);
    const int area = ds * ds;
    for (int y = 0; y < dst.h; y++)
        for (int x = 0; x < dst.w; x++)
            for (int c = 0; c < src.ch; c++)
            {
                int sum = 0;
                for (int dy = 0; dy < ds; dy++)
                    for (int dx = 0; dx < ds; dx++)
                        sum += src.at(x * ds + dx, y * ds + dy, c);
                dst.at(x, y, c) = (uint8_t)((sum + area / 2) / area);
            }
    return dst;
}

Image8 toGray(const Image8 &rgb)
{
    Image8 g(rgb.w, rgb.h, 1);
    for (size_t i = 0, n = (size_t)rgb.w * rgb.h; i < n; i++)
        g.px[i] = (uint8_t)((77 * rgb.px[3 * i] + 150 * rgb.px[3 * i + 1] + 29 * rgb.px[3 * i + 2] + 128) >> 8);
    return g;
}

// Separable 5-tap binomial blur (sigma ~1.0), edges clamped.
Image8 blur5(const Image8 &src)
{
    static const int k[5] = {1, 4, 6, 4, 1};
    Image<int> tmp(src.w, src.h);
    for (int y = 0; y < src.h; y++)
        for (int x = 0; x < src.w; x++)
        {
            int s = 0;
            for (int i = -2; i <= 2; i++)
                s += k[i + 2] * src.at(clamp(x + i, 0, src.w - 1), y);
            tmp.at(x, y) = s;
        }
    Image8 dst(src.w, src.h);
    for (int y = 0; y < src.h; y++)
        for (int x = 0; x < src.w; x++)
        {
            int s = 0;
            for (int i = -2; i <= 2; i++)
                s += k[i + 2] * tmp.at(x, clamp(y + i, 0, src.h - 1));
            dst.at(x, y) = (uint8_t)((s + 128) >> 8);
        }
    return dst;
}

void sobel(const Image8 &g, Image16 &gx, Image16 &gy, Image32 &mag2)
{
    gx = Image16(g.w, g.h);
    gy = Image16(g.w, g.h);
    mag2 = Image32(g.w, g.h);
    for (int y = 1; y < g.h - 1; y++)
        for (int x = 1; x < g.w - 1; x++)
        {
            int dx = 2 * (g.at(x + 1, y) - g.at(x - 1, y)) + g.at(x + 1, y - 1) - g.at(x - 1, y - 1) +
                     g.at(x + 1, y + 1) - g.at(x - 1, y + 1);
            int dy = 2 * (g.at(x, y + 1) - g.at(x, y - 1)) + g.at(x + 1, y + 1) - g.at(x + 1, y - 1) +
                     g.at(x - 1, y + 1) - g.at(x - 1, y - 1);
            gx.at(x, y) = (int16_t)dx;
            gy.at(x, y) = (int16_t)dy;
            mag2.at(x, y) = dx * dx + dy * dy;
        }
}

// Otsu threshold on the gradient magnitude (not squared), ignoring the
// 1-pixel border. Gives a data-driven Canny high threshold.
double otsuMagnitude(const Image32 &mag2)
{
    const int bins = 1450; // sqrt(2) * 1020 max for a 3x3 Sobel on 8-bit data
    vector<double> hist(bins, 0);
    double total = 0;
    for (int y = 1; y < mag2.h - 1; y++)
        for (int x = 1; x < mag2.w - 1; x++)
        {
            int m = min(bins - 1, (int)lround(sqrt((double)mag2.at(x, y))));
            hist[m] += 1;
            total += 1;
        }
    double sumAll = 0;
    for (int i = 0; i < bins; i++)
        sumAll += i * hist[i];
    double wB = 0, sumB = 0, best = -1;
    int thr = 0;
    for (int i = 0; i < bins; i++)
    {
        wB += hist[i];
        if (wB == 0)
            continue;
        double wF = total - wB;
        if (wF == 0)
            break;
        sumB += i * hist[i];
        double mB = sumB / wB, mF = (sumAll - sumB) / wF;
        double between = wB * wF * (mB - mF) * (mB - mF);
        if (between > best)
        {
            best = between;
            thr = i;
        }
    }
    return thr;
}

// Canny: non-maximum suppression along the quantised gradient direction,
// then hysteresis with an explicit stack. Returns a 0/1 edge mask.
Image8 canny(const Image16 &gx, const Image16 &gy, const Image32 &mag2, double low, double high)
{
    const int w = gx.w, h = gx.h;
    const int64_t low2 = (int64_t)(low * low), high2 = (int64_t)(high * high);
    Image8 label(w, h, 1, 0); // 0 none, 1 weak, 2 strong (NMS survivors only)
    vector<pair<int, int>> stack;
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
        {
            int64_t m = mag2.at(x, y);
            if (m < low2)
                continue;
            int ax = abs(gx.at(x, y)), ay = abs(gy.at(x, y));
            int n1, n2;
            // tan(22.5 deg) ~= 0.4142 ~= 5/12
            if (ay * 12 <= ax * 5)
            { // gradient ~horizontal
                n1 = mag2.at(x + 1, y);
                n2 = mag2.at(x - 1, y);
            }
            else if (ax * 12 <= ay * 5)
            { // gradient ~vertical
                n1 = mag2.at(x, y + 1);
                n2 = mag2.at(x, y - 1);
            }
            else if ((gx.at(x, y) > 0) == (gy.at(x, y) > 0))
            { // diagonal, same sign
                n1 = mag2.at(x + 1, y + 1);
                n2 = mag2.at(x - 1, y - 1);
            }
            else
            {
                n1 = mag2.at(x + 1, y - 1);
                n2 = mag2.at(x - 1, y + 1);
            }
            if (m >= n1 && m > n2)
            {
                label.at(x, y) = m >= high2 ? 2 : 1;
                if (m >= high2)
                    stack.emplace_back(x, y);
            }
        }
    Image8 edge(w, h, 1, 0);
    while (!stack.empty())
    {
        auto [x, y] = stack.back();
        stack.pop_back();
        if (edge.at(x, y))
            continue;
        edge.at(x, y) = 1;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            {
                int nx = x + dx, ny = y + dy;
                if (edge.inside(nx, ny) && label.at(nx, ny) && !edge.at(nx, ny))
                    stack.emplace_back(nx, ny);
            }
    }
    return edge;
}

// Every edge pixel votes along its gradient direction, both ways, only at
// distances in [rmin, rmax]. Returns the accumulator summed over 3x3 windows.
Image32 houghVote(const Image8 &edge, const Image16 &gx, const Image16 &gy, int rmin, int rmax)
{
    Image32 acc(edge.w, edge.h);
    for (int y = 0; y < edge.h; y++)
        for (int x = 0; x < edge.w; x++)
        {
            if (!edge.at(x, y))
                continue;
            double g = hypot((double)gx.at(x, y), (double)gy.at(x, y));
            if (g == 0)
                continue;
            double ux = gx.at(x, y) / g, uy = gy.at(x, y) / g;
            for (int sgn = -1; sgn <= 1; sgn += 2)
                for (int d = rmin; d <= rmax; d++)
                {
                    int cx = (int)lround(x + sgn * d * ux), cy = (int)lround(y + sgn * d * uy);
                    if (acc.inside(cx, cy))
                        acc.at(cx, cy)++;
                }
        }
    Image32 sum(acc.w, acc.h);
    for (int y = 1; y < acc.h - 1; y++)
        for (int x = 1; x < acc.w - 1; x++)
        {
            int s = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    s += acc.at(x + dx, y + dy);
            sum.at(x, y) = s;
        }
    return sum;
}

struct Candidate
{
    int x, y, votes;
};

// Local maxima of the accumulator above `minVotes` within a square window.
vector<Candidate> findCenters(const Image32 &acc, int minVotes, int window)
{
    vector<Candidate> out;
    for (int y = 0; y < acc.h; y++)
        for (int x = 0; x < acc.w; x++)
        {
            int v = acc.at(x, y);
            if (v < minVotes)
                continue;
            bool isMax = true;
            for (int dy = -window; dy <= window && isMax; dy++)
                for (int dx = -window; dx <= window; dx++)
                {
                    int nx = x + dx, ny = y + dy;
                    if (!acc.inside(nx, ny) || (dx == 0 && dy == 0))
                        continue;
                    int nv = acc.at(nx, ny);
                    if (nv > v || (nv == v && (dy < 0 || (dy == 0 && dx < 0))))
                    {
                        isMax = false;
                        break;
                    }
                }
            if (isMax)
                out.push_back({x, y, v});
        }
    return out;
}

struct Fit
{
    double r = 0, support = 0;
};

// Histogram the distance of edge pixels whose gradient points at (or away
// from) the centre; engraving edges fail that test, the rim passes. Support is
// the fraction of the circumference covered by a 3-bin window around r.
Fit fitRadius(const Image8 &edge, const Image16 &gx, const Image16 &gy, int cx, int cy, int rmin, int rmax,
              double cosTol)
{
    vector<int> hist(rmax + 3, 0);
    const double dmin = rmin - 1.5, dmax = rmax + 1.5;
    for (int y = max(0, cy - rmax - 2); y <= min(edge.h - 1, cy + rmax + 2); y++)
        for (int x = max(0, cx - rmax - 2); x <= min(edge.w - 1, cx + rmax + 2); x++)
        {
            if (!edge.at(x, y))
                continue;
            double dx = x - cx, dy = y - cy;
            double d = sqrt(dx * dx + dy * dy);
            if (d < dmin || d > dmax)
                continue;
            double g = hypot((double)gx.at(x, y), (double)gy.at(x, y));
            if (g == 0)
                continue;
            double cosA = fabs(gx.at(x, y) * dx + gy.at(x, y) * dy) / (g * d);
            if (cosA < cosTol)
                continue;
            int bin = (int)lround(d);
            if (bin >= 0 && bin < (int)hist.size())
                hist[bin]++;
        }
    Fit best;
    for (int r = rmin; r <= rmax; r++)
    {
        int n = hist[r - 1] + hist[r] + hist[r + 1];
        double support = n / (2 * kPi * r);
        if (support > best.support)
        {
            // sub-pixel radius: centroid of the 3-bin window
            double num = (r - 1.0) * hist[r - 1] + (double)r * hist[r] + (r + 1.0) * hist[r + 1];
            best = {n ? num / n : r, min(1.0, support)};
        }
    }
    return best;
}

// Mean colour inside the central disk of a coin (radius fraction 0.6).
coin::Detection makeDetection(const Image8 &rgb, double cx, double cy, double r, double support, int ds)
{
    double sum[3] = {0, 0, 0};
    int n = 0;
    int rad = max(1, (int)(r * 0.6));
    for (int y = max(0, (int)cy - rad); y <= min(rgb.h - 1, (int)cy + rad); y++)
        for (int x = max(0, (int)cx - rad); x <= min(rgb.w - 1, (int)cx + rad); x++)
        {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > (double)rad * rad)
                continue;
            for (int c = 0; c < 3; c++)
                sum[c] += rgb.at(x, y, c);
            n++;
        }
    coin::Detection d;
    d.cx = cx * ds;
    d.cy = cy * ds;
    d.r = r * ds;
    d.support = support;
    if (n)
    {
        d.meanR = sum[0] / n;
        d.meanG = sum[1] / n;
        d.meanB = sum[2] / n;
    }
    return d;
}

// Refine centre and radius at full resolution. Around the coarse estimate, a
// gradient-magnitude-weighted histogram of the distance of radially aligned
// pixels is built for a few candidate centres; the strongest 3-bin peak wins.
struct Refined
{
    double cx, cy, r;
};

Refined refineAtFullRes(const Image8 &full, double cx0, double cy0, double r0, int ds, double cosTol,
                        double innerFrac)
{
    const double band = 1.5 * ds + 2;
    const int R = (int)ceil(r0 + band) + 3;
    const int x0 = max(1, (int)floor(cx0) - R), y0 = max(1, (int)floor(cy0) - R);
    const int x1 = min(full.w - 2, (int)ceil(cx0) + R), y1 = min(full.h - 2, (int)ceil(cy0) + R);
    if (x1 - x0 < 8 || y1 - y0 < 8)
        return {cx0, cy0, r0};
    const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;

    // luma, then a 3x3 binomial blur, over the box (coordinates relative to x0, y0)
    Image<int> gray(bw, bh), smooth(bw, bh);
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++)
            gray.at(x, y) = 77 * full.at(x0 + x, y0 + y, 0) + 150 * full.at(x0 + x, y0 + y, 1) +
                            29 * full.at(x0 + x, y0 + y, 2);
    for (int y = 1; y < bh - 1; y++)
        for (int x = 1; x < bw - 1; x++)
            smooth.at(x, y) = gray.at(x - 1, y - 1) + 2 * gray.at(x, y - 1) + gray.at(x + 1, y - 1) +
                              2 * gray.at(x - 1, y) + 4 * gray.at(x, y) + 2 * gray.at(x + 1, y) +
                              gray.at(x - 1, y + 1) + 2 * gray.at(x, y + 1) + gray.at(x + 1, y + 1);
    struct Px
    {
        float x, y, gx, gy, g;
    };
    vector<Px> pts;
    const double dlo = r0 - band - 2, dhi = r0 + band + 2;
    for (int y = 2; y < bh - 2; y++)
        for (int x = 2; x < bw - 2; x++)
        {
            double fx = x0 + x, fy = y0 + y;
            double d = hypot(fx - cx0, fy - cy0);
            if (d < dlo || d > dhi)
                continue;
            int gx = 2 * (smooth.at(x + 1, y) - smooth.at(x - 1, y)) + smooth.at(x + 1, y - 1) -
                     smooth.at(x - 1, y - 1) + smooth.at(x + 1, y + 1) - smooth.at(x - 1, y + 1);
            int gy = 2 * (smooth.at(x, y + 1) - smooth.at(x, y - 1)) + smooth.at(x + 1, y + 1) -
                     smooth.at(x + 1, y - 1) + smooth.at(x - 1, y + 1) - smooth.at(x - 1, y - 1);
            double g = hypot((double)gx, (double)gy);
            if (g > 0)
                pts.push_back({(float)fx, (float)fy, (float)gx, (float)gy, (float)g});
        }

    // For each candidate centre, score every radius in the band. The coin
    // boundary is the innermost ring whose score is close to the best one:
    // a shadow outside the coin can be stronger than the rim itself, but it
    // is never inside it.
    Refined best = {cx0, cy0, r0};
    double bestScore = -1;
    const int rlo = max(1, (int)floor(r0 - band)), rhi = (int)ceil(r0 + band);
    vector<double> hist(rhi + 3, 0), score(rhi + 3, 0);
    for (double oy = -ds / 2.0; oy <= ds / 2.0; oy += max(1.0, ds / 2.0))
        for (double ox = -ds / 2.0; ox <= ds / 2.0; ox += max(1.0, ds / 2.0))
        {
            const double cx = cx0 + ox, cy = cy0 + oy;
            fill(hist.begin(), hist.end(), 0.0);
            for (const Px &p : pts)
            {
                double dx = p.x - cx, dy = p.y - cy;
                double d = sqrt(dx * dx + dy * dy);
                if (d < 1)
                    continue;
                double cosA = fabs(p.gx * dx + p.gy * dy) / (p.g * d);
                if (cosA < cosTol)
                    continue;
                int bin = (int)lround(d);
                if (bin >= 1 && bin < (int)hist.size() - 1)
                    hist[bin] += p.g;
            }
            double peak = 0;
            for (int r = rlo; r <= rhi && r + 1 < (int)hist.size(); r++)
            {
                score[r] = (hist[r - 1] + hist[r] + hist[r + 1]) / r; // weight per unit circumference
                peak = max(peak, score[r]);
            }
            if (peak <= bestScore)
                continue;
            bestScore = peak;
            for (int r = rlo; r <= rhi && r + 1 < (int)hist.size(); r++)
            {
                bool localMax = (r == rlo || score[r] >= score[r - 1]) && (r == rhi || score[r] >= score[r + 1]);
                if (localMax && score[r] >= innerFrac * peak)
                {
                    double n = hist[r - 1] + hist[r] + hist[r + 1];
                    double num = (r - 1.0) * hist[r - 1] + (double)r * hist[r] + (r + 1.0) * hist[r + 1];
                    best = {cx, cy, n > 0 ? num / n : (double)r};
                    break;
                }
            }
        }
    return best;
}

// ------------------------------------------------------------------ drawing

void drawCircle(Image8 &img, int cx, int cy, int radius, array<unsigned char, 3> rgb)
{
    if (radius <= 0)
    {
        if (img.inside(cx, cy))
            for (int c = 0; c < 3; c++)
                img.at(cx, cy, c) = rgb[c];
        return;
    }
    int x = radius, y = 0, err = 1 - radius; // midpoint circle
    auto put = [&](int px, int py) {
        if (img.inside(px, py))
            for (int c = 0; c < 3; c++)
                img.at(px, py, c) = rgb[c];
    };
    while (x >= y)
    {
        put(cx + x, cy + y), put(cx - x, cy + y), put(cx + x, cy - y), put(cx - x, cy - y);
        put(cx + y, cy + x), put(cx - y, cy + x), put(cx + y, cy - x), put(cx - y, cy - x);
        y++;
        if (err < 0)
            err += 2 * y + 1;
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void drawMarker(Image8 &img, int cx, int cy, int r, array<unsigned char, 3> rgb)
{
    for (int k = 0; k <= 2; k++)
        drawCircle(img, cx, cy, k, rgb);
    for (int k = -1; k <= 1; k++)
        drawCircle(img, cx, cy, r + k, rgb);
}

} // namespace

int main(int argc, char **argv)
{
    using coin::Option;
    const vector<Option> opts = {
        {"-f", "image.ppm", "input PPM (P3 or P6)"},
        {"-ds", "auto", "downscale factor (0 = auto, shorter side ~800 px)"},
        {"-rmin", "auto", "min coin radius, full-res px (0 = 2% of shorter side)"},
        {"-rmax", "auto", "max coin radius, full-res px (0 = 7% of shorter side)"},
        {"-ht", "auto", "Canny high threshold, Sobel units at working scale (0 = Otsu)"},
        {"-lt", "auto", "Canny low threshold (0 = lowfrac * high)"},
        {"-lowfrac", "0.5", "low threshold as a fraction of high"},
        {"-vt", "0.25", "centre vote threshold as a fraction of 2*pi*rmin"},
        {"-ct", "0.4", "min rim support (fraction of circumference) to accept a circle"},
        {"-cos", "0.9", "min |cos| between edge gradient and radial direction"},
        {"-copper", "0.06", "red-minus-green at or above this is copper"},
        {"-gold", "0.15", "tint-corrected red-minus-blue at or above this is brass"},
        {"-ref", "auto", "penny radius in full-res px (0 = fit from data)"},
        {"-refine", "1", "1 = refine centre and radius at full resolution"},
        {"-inner", "0.7", "refine: take the innermost ring scoring this fraction of the best"},
        {"-o", ".", "output directory"},
        {"-debug", "0", "1 = write intermediate images and print timings"},
    };
    coin::Args args(argc, argv, opts);
    const string infile = args.str("-f", "image.ppm");
    const string outdir = args.str("-o", ".");
    const bool debug = args.flagOn("-debug");
    const double lowFrac = args.real("-lowfrac", 0.5);
    const double vtFrac = args.real("-vt", 0.25);
    const double ct = args.real("-ct", 0.4);
    const double cosTol = args.real("-cos", 0.9);
    coin::ColourThresholds colour;
    colour.copper = args.real("-copper", colour.copper);
    colour.gold = args.real("-gold", colour.gold);
    const double refPenny = args.real("-ref", 0);
    const bool refine = args.integer("-refine", 1) != 0;
    const double innerFrac = args.real("-inner", 0.7);

    auto t0 = chrono::steady_clock::now();
    auto phase = [&](const char *name) {
        if (!debug)
            return;
        auto t1 = chrono::steady_clock::now();
        fprintf(stderr, "%-16s %7.3f s\n", name, chrono::duration<double>(t1 - t0).count());
        t0 = t1;
    };

    try
    {
        Image8 full = readPPM(infile);
        phase("read");

        const int ds = args.integer("-ds", 0) > 0 ? args.integer("-ds", 0) : coin::autoDownscale(full.w, full.h);
        const int minSide = min(full.w, full.h);
        int rminFull = args.integer("-rmin", 0) > 0 ? args.integer("-rmin", 0) : (int)lround(0.02 * minSide);
        int rmaxFull = args.integer("-rmax", 0) > 0 ? args.integer("-rmax", 0) : (int)lround(0.07 * minSide);
        const int rmin = max(3, rminFull / ds), rmax = max(rmin + 1, rmaxFull / ds);

        Image8 rgb = downscale(full, ds);
        Image8 gray = blur5(toGray(rgb));
        phase("downscale+blur");

        Image16 gx, gy;
        Image32 mag2;
        sobel(gray, gx, gy, mag2);
        double high = args.real("-ht", 0) > 0 ? args.real("-ht", 0) : otsuMagnitude(mag2);
        double low = args.real("-lt", 0) > 0 ? args.real("-lt", 0) : lowFrac * high;
        Image8 edge = canny(gx, gy, mag2, low, high);
        phase("sobel+canny");

        Image32 acc = houghVote(edge, gx, gy, rmin, rmax);
        const int minVotes = (int)lround(vtFrac * 2 * kPi * rmin);
        vector<Candidate> cands = findCenters(acc, minVotes, max(2, (int)(0.7 * rmin)));
        phase("vote+centres");

        struct Scored
        {
            Candidate c;
            Fit f;
        };
        vector<Scored> scored;
        for (const auto &c : cands)
        {
            Fit f = fitRadius(edge, gx, gy, c.x, c.y, rmin, rmax, cosTol);
            if (f.support >= ct)
                scored.push_back({c, f});
        }
        sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b) {
            return a.f.support != b.f.support ? a.f.support > b.f.support : a.c.votes > b.c.votes;
        });
        vector<Scored> kept;
        for (const auto &s : scored)
        {
            bool overlap = false;
            for (const auto &k : kept)
            {
                double dx = s.c.x - k.c.x, dy = s.c.y - k.c.y;
                double lim = 0.8 * max(s.f.r, k.f.r);
                if (dx * dx + dy * dy < lim * lim)
                {
                    overlap = true;
                    break;
                }
            }
            if (!overlap)
                kept.push_back(s);
        }
        phase("fit+dedup");

        vector<coin::Detection> dets;
        for (const auto &k : kept)
        {
            coin::Detection d = makeDetection(rgb, k.c.x, k.c.y, k.f.r, k.f.support, ds);
            if (refine && ds > 1)
            {
                Refined rf = refineAtFullRes(full, d.cx, d.cy, d.r, ds, cosTol, innerFrac);
                d.cx = rf.cx;
                d.cy = rf.cy;
                d.r = rf.r;
            }
            dets.push_back(d);
        }
        phase("refine");
        double penny = coin::classify(dets, refPenny, colour);

        ofstream res(outdir + "/results.txt");
        coin::writeResults(res, dets, penny);
        coin::writeResults(cout, dets, penny);
        ofstream circ(outdir + "/circles.txt");
        coin::writeCircles(circ, dets);

        Image8 coinsImg = rgb;
        for (const auto &d : dets)
            drawMarker(coinsImg, (int)lround(d.cx / ds), (int)lround(d.cy / ds), (int)lround(d.r / ds),
                       coin::info(d.type).rgb);
        writePPM(outdir + "/imageCoins.ppm", coinsImg);
        phase("classify+write");

        if (debug)
        {
            fprintf(stderr, "ds=%d work=%dx%d r=[%d,%d] canny=(%.0f,%.0f) minVotes=%d edges=%zu candidates=%zu "
                            "fitted=%zu kept=%zu penny=%.1f\n",
                    ds, rgb.w, rgb.h, rmin, rmax, low, high, minVotes,
                    (size_t)count(edge.px.begin(), edge.px.end(), 1), cands.size(), scored.size(), kept.size(),
                    penny);
            Image8 edgeImg(edge.w, edge.h, 1);
            for (size_t i = 0; i < edge.px.size(); i++)
                edgeImg.px[i] = edge.px[i] ? 255 : 0;
            writePPM(outdir + "/imagef.ppm", edgeImg);
            int maxV = *max_element(acc.px.begin(), acc.px.end());
            Image8 accImg(acc.w, acc.h, 1);
            for (size_t i = 0; i < acc.px.size(); i++)
                accImg.px[i] = maxV ? (uint8_t)(255LL * acc.px[i] / maxV) : 0;
            writePPM(outdir + "/imagev.ppm", accImg);
            Image8 ccImg = rgb;
            for (const auto &c : cands)
                drawMarker(ccImg, c.x, c.y, 0, {255, 0, 0});
            writePPM(outdir + "/imageCC.ppm", ccImg);
            Image8 circImg = rgb;
            for (const auto &k : kept)
                drawMarker(circImg, k.c.x, k.c.y, (int)lround(k.f.r), {255, 0, 0});
            writePPM(outdir + "/imageCircles.ppm", circImg);
            phase("debug images");
        }
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
