#include "Zhuizong.h"
#include <QDebug>
#include <opencv2/highgui.hpp>
#include<string>
using namespace std;
using namespace cv;


cv::Ptr<Tracker> Zhuizong::createTrackerByName()
{
    Ptr<Tracker> tracker;
//    if (trackerType ==  trackerTypes[0])
//        tracker = TrackerBoosting::create();
//    else if (trackerType == trackerTypes[1])
//        tracker = TrackerMIL::create();
//    else if (trackerType == trackerTypes[2])
//        tracker = TrackerKCF::create();
//    else if (trackerType == trackerTypes[3])
//        tracker = TrackerTLD::create();
//    else if (trackerType == trackerTypes[4])
//        tracker = TrackerMedianFlow::create();
//    else if (trackerType == trackerTypes[5])
//        tracker = TrackerGOTURN::create();
//    else if (trackerType == trackerTypes[6])
        tracker = TrackerMOSSE::create();
//    else if (trackerType == trackerTypes[7])
//        tracker = TrackerCSRT::create();
//    else {
//        qDebug() << "Incorrect tracker name" << endl;
//        qDebug() << "Available trackers are: " << endl;

//    }
    return tracker;
}
void Zhuizong::getRandomColors(vector<Scalar> &colors, int numColors)
{
    RNG rng(time(0));
    for (int i = 0; i < numColors; i++)
        colors.push_back(Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255)));
}
