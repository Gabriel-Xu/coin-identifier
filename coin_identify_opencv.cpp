// OpenCV coin detector: decode once, downscale, blur, Hough circles on the
// grayscale image (HoughCircles runs its own Canny), colour from the coin
// centre, then the same scale-invariant classification as the custom build.
#include "coin_common.hpp"

#include <fstream>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

int main(int argc, char **argv)
{
    using coin::Option;
    const vector<Option> opts = {
        {"-f", "image.jpg", "input image (any format OpenCV reads)"},
        {"-ds", "auto", "downscale factor (0 = auto, shorter side ~1600 px)"},
        {"-rmin", "auto", "min coin radius, full-res px (0 = 2% of shorter side)"},
        {"-rmax", "auto", "max coin radius, full-res px (0 = 7% of shorter side)"},
        {"-method", "alt", "alt (HOUGH_GRADIENT_ALT) or gradient (HOUGH_GRADIENT)"},
        {"-ht", "auto", "Canny high threshold inside HoughCircles (0 = 300 alt / 120 gradient)"},
        {"-ct", "30", "gradient method: accumulator threshold"},
        {"-perf", "0.6", "alt method: circle perfectness 0..1 (lower finds more)"},
        {"-blur", "5", "Gaussian blur kernel size at working scale (odd, 0 = none)"},
        {"-copper", "0.06", "red-minus-green at or above this is copper"},
        {"-gold", "0.15", "tint-corrected red-minus-blue at or above this is brass"},
        {"-ref", "auto", "penny radius in full-res px (0 = fit from data)"},
        {"-o", ".", "output directory"},
        {"-debug", "0", "1 = write intermediate images"},
    };
    coin::Args args(argc, argv, opts);
    const string infile = args.str("-f", "image.jpg");
    const string outdir = args.str("-o", ".");
    const string method = args.str("-method", "alt");
    const bool debug = args.flagOn("-debug");
    const int blurK = args.integer("-blur", 5);
    const double perf = args.real("-perf", 0.6);
    const int ct = args.integer("-ct", 30);
    const double refPenny = args.real("-ref", 0);
    coin::ColourThresholds colour;
    colour.copper = args.real("-copper", colour.copper);
    colour.gold = args.real("-gold", colour.gold);
    if (method != "alt" && method != "gradient")
    {
        cerr << "Error: -method must be 'alt' or 'gradient'\n";
        return 2;
    }

    Mat full = imread(infile, IMREAD_COLOR);
    if (full.empty())
    {
        cerr << "Error: could not read " << infile << "\n";
        return 1;
    }
    const int ds = args.integer("-ds", 0) > 0 ? args.integer("-ds", 0) : coin::autoDownscale(full.cols, full.rows, 1600);
    const int minSide = min(full.cols, full.rows);
    const int rminFull = args.integer("-rmin", 0) > 0 ? args.integer("-rmin", 0) : (int)lround(0.02 * minSide);
    const int rmaxFull = args.integer("-rmax", 0) > 0 ? args.integer("-rmax", 0) : (int)lround(0.07 * minSide);
    const int rmin = max(3, rminFull / ds), rmax = max(rmin + 1, rmaxFull / ds);
    const double ht = args.real("-ht", 0) > 0 ? args.real("-ht", 0) : (method == "alt" ? 300 : 120);

    Mat bgr;
    if (ds > 1)
        resize(full, bgr, Size(), 1.0 / ds, 1.0 / ds, INTER_AREA);
    else
        bgr = full;
    Mat gray;
    cvtColor(bgr, gray, COLOR_BGR2GRAY);
    if (blurK > 1)
        GaussianBlur(gray, gray, Size(blurK | 1, blurK | 1), 0);

    // Two smallest coins touching are 2*rmin apart; allow a little slack.
    const double minDist = 1.8 * rmin;
    vector<Vec3f> circles;
    if (method == "alt")
        HoughCircles(gray, circles, HOUGH_GRADIENT_ALT, 1.5, minDist, ht, perf, rmin, rmax);
    else
        HoughCircles(gray, circles, HOUGH_GRADIENT, 1, minDist, ht, ct, rmin, rmax);

    vector<coin::Detection> dets;
    for (const auto &c : circles)
    {
        coin::Detection d;
        d.cx = c[0] * ds;
        d.cy = c[1] * ds;
        d.r = c[2] * ds;
        Mat mask = Mat::zeros(bgr.size(), CV_8U);
        circle(mask, Point((int)lround(c[0]), (int)lround(c[1])), max(1, (int)(c[2] * 0.6)), 255, FILLED);
        Scalar m = mean(bgr, mask);
        d.meanB = m[0];
        d.meanG = m[1];
        d.meanR = m[2];
        d.support = 1.0;
        dets.push_back(d);
    }
    double penny = coin::classify(dets, refPenny, colour);

    ofstream res(outdir + "/results.txt");
    coin::writeResults(res, dets, penny);
    coin::writeResults(cout, dets, penny);
    ofstream circ(outdir + "/circles.txt");
    coin::writeCircles(circ, dets);

    Mat coins = bgr.clone(), outline = bgr.clone();
    for (const auto &d : dets)
    {
        Point centre((int)lround(d.cx / ds), (int)lround(d.cy / ds));
        int radius = (int)lround(d.r / ds);
        auto rgb = coin::info(d.type).rgb;
        Scalar col(rgb[2], rgb[1], rgb[0]);
        circle(coins, centre, 2, col, FILLED, LINE_AA);
        circle(coins, centre, radius, col, 3, LINE_AA);
        circle(outline, centre, 2, Scalar(0, 0, 255), FILLED, LINE_AA);
        circle(outline, centre, radius, Scalar(0, 0, 255), 3, LINE_AA);
    }
    imwrite(outdir + "/imageCoins.jpg", coins);
    imwrite(outdir + "/imageCircles.jpg", outline);
    if (debug)
    {
        imwrite(outdir + "/imageg.jpg", gray);
        Mat edges;
        Canny(gray, edges, ht / 2, ht);
        imwrite(outdir + "/imagef.jpg", edges);
        fprintf(stderr, "ds=%d work=%dx%d r=[%d,%d] method=%s ht=%.0f circles=%zu penny=%.1f\n", ds, bgr.cols,
                bgr.rows, rmin, rmax, method.c_str(), ht, circles.size(), penny);
    }
    return 0;
}
