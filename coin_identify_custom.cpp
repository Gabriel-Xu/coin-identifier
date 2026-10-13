#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <map>
#include <cmath>
#include <unordered_set>

using namespace std;

void drawLine(int x1, int y1, int x2, int y2, vector<vector<int>> &filled, int mx, int my)
{
    int dx = x2 - x1, dy = y2 - y1;
    int j, e;
    if (x1 == x2)
    {
        for (int i = min(y1, y2); i <= max(y1, y2); i++)
        {
            if (i >= 0 && i < my)
                filled[x1][i] += 1;
        }
    }
    else if (y1 == y2)
    {
        for (int i = min(x1, x2); i <= max(x1, x2); i++)
        {
            if (i >= 0 && i < mx)
                filled[i][y1] += 1;
        }
    }
    else if (abs(dx) >= abs(dy))
    {
        if (x1 > x2)
        {
            swap(x1, x2);
            swap(y1, y2);
            dx *= -1;
            dy *= -1;
        }
        j = y1;
        e = abs(dy) - dx;
        for (int i = x1; i <= x2; i++)
        {
            if (i >= 0 && i < mx && j >= 0 && j < my)
            {
                filled[i][j] += 1;
            }
            if (e >= 0)
            {
                j += (dy > 0 ? 1 : -1);
                e -= dx;
            }
            e += abs(dy);
        }
    }
    else
    {
        if (y1 > y2)
        {
            swap(x1, x2);
            swap(y1, y2);
            dx *= -1;
            dy *= -1;
        }
        j = x1;
        e = abs(dx) - dy;
        for (int i = y1; i <= y2; i++)
        {
            if (j >= 0 && j < mx && i >= 0 && i < my)
            {
                filled[j][i] += 1;
            }
            if (e >= 0)
            {
                j += (dx > 0 ? 1 : -1);
                e -= dy;
            }
            e += abs(dx);
        }
    }
}

void drawCircle(int cx, int cy, int radius, vector<vector<int>> &filled, int mx, int my, int shift, int color = 1)
{
    vector<int> a = {1, -1};
    int x, y, xmax, y2, y2_new, ty;
    xmax = round(radius * .70710678);
    y = radius;
    y2 = y * y;
    ty = (2 * y) - 1;
    y2_new = y2;
    for (x = 0; x <= xmax + shift; x++)
    {
        if ((y2 - y2_new) >= ty)
        {
            y2 -= ty;
            y--;
            ty -= 2;
        }
        for (int i : a)
        {
            for (int j : a)
            {
                if (cx + i * x >= 0 && cx + i * x < mx && cy + j * y >= 0 && cy + j * y < my)
                {
                    filled[cx + i * x][cy + j * y] = color;
                }
                if (cx + j * y >= 0 && cx + j * y < mx && cy + i * x >= 0 && cy + i * x < my)
                {
                    filled[cx + j * y][cy + i * x] = color;
                }
            }
        }
        y2_new -= 2 * x - 3;
    }
}

vector<int> checkCircle(int cx, int cy, int radius, vector<vector<bool>> &filled, int mx, int my)
{
    vector<int> a = {1, -1};
    int x, y, xmax, y2, y2_new, ty;
    xmax = round(radius * .70710678);
    y = radius;
    y2 = y * y;
    ty = (2 * y) - 1;
    y2_new = y2;
    int ans = 0;
    int tot = 0;
    for (x = 0; x <= xmax + 1; x++)
    {
        if ((y2 - y2_new) >= ty)
        {
            y2 -= ty;
            y--;
            ty -= 2;
        }
        for (int i : a)
        {
            for (int j : a)
            {
                if (cx + i * x >= 0 && cx + i * x < mx && cy + j * y >= 0 && cy + j * y < my)
                {
                    if (filled[cx + i * x][cy + j * y])
                        ans++;
                    tot++;
                }
                if (cx + j * y >= 0 && cx + j * y < mx && cy + i * x >= 0 && cy + i * x < my)
                {
                    if (filled[cx + j * y][cy + i * x])
                        ans++;
                    tot++;
                }
            }
        }
        y2_new -= 2 * x - 3;
    }
    return {ans, tot};
}

vector<int> di = {0, 0, 1, 1, 1, -1, -1, -1};
vector<int> dj = {1, -1, 1, 0, -1, 1, 0, -1};
void dfs(vector<vector<int>> &ans, vector<vector<bool>> &visited, int i, int j, int mx, int my)
{
    visited[i][j] = true;
    ans[i][j] = 2;
    for (int dir = 0; dir < 8; dir++)
    {
        int newi = i + di[dir], newj = j + dj[dir];
        if (newi >= 0 && newi < my && newj >= 0 && newj < mx && !visited[newi][newj] && ans[newi][newj] == 1)
        {
            dfs(ans, visited, newi, newj, mx, my);
        }
    }
}

void part3(int argc, char **argv)
{
    int mx, my;
    string infile = "image.ppm";
    string outfilef = "imagef.ppm";
    string outfilev = "imagev.ppm";
    string outfileCC = "imageCC.ppm";
    string outfileCircles = "imageCircles.ppm";
    string outfileCoins = "imageCoins.ppm";
    string outfileResults = "results.txt";
    int lt = 70, ht = 100, vt = 15, ct = 25;
    int pcutoff = 93;
    int rmin = 80, rmax = 120;
    for (int i = 0; i < argc - 1; i++)
    {
        string s = argv[i];
        string s2 = argv[i + 1];
        if (s == "-f")
            infile = s2;
        if (s == "-ff")
            outfilef = s2;
        if (s == "-fv")
            outfilev = s2;
        if (s == "-fcc")
            outfileCC = s2;
        if (s == "-fCi")
            outfileCircles = s2;
        if (s == "-fCo")
            outfileCoins = s2;
        if (s == "-fR")
            outfileResults = s2;
        if (s == "-lt")
            lt = stoi(s2);
        if (s == "-ht")
            ht = stoi(s2);
        if (s == "-TC")
            vt = stoi(s2);
        if (s == "-TCircle")
            ct = stoi(s2);
        if (s == "-pt")
            pcutoff = stoi(s2);
    }
    lt *= lt;
    ht *= ht;
    ifstream fin(infile);
    string temp;
    fin >> temp >> mx >> my >> temp;
    vector<vector<vector<int>>> grid(my, vector<vector<int>>(mx, vector<int>(3)));
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            fin >> grid[i][j][0] >> grid[i][j][1] >> grid[i][j][2];
        }
    }
    vector<vector<int>> grayscale(my, vector<int>(mx));
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            grayscale[i][j] = (int)((double)(grid[i][j][0] + grid[i][j][1] + grid[i][j][2]) / 3 + .5);
        }
    }
    vector<vector<int>> sobelx(my, vector<int>(mx));
    vector<vector<int>> sobely(my, vector<int>(mx));
    for (int i = 1; i < my - 1; i++)
    {
        for (int j = 1; j < mx - 1; j++)
        {
            sobelx[i][j] = 2 * (grayscale[i][j + 1] - grayscale[i][j - 1]) + grayscale[i - 1][j + 1] - grayscale[i - 1][j - 1] + grayscale[i + 1][j + 1] - grayscale[i + 1][j - 1];
            sobely[i][j] = 2 * (grayscale[i + 1][j] - grayscale[i - 1][j]) + grayscale[i + 1][j + 1] - grayscale[i - 1][j + 1] + grayscale[i + 1][j - 1] - grayscale[i - 1][j - 1];
        }
    }
    vector<vector<int>> ans(my);
    vector<vector<bool>> visited(my);
    for (int i = 0; i < my; i++)
    {
        ans[i].resize(mx);
        visited[i].resize(mx);
        for (int j = 0; j < mx; j++)
        {
            visited[i][j] = false;
            if (i == 0 || i == my - 1 || j == 0 || j == mx - 1)
            {
                ans[i][j] = 0;
            }
            else if (sobelx[i][j] * sobelx[i][j] + sobely[i][j] * sobely[i][j] >= ht)
            {
                ans[i][j] = 2;
            }
            else if (sobelx[i][j] * sobelx[i][j] + sobely[i][j] * sobely[i][j] >= lt)
            {
                ans[i][j] = 1;
            }
            else
            {
                ans[i][j] = 0;
            }
        }
    }
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            if (!visited[i][j] && ans[i][j] == 2)
            {
                dfs(ans, visited, i, j, mx, my);
            }
        }
    }
    double PI = 3.141592653589793238462643383279502884197;
    vector<vector<double>> magnitude(my, vector<double>(mx));
    vector<vector<double>> angle(my, vector<double>(mx));
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            if (i == 0 || i == my - 1 || j == 0 || j == mx - 1)
            {
                magnitude[i][j] = 0;
                angle[i][j] = 0;
            }
            else
            {
                magnitude[i][j] = sqrt(sobelx[i][j] * sobelx[i][j] + sobely[i][j] * sobely[i][j]);
                angle[i][j] = atan2(sobely[i][j], sobelx[i][j]);
            }
        }
    }
    map<int, vector<int>> dir;
    dir[180] = {0, -1};
    dir[135] = {1, -1};
    dir[90] = {1, 0};
    dir[45] = {1, 1};
    dir[0] = {0, 1};
    dir[-45] = {-1, 1};
    dir[-90] = {-1, 0};
    dir[-135] = {-1, -1};
    vector<vector<bool>> ans2(my, vector<bool>(mx));
    vector<vector<bool>> ans_final(my);
    for (int i = 0; i < my; i++)
    {
        for (int j = 1; j < mx; j++)
        {
            if (i == 0 || i == my - 1 || j == 0 || j == mx - 1)
            {
                ans2[i][j] = 0;
            }
            else
            {
                int cur_angle;
                if (angle[i][j] > PI * 7 / 8)
                    cur_angle = 180;
                else if (angle[i][j] > PI * 5 / 8)
                    cur_angle = 135;
                else if (angle[i][j] > PI * 3 / 8)
                    cur_angle = 90;
                else if (angle[i][j] > PI / 8)
                    cur_angle = 45;
                else if (angle[i][j] > -PI / 8)
                    cur_angle = 0;
                else if (angle[i][j] > -PI * 3 / 8)
                    cur_angle = -45;
                else if (angle[i][j] > -PI * 5 / 8)
                    cur_angle = -90;
                else if (angle[i][j] > -PI * 7 / 8)
                    cur_angle = -135;
                else
                    cur_angle = 180;
                ans2[i][j] = magnitude[i][j] >= magnitude[i + dir[cur_angle][0]][j + dir[cur_angle][1]] && magnitude[i][j] >= magnitude[i - dir[cur_angle][0]][j - dir[cur_angle][1]];
            }
        }
    }
    ofstream fout(outfilef);
    fout << "P3 " << mx << " " << my << " 1\n";
    for (int i = 0; i < my; i++)
    {
        ans_final[i].resize(mx);
        for (int j = 0; j < mx; j++)
        {
            ans_final[i][j] = ans[i][j] == 2 && ans2[i][j] == 1;
            if (ans_final[i][j])
            {
                fout << "1 1 1 ";
            }
            else
            {
                fout << "0 0 0 ";
            }
        }
        fout << "\n";
    }
    fout.close();
    vector<vector<int>> votes(my);
    for (auto &v : votes)
    {
        v.resize(mx);
        fill(v.begin(), v.end(), 0);
    }
    int buffer = rmax + 40;
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            if (ans_final[i][j])
            {
                int i1, i2, j1, j2;
                if (sobelx[i][j] == 0 && sobely[i][j] == 0)
                {
                    continue;
                }
                if (abs(sobely[i][j]) > abs(sobelx[i][j]))
                {
                    i1 = i + buffer;
                    j1 = round(j + (double)sobelx[i][j] / sobely[i][j] * (i1 - i));
                    i2 = i - buffer;
                    j2 = round(j - (double)sobelx[i][j] / sobely[i][j] * (i - i2));
                }
                else
                {
                    j1 = j + buffer;
                    i1 = round(i + (double)sobely[i][j] / sobelx[i][j] * (j1 - j));
                    j2 = j - buffer;
                    i2 = round(i - (double)sobely[i][j] / sobelx[i][j] * (j - j2));
                }
                drawLine(i1, j1, i2, j2, votes, my, mx);
            }
        }
    }
    int max_votes = 0;
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
            max_votes = max(max_votes, votes[i][j]);
    }
    fout.open(outfilev);
    fout << "P3 " << mx << " " << my << " " << max_votes << "\n";
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                fout << votes[i][j] << " ";
            }
        }
        fout << "\n";
    }
    fout.close();
    vector<vector<int>> centers(my);
    for (int i = 0; i < my; i++)
    {
        centers[i].resize(mx);
        for (int j = 0; j < mx; j++)
        {
            centers[i][j] = 0;
        }
    }
    vector<vector<int>> circles;
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            if (votes[i][j] >= vt)
            {
                centers[i][j] = 1;
                for (int k = 1; k <= 3; k++)
                    drawCircle(i, j, k, centers, my, mx, 1);
                drawCircle(i, j, 4, centers, my, mx, 0);
                vector<int> max_circle;
                for (int r = rmin; r <= rmax; r++)
                {
                    vector<int> circle_counts = checkCircle(i, j, r, ans_final, my, mx);
                    if ((double)circle_counts[0] / circle_counts[1] >= .01 * ct)
                    {
                        if (max_circle.size() == 0 || 100 * circle_counts[0] / circle_counts[1] > max_circle[3])
                            max_circle = {i, j, r, 100 * circle_counts[0] / circle_counts[1]};
                    }
                }
                for (int r = 170; r <= 190; r++)
                {
                    vector<int> circle_counts = checkCircle(i, j, r, ans_final, my, mx);
                    if ((double)circle_counts[0] / circle_counts[1] >= .01 * ct)
                    {
                        if (max_circle.size() == 0 || 100 * circle_counts[0] / circle_counts[1] > max_circle[3])
                            max_circle = {i, j, r, 100 * circle_counts[0] / circle_counts[1]};
                    }
                }
                if (max_circle.size() > 0)
                    circles.push_back(max_circle);
            }
        }
    }
    unordered_set<int> del_circles;
    for (int i = 0; i < circles.size(); i++)
    {
        for (int j = i + 1; j < circles.size(); j++)
        {
            if (!del_circles.count(i) && !del_circles.count(j))
            {
                if ((circles[i][0] - circles[j][0]) * (circles[i][0] - circles[j][0]) + (circles[i][1] - circles[j][1]) * (circles[i][1] - circles[j][1]) < 110 * 110 && abs(circles[i][2] - circles[j][2]) <= 100)
                {
                    //                     if(circles[i][3]==circles[j][3]) {
                    //                         if(circles[i][2]>circles[j][2]) del_circles.insert(i);
                    //                         else del_circles.insert(j);
                    //                     }
                    if (circles[i][3] > circles[j][3])
                        del_circles.insert(j);
                    else
                        del_circles.insert(i);
                }
            }
        }
    }
    vector<vector<int>> circles_img(my);
    vector<vector<int>> coins_img(my);
    for (int i = 0; i < my; i++)
    {
        circles_img[i].resize(mx);
        coins_img[i].resize(mx);
        for (int j = 0; j < mx; j++)
        {
            circles_img[i][j] = 0;
            coins_img[i][j] = -1;
        }
    }
    map<string, int> coin_counts;
    vector<string> color_to_coin = {"dime", "penny", "nickel", "quarter", "half dollar coin", "dollar coin"};
    coin_counts["dime"] = 0;
    coin_counts["penny"] = 0;
    coin_counts["nickel"] = 0;
    coin_counts["quarter"] = 0;
    coin_counts["half dollar coin"] = 0;
    coin_counts["dollar coin"] = 0;
    for (int idx = 0; idx < circles.size(); idx++)
    {
        if (del_circles.count(idx))
            continue;
        vector<int> circle = circles[idx];
        int i = circle[0], j = circle[1];
        circles_img[i][j] = 1;
        coins_img[i][j] = 1;
        for (int k = 1; k <= 3; k++)
        {
            drawCircle(i, j, k, circles_img, my, mx, 1);
            drawCircle(i, j, k, coins_img, my, mx, 1);
        }
        drawCircle(i, j, 4, circles_img, my, mx, 0);
        drawCircle(i, j, 4, coins_img, my, mx, 0);
        drawCircle(i, j, circle[2] - 1, circles_img, my, mx, 1);
        drawCircle(i, j, circle[2], circles_img, my, mx, 1);
        drawCircle(i, j, circle[2] + 1, circles_img, my, mx, 1);
        int color;
        if (circle[2] < 85)
            color = 0;
        else if (circle[2] < pcutoff)
            color = 1;
        else if (circle[2] < 102)
            color = 2;
        else if (circle[2] < 110)
            color = 3;
        else if (circle[2] < 130)
            color = 4;
        else
            color = 5;
        drawCircle(i, j, circle[2] - 1, coins_img, my, mx, 1, color);
        drawCircle(i, j, circle[2], coins_img, my, mx, 1, color);
        drawCircle(i, j, circle[2] + 1, coins_img, my, mx, 1, color);
        coin_counts[color_to_coin[color]]++;
    }
    fout.open(outfileResults);
    int total = 0;
    for (auto &x : coin_counts)
    {
        fout << x.second << " ";
        cout << x.second << " ";
        if (x.first == "penny")
        {
            fout << "pennies\n";
            cout << "pennies\n";
        }
        else
        {
            fout << x.first << "s\n";
            cout << x.first << "s\n";
        }
        if (x.first == "penny")
            total += x.second;
        else if (x.first == "nickel")
            total += 5 * x.second;
        else if (x.first == "quarter")
            total += 25 * x.second;
    }
    fout << "Total sum: $" << total / 100 << "." << total % 100;
    cout << "Total sum: $" << total / 100 << "." << total % 100 << "\n";
    fout.close();
    fout.open(outfileCC);
    fout << "P3 " << mx << " " << my << " 255\n";
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            if (centers[i][j])
            {
                fout << "255 0 0 ";
            }
            else
            {
                for (int k = 0; k < 3; k++)
                {
                    fout << grid[i][j][k] << " ";
                }
            }
        }
        fout << "\n";
    }
    fout.close();
    fout.open(outfileCircles);
    fout << "P3 " << mx << " " << my << " 255\n";
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            if (circles_img[i][j])
            {
                fout << "255 0 0 ";
            }
            else
            {
                for (int k = 0; k < 3; k++)
                {
                    fout << grid[i][j][k] << " ";
                }
            }
        }
        fout << "\n";
    }
    fout.close();
    fout.open(outfileCoins);
    fout << "P3 " << mx << " " << my << " 255\n";
    for (int i = 0; i < my; i++)
    {
        for (int j = 0; j < mx; j++)
        {
            if (coins_img[i][j] == 0)
            {
                fout << "0 0 255 ";
            }
            else if (coins_img[i][j] == 1)
            {
                fout << "255 0 0 ";
            }
            else if (coins_img[i][j] == 2)
            {
                fout << "255 255 0 ";
            }
            else if (coins_img[i][j] == 3)
            {
                fout << "255 0 255 ";
            }
            else if (coins_img[i][j] == 4)
            {
                fout << "0 255 0 ";
            }
            else if (coins_img[i][j] == 5)
            {
                fout << "255 105 180 ";
            }
            else
            {
                for (int k = 0; k < 3; k++)
                {
                    fout << grid[i][j][k] << " ";
                }
            }
        }
        fout << "\n";
    }
    fout.close();
}

int main(int argc, char **argv)
{
    part3(argc, argv);
}
