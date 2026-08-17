#ifndef TEMPLATEMATCH_H
#define TEMPLATEMATCH_H
#include <opencv2/opencv.hpp>
#include<opencv2/highgui.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <time.h>
#include <QtCore>
#include <QObject>
#include <opencv2/features2d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <tuple>
#include "detection/common/character_template_matcher.h"
using namespace cv;

class TemplateMatch:public QThread
{
    Q_OBJECT
public:
    explicit TemplateMatch(QObject* parent = nullptr) ;
    cv::Mat img;
    Mat *img1=nullptr;
    Mat *imgshibie=nullptr;
    cv::Mat templ,result;
    char window_title_1;
    char window_title_2;
    char window_title_3;
    int match_method;
    int value;
    String input="";
//    int max_Trackbar = 5;
    double getMSSIM(cv::Mat& img1, cv::Mat& img2);
    double getSimilarity(const cv::Mat &img1, const cv::Mat &img2);
    int calculateOverlapArea(const cv::Rect& rect1, const cv::Rect& rect2);
    double calculateIOU(const cv::Rect& rectA, const cv::Rect& rectB);
    // 新增：用于存储 run3 每次运算完的检测结果 <矩形框, 匹配分数, 目标字符位置索引>
    std::vector<std::tuple<cv::Rect, double, size_t>> lastMatchResults;
    static TemplateMatchPreparedTemplates prepareDigitTemplates(
        const std::vector<cv::Mat> &digitTemplates);
    int run3(
        const cv::Mat &targetImage,
        const TemplateMatchPreparedTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes);

signals:



public slots:
    void receshibie(Mat*img2);
    int run3(std::vector<cv::Mat> digitTemplates);
    int run3(std::vector<cv::Mat> digitTemplates, const std::vector<int> &templateTargetIndexes);
    void jianceshibiestr(String string1);
    void ssimvalue(int s);
};

#endif // TEMPLATEMATCH_H
