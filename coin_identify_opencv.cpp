#include <stdio.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;

using namespace cv;

int main(int argc, char **argv)
{
    string infile = "image.jpg";
    int lt = 50, ht = 150, ct = 15, bt = 15;
    int penny = 90, nickel = 102, dt = 10000, dime = 82, quarter = 120;
    for (int i = 0; i < argc - 1; i++)
    {
        string s = argv[i];
        string s2 = argv[i + 1];
        if (s == "-f")
            infile = s2;
        if (s == "-lt")
            lt = stoi(s2);
        if (s == "-ht")
            ht = stoi(s2);
        if (s == "-ht")
            ht = stoi(s2);
        if (s == "-ct")
            ct = stoi(s2);
        if (s == "-bt")
            bt = stoi(s2);
        if (s == "-pt")
            penny = stoi(s2);
        if (s == "-dt")
            dt = stoi(s2);
        if (s == "-dt2")
            dime = stoi(s2);
        if (s == "-qt")
            quarter = stoi(s2);
    }
    Mat image = imread(infile);
    Mat image2 = imread(infile);
    Mat gray = imread(infile, IMREAD_GRAYSCALE);
    imwrite("./imageg.jpg", gray);
    medianBlur(gray, gray, bt);
    Canny(gray, gray, lt, ht, 3);
    imwrite("./imagef.jpg", gray);
    vector<Vec3f> circles, circles2;
    HoughCircles(gray, circles, HOUGH_GRADIENT, 1,
                 gray.rows / 32,  // change this value to detect circles with different distances to each other
                 500, ct, 80, 120 // change the last two parameters
                                  // (min_radius & max_radius) to detect larger circles
    );
    HoughCircles(gray, circles2, HOUGH_GRADIENT, 1,
                 gray.rows / 16,   // change this value to detect circles with different distances to each other
                 500, dt, 180, 200 // change the last two parameters
                                   // (min_radius & max_radius) to detect larger circles
    );
    for (auto &x : circles2)
        circles.push_back(x);
    map<string, int> coin_counts;
    vector<string> color_to_coin = {"dime", "penny", "nickel", "quarter", "half dollar coin", "dollar coin"};
    coin_counts["dime"] = 0;
    coin_counts["penny"] = 0;
    coin_counts["nickel"] = 0;
    coin_counts["quarter"] = 0;
    coin_counts["half dollar coin"] = 0;
    coin_counts["dollar coin"] = 0;
    for (size_t i = 0; i < circles.size(); i++)
    {
        Vec3i c = circles[i];
        Point center = Point(c[0], c[1]);
        Vec3b center_color = image2.at<Vec3b>(center);
        int r = center_color[2], g = center_color[1], b = center_color[0];
        // circle center
        circle(image, center, 1, Scalar(0, 0, 255), 3, LINE_AA);
        // circle outline
        int radius = c[2];
        circle(image, center, radius - 1, Scalar(0, 0, 255), 3, LINE_AA);
        circle(image, center, radius, Scalar(0, 0, 255), 3, LINE_AA);
        circle(image, center, radius + 1, Scalar(0, 0, 255), 3, LINE_AA);
        int color;
        Scalar col;
        if (radius < dime)
        {
            color = 0;
            col = Scalar(255, 0, 0);
        }
        else if (radius < 90 || radius < penny && r > g && r > b)
        {
            color = 1;
            col = Scalar(0, 0, 255);
        }
        else if (radius < nickel)
        {
            color = 2;
            col = Scalar(0, 255, 255);
        }
        else if (radius < quarter)
        {
            color = 3;
            col = Scalar(150, 0, 150);
        }
        else if (radius < 150)
        {
            color = 4;
            col = Scalar(0, 255, 0);
        }
        else
        {
            color = 5;
            col = Scalar(180, 105, 255);
        }
        coin_counts[color_to_coin[color]]++;
        circle(image2, center, 1, col, 3, LINE_AA);
        circle(image2, center, radius - 1, col, 3, LINE_AA);
        circle(image2, center, radius, col, 3, LINE_AA);
        circle(image2, center, radius + 1, col, 3, LINE_AA);
    }
    imwrite("./imageCircles.jpg", image);
    imwrite("./imageCoins.jpg", image2);
    ofstream fout;
    fout.open("results.txt");
    int total = 0;
    for (auto &x : coin_counts)
    {
        fout << x.second << " ";
        cout << x.second << " ";
        fout << x.first << "s\n";
        cout << x.first << "s\n";
        if (x.first == "penny")
            total += x.second;
        else if (x.first == "nickel")
            total += 5 * x.second;
        else if (x.first == "quarter")
            total += 25 * x.second;
        else if (x.first == "dime")
            total += 10 * x.second;
        else if (x.first == "half dollar coin")
            total += 50 * x.second;
        else if (x.first == "dollar coin")
            total += 100 * x.second;
    }
    fout << "Total sum: $" << total / 100 << "." << total % 100;
    cout << "Total sum: $" << total / 100 << "." << total % 100 << "\n";
    fout.close();
}
