#ifndef ZHUIZONG_H
#define ZHUIZONG_H

#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <QObject>

using namespace std;
using namespace cv;

class Zhuizong  {

public:
    vector<string> trackerTypes = {"BOOSTING", "MIL", "KCF", "TLD", "MEDIANFLOW", "GOTURN", "MOSSE", "CSRT"};
    Ptr<Tracker> createTrackerByName();
    void getRandomColors(vector<Scalar> &colors, int numColors);

private:


};

#endif // ZHUIZONG_H
