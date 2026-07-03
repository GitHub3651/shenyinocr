#include "templatematch.h"
#include <iostream>
#include <QDebug>
#include <QObject>
#include <QMessageBox>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/core/utility.hpp>

using namespace std;
using namespace cv;
using namespace cv::xfeatures2d;

TemplateMatch::TemplateMatch(QObject* parent)
    : QThread(parent)
{
    myThread = new MyThread();
    cameraThread = new CameraThread();
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<cv::Mat*>("cv::Mat*");
}

int TemplateMatch::calculateOverlapArea(const cv::Rect& rect1, const cv::Rect& rect2) {
    int overlapX = std::max(0, std::min(rect1.x + rect1.width, rect2.x + rect2.width) - std::max(rect1.x, rect2.x));
    int overlapY = std::max(0, std::min(rect1.y + rect1.height, rect2.y + rect2.height) - std::max(rect1.y, rect2.y));
    return overlapX * overlapY;
}

//------------------------【1】基于轮廓特征的相似度计算--------------------------------//
// 与之前类似，只是多加一个参数 winNamePrefix 用于区分显示窗口
double TemplateMatch::getSimilarity(const cv::Mat &img1, const cv::Mat &img2)
{
    // 1) 转灰度
    cv::Mat gray1, gray2;
    if (img1.channels() == 3) {
        cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
    } else {
        gray1 = img1.clone();
    }

    if (img2.channels() == 3) {
        cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
    } else {
        gray2 = img2.clone();
    }

    // 2) 高斯模糊（可根据实际情况调整核大小）
    cv::GaussianBlur(gray1, gray1, cv::Size(5, 5), 0);
    cv::GaussianBlur(gray2, gray2, cv::Size(5, 5), 0);

    // 3) 二值化（使用Otsu阈值）
    cv::Mat bin1, bin2;
    cv::threshold(gray1, bin1, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    cv::threshold(gray2, bin2, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2,2)); // 可调参数
    cv::dilate(bin1, bin1, kernel, cv::Point(-1, -1), 1); // 可调参数
    cv::dilate(bin2, bin2, kernel, cv::Point(-1, -1), 1); // 可调参数

    // 4) 保证模板图不大于目标图，如若 bin2（模板）大于 bin1，就缩放 bin2 到与 bin1 同样大小
    if (bin2.cols > bin1.cols || bin2.rows > bin1.rows) {
        cv::resize(bin2, bin2, bin1.size());
    }

    // 5) 使用 matchTemplate 计算相似度
    // 注意 matchTemplate 中第二个参数为“模板”，第一个参数为“待匹配的图像”
    // 如果 bin2 是模板，就应该写成 matchTemplate(bin1, bin2, result, ...)
    cv::Mat result;
    cv::matchTemplate(bin1, bin2, result, cv::TM_CCOEFF_NORMED);

    // 6) matchTemplate 结果在 result 中，若和 bin1、bin2 大小正好相同，那么只会输出单个值
    double similarity = result.at<float>(0, 0);

    return similarity;
}






//------------------------【2】接收/存储外部图像--------------------------------//
void TemplateMatch::recemuban(Mat *img1)
{
    imgmuban = img1;
}

void TemplateMatch::receshibie(Mat *img2)
{
    imgshibie = img2;
}

// 这里如果你想把 int 型的 value 用作阈值，也可以保留
void TemplateMatch::ssimvalue(int s)
{
    value = s;
//    qDebug()<<"threshold"<<value;
}

////固定尺寸分割
//void TemplateMatch::extractDigits(const cv::Mat &image, std::vector<cv::Mat> &digitRegions)
//{


//    // 1. 转灰度
//    cv::Mat gray;
//    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);


//    // 2.中值滤波 去除空白部分噪声 防止对空白部分进行分割
//    cv::Mat filtered;
//    cv::GaussianBlur(gray, filtered, cv::Size(5, 5), 0); // 核大小和标准差可根据实际调整
//    cv::medianBlur(filtered, filtered, 5); // 3为滤波核大小，可根据噪声情况调整

//    // 3. 自适应二值化
//    cv::Mat binary;
//    cv::adaptiveThreshold(
//        filtered,
//        binary,
//        255,
//        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
//        cv::THRESH_BINARY_INV,
//        block_size1,
//        block_size2
//    );

//    // 4. 形态学去噪(开运算等)
//    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelsize, kernelsize));
//    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);


//    cv::imshow("Binarized Image", binary);
////    cv::imwrite("D:\\software\\QT\\project\\mobanpipei1\\muban\\p1.png", binary);

////    // 纵向膨胀
////    cv::Mat verticalKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, verticalKernel)); // 垂直矩形核
////    cv::dilate(binary, binary, verticalKernel);

////    // 横向膨胀
////    cv::Mat horizontalKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(horizontalKernel, 1)); // 水平矩形核
////    cv::dilate(binary, binary, horizontalKernel);


//    // 5. 连通域分析
//    cv::Mat labels, stats, centroids;
//    cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);
////    cv::imwrite("D:\\software\\QT\\project\\mobanpipei1\\muban\\p2.png", labels);


//    // 6. 过滤小面积连通域
//    cv::Mat cleanedBinary = cv::Mat::zeros(binary.size(), CV_8UC1);

//    for (int i = 1; i < stats.rows; ++i)  // 忽略背景标签0
//    {
//        int area = stats.at<int>(i, cv::CC_STAT_AREA);
//        if (area > 150) { // area阈值可根据实际需要调节
//            cleanedBinary.setTo(255, labels == i);
//        }
//    }
////    cv::imwrite("D:\\software\\QT\\project\\mobanpipei1\\muban\\p3.png", cleanedBinary);



//    // 7. 查找轮廓
//    std::vector<std::vector<cv::Point>> contours;
//    cv::findContours(cleanedBinary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
////    qDebug()<<"contours.size"<<contours.size();



// //    8. 根据轮廓提取边界框，并基于宽高过滤
//    std::vector<cv::Rect> boundingBoxes;
//    for (const auto &contour : contours)
//        {
//            cv::Rect bbox = cv::boundingRect(contour);

//            // 原外接矩形的左上角坐标
//            int originalTopLeftX = bbox.x;
//            int originalTopLeftY = bbox.y;

//            // 新矩形的左上角坐标等于原外接矩形的左上角坐标
//            int newX = originalTopLeftX;
//            int newY = originalTopLeftY;

//            // 确保新矩形在图像内
//            if (newX < 0) {
//                newX = 0;
//            }
//            if (newY < 0) {
//                newY = 0;
//            }
//            if (newX + fixedcols > cleanedBinary.cols) {
//                newX = cleanedBinary.cols - fixedcols;
//            }
//            if (newY + fixedrows > cleanedBinary.rows) {
//                newY = cleanedBinary.rows - fixedrows;
//            }

//            // 创建固定宽高的矩形
//            cv::Rect fixedBbox(newX, newY, fixedcols, fixedrows);


//            if ((bbox.x!= 0 || bbox.y!= 0) && bbox.width > 0 && bbox.height > 0) {
//                boundingBoxes.push_back(fixedBbox);
//            }
//        }


//    int rowThreshold = 10; // 根据行距可调

//    // 1. 排序：先按行 (y 坐标)，再按列 (x 坐标)
//    std::sort(
//        boundingBoxes.begin(),
//        boundingBoxes.end(),
//        [rowThreshold](const cv::Rect &a, const cv::Rect &b) -> bool
//        {
//            // 若 a.y 与 b.y 距离小，视为同一行，则按 x 排序
//            if (std::abs(a.y - b.y) < rowThreshold)
//            {
//                return a.x < b.x; // 同一行，按 x 排序
//            }
//            else
//            {
//                // 否则，按 y 排序（谁在上谁优先）
//                return a.y < b.y;
//            }
//        }
//    );

//    // 2. 将同一行的矩形纵坐标统一为该行的最大 y 值
//    for (size_t i = 0; i < boundingBoxes.size();)
//    {
//        int currentRowY = boundingBoxes[i].y; // 当前行的初始 y 坐标
//        std::vector<size_t> sameRowIndices;  // 存储同一行的索引

//        // 找到属于同一行的所有矩形
//        for (size_t j = i; j < boundingBoxes.size(); ++j)
//        {
//            if (std::abs(boundingBoxes[j].y - currentRowY) < rowThreshold)
//            {
//                sameRowIndices.push_back(j); // 同一行
//            }
//            else
//            {
//                break; // 超过 rowThreshold，属于下一行
//            }
//        }

//        // 找到同一行中最小的 y 坐标
//        int minY = currentRowY;
//        for (size_t idx : sameRowIndices)
//        {

//           minY = std::min(minY, boundingBoxes[idx].y); // 更新最大 y
//        }



//        // 将同一行的矩形 y 坐标设置为 `minY`
//        for (size_t idx : sameRowIndices)
//        {
//            boundingBoxes[idx].y = minY; // 统一 y 坐标
//        }

//        // 跳过当前行的所有矩形
//        i += sameRowIndices.size();
//    }

//        // 过滤有重叠的矩形框
//      for (size_t i = 1; i < boundingBoxes.size(); ++i) {
//            bool shouldRemove = false;
//            for (size_t j = 0; j < i; ++j) {
//                int overlapArea = calculateOverlapArea(boundingBoxes[i], boundingBoxes[j]);
//                if (overlapArea > 150) {
//                    shouldRemove = true;
//                    break;
//                }
//            }
//            if (shouldRemove) {
//                boundingBoxes.erase(boundingBoxes.begin() + i);
//                --i; // 由于删除了一个元素，需要将索引减1，以确保不遗漏元素
//            }
//        }

//    // 9. 更新 rectTopCenterPoints，画检测框
//    rectTopCenterPoints.clear();   // 避免重复累加
//    imageWithBoxes = image.clone(); // 用于显示
//    for (const auto &bbox : boundingBoxes)
//    {
//        // 记录“正上方中心点”
//        cv::Point topCenter(bbox.x + bbox.width/2, bbox.y);
//        rectTopCenterPoints.push_back(topCenter);

//        // 绘制矩形框
//        cv::rectangle(imageWithBoxes, bbox, cv::Scalar(0,255,0), 1);
//    }
//    // 显示检测结果
//    cv::imshow("Detected Digit Regions", imageWithBoxes);
//    cv::imwrite("D:\\software\\QT\\project\\mobanpipei1\\muban\\imageWithBoxes.png", imageWithBoxes);

//    // 10. 最终裁剪 ROI 输出
//    digitRegions.clear(); // 避免重复累加
//    for (const auto &bbox : boundingBoxes)
//    {
//        cv::Mat roi = image(bbox).clone();
//        digitRegions.push_back(roi);
//    }

//}

//自适应分割
void TemplateMatch::extractDigits(const cv::Mat &image, std::vector<cv::Mat> &digitRegions)
{
    // 1. 转灰度
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    // 2.中值滤波 去除空白部分噪声 防止对空白部分进行分割
    cv::Mat filtered;
    cv::GaussianBlur(gray, filtered, cv::Size(5, 5), 0); // 核大小和标准差可根据实际调整
    cv::medianBlur(filtered, filtered, 5); // 3为滤波核大小，可根据噪声情况调整

    // 2. 自适应二值化
    cv::Mat binary;
    cv::adaptiveThreshold(
        filtered,
        binary,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY_INV,
        block_size1,
        2
    );

    // 3. 形态学去噪(开运算等)
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelsize, kernelsize));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);

    // 纵向膨胀
    cv::Mat verticalKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, verticalKernelsize)); // 垂直矩形核
    cv::dilate(binary, binary, verticalKernel);

    // 横向膨胀
    cv::Mat horizontalKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(horizontalKernelsize, 1)); // 水平矩形核
    cv::dilate(binary, binary, horizontalKernel);

    // 4. 连通域分析
    cv::Mat labels, stats, centroids;
    cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);

    // 5. 过滤小面积连通域
    cv::Mat cleanedBinary = cv::Mat::zeros(binary.size(), CV_8UC1);
    for (int i = 1; i < stats.rows; ++i)  // 忽略背景标签0
    {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > 100) { // area阈值可根据实际需要调节
            cleanedBinary.setTo(255, labels == i);
        }
    }

    // 6. 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(cleanedBinary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 7. 根据轮廓提取边界框，并基于宽高过滤
    std::vector<cv::Rect> boundingBoxes;
    for (const auto &contour : contours)
    {
        cv::Rect bbox = cv::boundingRect(contour);
        // 宽高过滤：width_min, width_max, height_min, height_max 为你设定的阈值
        if (bbox.width  > width_min  &&
            bbox.height > height_min &&
            bbox.width  < width_max  &&
            bbox.height < height_max)
        {
            boundingBoxes.push_back(bbox);
        }
    }
//    cv::imshow("windowbinary",cleanedBinary);



    // =========================================================
    // 关键：对 boundingBoxes 进行「先上到下，再左到右」排序
    // =========================================================
    int rowThreshold = 15; // 根据行距可调

 //    比较器：若 y 距离近 => 看 x；否则看 y
    std::sort(
        boundingBoxes.begin(),
        boundingBoxes.end(),
        [rowThreshold](const cv::Rect &a, const cv::Rect &b) -> bool
        {
            // 若 a.y 与 b.y 距离小，视为同一行，则按 x 排序
            if (std::abs(a.y - b.y) < rowThreshold)
            {
                return a.x < b.x;
            }
            else
            {
                // 否则，谁 y 小谁在前（谁在上）
                return a.y < b.y;
            }
        }
    );

    // 定义列间距阈值，根据实际图像调整
//    int columnThreshold = 10;

    // 比较器：若 x 距离近 => 看 y；否则看 x
//    std::sort(
//        boundingBoxes.begin(),
//        boundingBoxes.end(),
//        [columnThreshold](const cv::Rect &a, const cv::Rect &b) -> bool
//        {
//            // 若 a.x 与 b.x 距离小，视为同一列，则按 y 排序
//            if (std::abs(a.x - b.x) < columnThreshold)
//            {
//                return a.y < b.y;
//            }
//            else
//            {
//                // 否则，谁 x 小谁在前（谁在左）
//                return a.x < b.x;
//            }
//        }
//    );

    // ---------------------------------------------------------
    // 如果你需要更稳健的“分行聚类”，可以改成以下的「先 y 初排 + 行聚类 + 行内 x 排序」方案
    // （在实战中更灵活一些，但这里先给出最简便的一步排序版本）
    // ---------------------------------------------------------

    // 8. 更新 rectTopCenterPoints，画检测框
    rectTopCenterPoints.clear();   // 避免重复累加
    imageWithBoxes = image.clone(); // 用于显示
    for (const auto &bbox : boundingBoxes)
    {
        // 记录“正上方中心点”
        cv::Point topCenter(bbox.x + bbox.width/2, bbox.y);
        rectTopCenterPoints.push_back(topCenter);

        // 绘制矩形框
        cv::rectangle(imageWithBoxes, bbox, cv::Scalar(0,255,0), 2);
    }
    // 显示检测结果
    cv::imshow("Detected Digit Regions", imageWithBoxes);
//    cv::waitKey(0);

    // 9. 最终裁剪 ROI 输出
    digitRegions.clear(); // 避免重复累加
    for (const auto &bbox : boundingBoxes)
    {
        cv::Mat roi = image(bbox).clone();
        digitRegions.push_back(roi);
    }
}




//------------------------【4】设置参数--------------------------------//
void TemplateMatch::caijiansize(int a, int b, int c, int d, int e, int i, int j)
{
    width_min   = a;
    width_max   = b;
    height_min  = c;
    height_max  = d;
    block_size1 = e;
//    block_size2 = f;
    horizontalKernelsize = i;
    verticalKernelsize = j;
//    qDebug()<<"hengxiang"<<horizontalKernelsize;

}

void TemplateMatch::kernel(int a)
{
    kernelsize = a;
//    qDebug() << "kernelsize" << kernelsize;
}

//------------------------【5】主要匹配函数：1对1匹配ROI--------------------------------//
double TemplateMatch::run1(std::vector<cv::Mat> digitTemplates)
{
    // --------------------【1】提取ROI-------------------- //
    // 从识别图像中提取ROI
    Mat img_display;
    imgshibie->copyTo(img_display);

    std::vector<cv::Mat> digitRegions;
    extractDigits(img_display, digitRegions);

    // 如果数字区域数量少于模板数量，则无法逐一匹配
    if (digitRegions.size() < digitTemplates.size()) {
        qDebug() << "[ERROR] digitRegions.size()=" << digitRegions.size()
                 << " digitTemplates.size()=" << digitTemplates.size();
        return 0.0;
    }

    int N = static_cast<int>(digitRegions.size());
    int M = static_cast<int>(digitTemplates.size());

    // SSIM 阈值(0~1)
    double thresholdSSIM = (double)value / 100.0;
    thresholdSSIM = std::min(std::max(thresholdSSIM, 0.0), 1.0); // 限制在 0~1

    // --------------------【A】搜索第1张模板的最佳匹配位置 -------------------- //
    int startIndex = -1;

    // 遍历所有ROI，找与第一张模板的相似度 >= thresholdSSIM 的那个
    for (int i = 0; i < N; i++)
    {
        // 调整大小，使模板大小适应ROI
        cv::Mat regionResized;
        cv::resize(digitRegions[i], regionResized, digitTemplates[0].size());

        // 直接用getSimilarity进行匹配
        double similarity = getSimilarity(regionResized, digitTemplates[0]);

        // 输出每个区域与第1张模板的匹配度 (可注释)
        // qDebug() << "[INFO] Matching score for ROI[" << i << "] with Template[0] = " << similarity;

        // 检查是否满足阈值
        if (similarity >= thresholdSSIM) {
            startIndex = i;
            // qDebug() << "[INFO] Found matching ROI[" << i << "] for Template[0] with similarity =" << similarity;
            break;
        }
    }

    // 如果没有找到满足阈值的匹配，默认从第一个区域开始
    if (startIndex == -1) {
        startIndex = 0;
//        qDebug() << "[WARN] No ROI matched the first template above the threshold. Starting from index 0.";
    }

    // 确保从startIndex开始，后续匹配不越界
    if (startIndex + M > N) {
        startIndex = N - M;
        if (startIndex < 0) startIndex = 0;
//        qDebug() << "[WARN] Adjusted startIndex to" << startIndex << "to prevent out-of-bounds.";
    }

    // --------------------【B】逐一匹配各张模板 -------------------- //
    double minScore = 1.0;           // 记录所有匹配的最小分(可选)
    std::vector<double> shapeVals(M, 0.0);

    for (int i = 0; i < M; i++)
    {
        int regionIndex = startIndex + i;
        // 确保不越界
        if (regionIndex >= N) {
            qDebug() << "[ERROR] regionIndex" << regionIndex << "is out of bounds. Skipping Template[" << i << "].";
            shapeVals[i] = 0.0;
            continue;
        }

        // 获取当前的目标ROI & 模板图
        cv::Mat roi = digitRegions[regionIndex].clone();
        cv::Mat tmpl = digitTemplates[i].clone();

        // 如果图为空，跳过或赋0
        if (roi.empty() || tmpl.empty()) {
            qDebug() << "[ERROR] ROI[" << regionIndex << "] or Template[" << i << "] is empty. Skip this pair.";
            shapeVals[i] = 0.0;
            continue;
        }

        // 调整大小，使两者尺寸一致
        cv::resize(roi, roi, tmpl.size());

        // 这里不再做轮廓提取，只做基本预处理：
        //   1. 转灰度
        //   2. 高斯模糊 (可选)
        //   3. 二值化 (可选)
        // 这些都已经在 getSimilarity() 里做了，所以我们可以直接传 roi, tmpl 进去。



        // 计算相似度
        double scoreVal = getSimilarity(roi, tmpl);
        shapeVals[i] = scoreVal;

        // 输出每个区域的匹配度 (可注释)
        // qDebug() << "[INFO] Matching score for ROI[" << regionIndex
        //          << "] with Template[" << i << "] = " << scoreVal;

        // 若要记录最小分
        if (scoreVal < minScore) {
            minScore = scoreVal;
        }
    }

    // --------------------【C】在 imageWithBoxes 上写分数 (可选)-------------------- //
    // (如果你的 rectTopCenterPoints 跟 digitRegions 数量一致，可参考如下方法做可视化)
    for (int i = 0; i < M && i < static_cast<int>(rectTopCenterPoints.size()); ++i)
    {
        // 获取当前矩形框的中心坐标
        cv::Point topLeft = rectTopCenterPoints[startIndex + i];
        cv::Size boxSize = digitRegions[startIndex + i].size();

        // 获取当前模板与ROI的匹配得分
        double scoreVal = shapeVals[i];

        // 计算文本(百分比形式)
        double scorePercent = scoreVal * 100.0;
        std::string scoreText = std::to_string(static_cast<int>(scorePercent));

        // 计算文本的大小
        int baseline = 0;
        double fontScale = 0.4;  // 设置文本字体大小
        cv::Size textSize = cv::getTextSize(scoreText, cv::FONT_HERSHEY_SIMPLEX, fontScale, 1, &baseline);

        // 计算文本的居中位置（文本的中心需要与框的中心对齐）
        int x = topLeft.x - textSize.width / 2; // 文本X坐标，使文本居中
        int y = topLeft.y - 1;                  // 文本Y坐标，使文本显示在矩形框上方

        // 绘制文本
        cv::putText(imageWithBoxes, scoreText, cv::Point(x, y),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 0, 0), 1);

        // 判断得分是否低于阈值（说明相似度不够）
        if (scoreVal < thresholdSSIM) {
            // 计算矩形框的左上角和右下角坐标
            int rectLeft   = topLeft.x - boxSize.width / 2;
            int rectTop    = topLeft.y;
            int rectRight  = topLeft.x + boxSize.width / 2;
            int rectBottom = topLeft.y + boxSize.height;

            // 绘制红色矩形框
            cv::rectangle(imageWithBoxes, cv::Point(rectLeft, rectTop),
                          cv::Point(rectRight, rectBottom), cv::Scalar(0, 0, 255), 2);
        }
    }

    // --------------------【D】显示结果 (可选)-------------------- //
    double scaleFactor = 2.0;  // 放大倍数，可根据需要调整
    cv::Mat imageWithBoxesResized;
    cv::resize(imageWithBoxes, imageWithBoxesResized,
               cv::Size(), scaleFactor, scaleFactor, cv::INTER_LINEAR);

    cv::namedWindow("Match Result", cv::WINDOW_NORMAL);
    cv::resizeWindow("Match Result", imageWithBoxesResized.cols*0.5, imageWithBoxesResized.rows*0.5);
    cv::imshow("Match Result", imageWithBoxesResized);
    cv::waitKey(1);

    // --------------------【E】返回最小分 (或其他指标)-------------------- //
    return minScore;
}



double TemplateMatch::calculateIOU(const cv::Rect& rectA, const cv::Rect& rectB) {
    cv::Rect intersection = rectA & rectB;
    if (intersection.area() <= 0) return 0.0;
    double unionArea = rectA.area() + rectB.area() - intersection.area();
    return intersection.area() / unionArea;
}




double TemplateMatch::run2(std::vector<Mat> digitTemplates,std::vector<Mat> digitRigions)
{
    // --------------------【1】提取ROI-------------------- //
    // 如果数字区域数量少于模板数量，则无法逐一匹配
    if (digitRigions.size() < digitTemplates.size()) {
        qDebug() << "[ERROR] digitRegions.size()=" << digitRigions.size()
                 << " digitTemplates.size()=" << digitTemplates.size();
        return 0.0;
    }

    int N = static_cast<int>(digitRigions.size());
    int M = static_cast<int>(digitTemplates.size());

    // SSIM 阈值(0~1)
    double thresholdSSIM = (double)value / 100.0;
    thresholdSSIM = std::min(std::max(thresholdSSIM, 0.0), 1.0); // 限制在 0~1

    // --------------------【A】搜索第1张模板的最佳匹配位置 -------------------- //
    int startIndex = -1;

    // 遍历所有ROI，找与第一张模板的相似度 >= thresholdSSIM 的那个
    for (int i = 0; i < N; i++)
    {
        // 调整大小，使模板大小适应ROI
        cv::Mat regionResized;
        cv::resize(digitRigions[i], regionResized, digitTemplates[0].size());

        // 直接用getSimilarity进行匹配
        double similarity = getSimilarity(regionResized, digitTemplates[0]);

        // 输出每个区域与第1张模板的匹配度 (可注释)
        // qDebug() << "[INFO] Matching score for ROI[" << i << "] with Template[0] = " << similarity;

        // 检查是否满足阈值
        if (similarity >= thresholdSSIM) {
            startIndex = i;
            // qDebug() << "[INFO] Found matching ROI[" << i << "] for Template[0] with similarity =" << similarity;
            break;
        }
    }

    // 如果没有找到满足阈值的匹配，默认从第一个区域开始
    if (startIndex == -1) {
        startIndex = 0;
//        qDebug() << "[WARN] No ROI matched the first template above the threshold. Starting from index 0.";
    }

    // 确保从startIndex开始，后续匹配不越界
    if (startIndex + M > N) {
        startIndex = N - M;
        if (startIndex < 0) startIndex = 0;
//        qDebug() << "[WARN] Adjusted startIndex to" << startIndex << "to prevent out-of-bounds.";
    }

    // --------------------【B】逐一匹配各张模板 -------------------- //
    double minScore = 1.0;           // 记录所有匹配的最小分(可选)
    std::vector<double> shapeVals(M, 0.0);

    for (int i = 0; i < M; i++)
    {
        int regionIndex = startIndex + i;
        // 确保不越界
        if (regionIndex >= N) {
            qDebug() << "[ERROR] regionIndex" << regionIndex << "is out of bounds. Skipping Template[" << i << "].";
            shapeVals[i] = 0.0;
            continue;
        }

        // 获取当前的目标ROI & 模板图
        cv::Mat roi = digitRigions[regionIndex].clone();
        cv::Mat tmpl = digitTemplates[i].clone();

        // 如果图为空，跳过或赋0
        if (roi.empty() || tmpl.empty()) {
            qDebug() << "[ERROR] ROI[" << regionIndex << "] or Template[" << i << "] is empty. Skip this pair.";
            shapeVals[i] = 0.0;
            continue;
        }

        // 调整大小，使两者尺寸一致
        cv::resize(roi, roi, tmpl.size());

        // 这里不再做轮廓提取，只做基本预处理：
        //   1. 转灰度
        //   2. 高斯模糊 (可选)
        //   3. 二值化 (可选)
        // 这些都已经在 getSimilarity() 里做了，所以我们可以直接传 roi, tmpl 进去。

//        //对目标图像进行膨胀（适用于字符较大的情况）
//        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3,3)); // 可调参数
//        cv::dilate(roi, roi, kernel, cv::Point(-1, -1), 1); // 可调参数
//        //对模板图像进行膨胀
//        cv::dilate(tmpl, tmpl, kernel, cv::Point(-1, -1), 1); // 应与ROI相同

        // 计算相似度
        double scoreVal = getSimilarity(roi, tmpl);
        shapeVals[i] = scoreVal;

        // 输出每个区域的匹配度 (可注释)
         qDebug() << "[INFO] Matching score for ROI[" << regionIndex
                  << "] with Template[" << i << "] = " << scoreVal;

        // 若要记录最小分
        if (scoreVal < minScore) {
            minScore = scoreVal;
        }
    }

    // --------------------【C】在 imageWithBoxes 上写分数 (可选)-------------------- //
    // (如果你的 rectTopCenterPoints 跟 digitRegions 数量一致，可参考如下方法做可视化)
    for (int i = 0; i < M && i < static_cast<int>(rectTopCenterPoints.size()); ++i)
    {
        // 获取当前矩形框的中心坐标
        cv::Point topLeft = rectTopCenterPoints[startIndex + i];
        cv::Size boxSize = digitRigions[startIndex + i].size();

        // 获取当前模板与ROI的匹配得分
        double scoreVal = shapeVals[i];

        // 计算文本(百分比形式)
        double scorePercent = scoreVal * 100.0;
        std::string scoreText = std::to_string(static_cast<int>(scorePercent));

        // 计算文本的大小
        int baseline = 0;
        double fontScale = 0.4;  // 设置文本字体大小
        cv::Size textSize = cv::getTextSize(scoreText, cv::FONT_HERSHEY_SIMPLEX, fontScale, 1, &baseline);

        // 计算文本的居中位置（文本的中心需要与框的中心对齐）
        int x = topLeft.x - textSize.width / 2; // 文本X坐标，使文本居中
        int y = topLeft.y - 1;                  // 文本Y坐标，使文本显示在矩形框上方

        // 绘制文本
        cv::putText(imageWithBoxes, scoreText, cv::Point(x, y),
                    cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 0, 0), 1);


        // 判断得分是否低于阈值（说明相似度不够）
        if (scoreVal < thresholdSSIM) {
            // 计算矩形框的左上角和右下角坐标
            int rectLeft   = topLeft.x - boxSize.width / 2;
            int rectTop    = topLeft.y;
            int rectRight  = topLeft.x + boxSize.width / 2;
            int rectBottom = topLeft.y + boxSize.height;

            // 绘制红色矩形框
            cv::rectangle(imageWithBoxes, cv::Point(rectLeft, rectTop),
                          cv::Point(rectRight, rectBottom), cv::Scalar(0, 0, 255), 2);
        }
    }

    // --------------------【D】显示结果 (可选)-------------------- //
    double scaleFactor = 2.0;  // 放大倍数，可根据需要调整
    cv::Mat imageWithBoxesResized;
    cv::resize(imageWithBoxes, imageWithBoxesResized,
               cv::Size(), scaleFactor, scaleFactor, cv::INTER_LINEAR);

    cv::namedWindow("Match Result", cv::WINDOW_NORMAL);
    cv::resizeWindow("Match Result", imageWithBoxesResized.cols, imageWithBoxesResized.rows);
    cv::imshow("Match Result", imageWithBoxesResized);
    cv::waitKey(1);

    // --------------------【E】返回最小分 (或其他指标)-------------------- //
    return minScore;
}

int TemplateMatch::run3(std::vector<cv::Mat> digitTemplates) {
    std::vector<int> templateTargetIndexes;
    templateTargetIndexes.reserve(digitTemplates.size());
    for (int i = 0; i < static_cast<int>(digitTemplates.size()); ++i) {
        templateTargetIndexes.push_back(i);
    }
    return run3(digitTemplates, templateTargetIndexes);
}

int TemplateMatch::run3(std::vector<cv::Mat> digitTemplates, const std::vector<int> &templateTargetIndexes) {

    Mat targetImage;
    imgshibie->copyTo(targetImage);
    // --- 1. 基础检查与图像预处理 ---
    if (digitTemplates.empty() || targetImage.empty()) {
        qDebug() << "[ERROR] Templates or target image is empty, cannot proceed with matching.";
        return 0.0;
    }

    // 将目标图像转换为灰度图，以进行模板匹配
    cv::Mat grayTarget;
    cv::cvtColor(targetImage, grayTarget, cv::COLOR_BGR2GRAY);

    // 将所有数字模板转换为灰度图
    std::vector<cv::Mat> grayTemplates;
    for (const cv::Mat& tmpl : digitTemplates) {
        if (tmpl.empty()) {
            grayTemplates.emplace_back(); // 如果模板为空，添加一个空Mat
            continue;
        }
        cv::Mat grayTmpl;
        // 如果模板是彩色图，转换为灰度图；否则直接复制
        if (tmpl.channels() == 3) {
            cv::cvtColor(tmpl, grayTmpl, cv::COLOR_BGR2GRAY);
        } else {
            grayTmpl = tmpl.clone();
        }
        grayTemplates.push_back(grayTmpl);
    }

    std::vector<int> effectiveTargetIndexes;
    effectiveTargetIndexes.reserve(grayTemplates.size());
    if (templateTargetIndexes.size() == grayTemplates.size()) {
        for (int index : templateTargetIndexes) {
            effectiveTargetIndexes.push_back(index);
        }
    } else {
        for (int i = 0; i < static_cast<int>(grayTemplates.size()); ++i) {
            effectiveTargetIndexes.push_back(i);
        }
    }

    // 配置匹配阈值，将其从百分比转换为0-1之间的浮点数
    const double threshold = static_cast<double>(value) / 100.0;

    // 存储所有模板的匹配位置和分数
    std::vector<std::vector<cv::Rect>> allMatchLocations(grayTemplates.size());
    std::vector<std::vector<double>> allMatchScores(grayTemplates.size());

    // =========================================================================
    // --- 2. 降采样 + CPU 多线程并行加速版 ---
    // =========================================================================

    // 设定降采样比例（0.5 表示宽高各缩小一半，计算量直接降为原来的 1/4 到 1/16）
    const double scale = 0.5;

    cv::Mat smallTarget;
    // 缩小目标大图
    cv::resize(grayTarget, smallTarget, cv::Size(), scale, scale, cv::INTER_LINEAR);

    // 提前缩小所有模板，避免在多线程内重复缩放，榨干性能
    std::vector<cv::Mat> smallTemplates(grayTemplates.size());
    for (size_t i = 0; i < grayTemplates.size(); ++i) {
        if (!grayTemplates[i].empty()) {
            cv::resize(grayTemplates[i], smallTemplates[i], cv::Size(), scale, scale, cv::INTER_LINEAR);
        }
    }

    cv::parallel_for_(cv::Range(0, grayTemplates.size()), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; ++i) {
            const cv::Mat& smallTmpl = smallTemplates[i];
            const cv::Mat& origTmpl = grayTemplates[i]; // 保留对原图模板的引用，用于还原宽高

            // 检查模板是否有效，并且尺寸是否小于缩小后的目标图像，否则跳过
            if (smallTmpl.empty() || smallTarget.cols < smallTmpl.cols || smallTarget.rows < smallTmpl.rows) {
                continue;
            }

            // 执行模板匹配：在缩小的图像上匹配，速度极快！
            cv::Mat result;
            cv::matchTemplate(smallTarget, smallTmpl, result, cv::TM_CCOEFF_NORMED);

            // 遍历匹配结果，收集所有分数超过阈值的匹配位置和分数
            for (int y = 0; y < result.rows; ++y) {
                const float* row = result.ptr<float>(y);
                for (int x = 0; x < result.cols; ++x) {
                    if (row[x] >= threshold) {
                        // 【坐标降维打击】：将缩小图上找到的 (x,y) 除以 scale，精准还原回原图坐标
                        int orig_x = static_cast<int>(std::round(x / scale));
                        int orig_y = static_cast<int>(std::round(y / scale));

                        // 框的宽高直接使用原始大模板的宽高，保证100%精准贴合
                        allMatchLocations[i].emplace_back(orig_x, orig_y, origTmpl.cols, origTmpl.rows);
                        allMatchScores[i].push_back(row[x]);
                    }
                }
            }
        }
    });

    // =========================================================================
    // --- 3. 为每个模板按匹配分数排序（降序） (CPU 多线程并行加速版) ---
    // =========================================================================
    cv::parallel_for_(cv::Range(0, allMatchLocations.size()), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; ++i) {
            if (allMatchLocations[i].empty()) continue;

            // 组合分数和位置
            std::vector<std::pair<double, cv::Rect>> matches;
            matches.reserve(allMatchLocations[i].size()); // 提前分配内存，提升速度
            for (size_t j = 0; j < allMatchLocations[i].size(); j++) {
                matches.emplace_back(allMatchScores[i][j], allMatchLocations[i][j]);
            }

            // 按分数降序排序
            std::sort(matches.begin(), matches.end(),
                [](const auto& a, const auto& b) {
                    return a.first > b.first;
                });

            // 将排序后的结果写回
            allMatchLocations[i].clear();
            allMatchScores[i].clear();
            for (const auto& match : matches) {
                allMatchLocations[i].push_back(match.second);
                allMatchScores[i].push_back(match.first);
            }
        }
    });

    // =========================================================================
    // 以下部分保持串行，因为它们存在逻辑依赖或数据整合操作，不适合多线程
    // =========================================================================

    // --- 4. 非重叠匹配位置选择 ---
    std::vector<std::vector<cv::Rect>> filteredLocations(grayTemplates.size());
    std::vector<std::vector<double>> filteredScores(grayTemplates.size());
    std::vector<cv::Rect> selectedLocations;  // 存储已选择的矩形

    int targetGroupCount = 0;
    for (int targetIndex : effectiveTargetIndexes) {
        if (targetIndex >= 0) {
            targetGroupCount = std::max(targetGroupCount, targetIndex + 1);
        }
    }
    if (targetGroupCount <= 0) {
        targetGroupCount = static_cast<int>(grayTemplates.size());
    }

    std::vector<std::vector<size_t>> templateIndexesByTarget(static_cast<size_t>(targetGroupCount));
    for (size_t i = 0; i < effectiveTargetIndexes.size(); ++i) {
        const int targetIndex = effectiveTargetIndexes[i];
        if (targetIndex >= 0 && targetIndex < targetGroupCount) {
            templateIndexesByTarget[static_cast<size_t>(targetIndex)].push_back(i);
        }
    }

    struct MatchCandidate {
        double score = 0.0;
        cv::Rect rect;
        size_t templateIndex = 0;
    };

    // 按目标字符位置处理；同一目标位置的多个模板图只会选出一个最佳匹配。
    for (int targetIndex = 0; targetIndex < targetGroupCount; ++targetIndex) {
        bool found = false;
        std::vector<MatchCandidate> candidates;

        for (size_t templateIndex : templateIndexesByTarget[static_cast<size_t>(targetIndex)]) {
            for (size_t j = 0; j < allMatchLocations[templateIndex].size(); ++j) {
                MatchCandidate candidate;
                candidate.score = allMatchScores[templateIndex][j];
                candidate.rect = allMatchLocations[templateIndex][j];
                candidate.templateIndex = templateIndex;
                candidates.push_back(candidate);
            }
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const MatchCandidate &a, const MatchCandidate &b) {
                      return a.score > b.score;
                  });

        for (const MatchCandidate &candidate : candidates) {
            const cv::Rect& candidateRect = candidate.rect;
            bool overlap = false;

            // 检查是否与任何已选位置重叠
            for (const cv::Rect& selectedRect : selectedLocations) {
                // 计算IoU
                double iou = calculateIOU(candidateRect, selectedRect);
                if (iou > 0.3) {  // IoU阈值设为0.3
                    overlap = true;
                    break;
                }
            }

            // 如果没重叠则选择该位置
            if (!overlap) {
                // 添加到最终结果
                filteredLocations[candidate.templateIndex].push_back(candidateRect);
                filteredScores[candidate.templateIndex].push_back(candidate.score);

                // 加入已选择集合
                selectedLocations.push_back(candidateRect);
                found = true;
                break;  // 只需当前模板的一个位置
            }
        }

        // 可选：如果当前模板没有找到不重叠的位置，记录日志
        if (!found) {
            qDebug() << "[INFO] No non-overlapping match found for target index: " << targetIndex;
        }
    }

    // 将过滤后的结果赋值回原始变量
    allMatchLocations = std::move(filteredLocations);
    allMatchScores = std::move(filteredScores);


    // --- 4. 尺寸统一与行内高度对齐的核心逻辑 ---

    // 4.1 计算所有模板的平均尺寸（包含宽度和高度）。
    double totalAvgWidth = 0.0;
    double totalAvgHeight = 0.0;
    int validTemplateCount = 0;

    for (const cv::Mat& tmpl : grayTemplates) {
        if (!tmpl.empty() && tmpl.rows > 0 && tmpl.cols > 0) {
            totalAvgWidth += tmpl.cols;
            totalAvgHeight += tmpl.rows;
            validTemplateCount++;
        }
    }

    const int globalAvgWidth = std::max(1, static_cast<int>(std::round(totalAvgWidth / (validTemplateCount > 0 ? validTemplateCount : 1))));
    const int globalAvgHeight = std::max(1, static_cast<int>(std::round(totalAvgHeight / (validTemplateCount > 0 ? validTemplateCount : 1))));

    // 4.2 将所有矩形框的尺寸统一为计算出的全局平均尺寸。
    for (auto& locs : allMatchLocations) {
        for (cv::Rect& rect : locs) {
            const cv::Point currentCenter = rect.tl() + cv::Point(rect.width / 2, rect.height / 2);
            rect = cv::Rect(
                currentCenter.x - globalAvgWidth / 2,   // 新的 x 坐标
                currentCenter.y - globalAvgHeight / 2,  // 新的 y 坐标
                globalAvgWidth,                         // 新的宽度，统一为平均宽度
                globalAvgHeight                         // 新的高度，统一为平均高度
            );
        }
    }

    // 4.3 按行统一矩形高度（高度差小于10像素的矩形视为同一行）。
    const int rowThreshold = 10; // 行距阈值，Y坐标差小于此值视为同一行

    std::vector<cv::Rect*> allRectPtrs;
    for (auto& locs : allMatchLocations) {
        for (cv::Rect& rect : locs) {
            allRectPtrs.push_back(&rect);
        }
    }

    if (allRectPtrs.empty()) {
        qDebug() << "[DEBUG] No rectangles found for row-wise height unification.";
    } else {
        std::sort(allRectPtrs.begin(), allRectPtrs.end(), [](const cv::Rect* a, const cv::Rect* b) {
            return a->y < b->y;
        });

        std::vector<std::vector<cv::Rect*>> groupedRows;
        std::vector<cv::Rect*> currentRow;
        currentRow.push_back(allRectPtrs[0]);
        int base_y_for_row = allRectPtrs[0]->y;

        for (size_t i = 1; i < allRectPtrs.size(); ++i) {
            cv::Rect* currentRect = allRectPtrs[i];
            if (std::abs(currentRect->y - base_y_for_row) <= rowThreshold) {
                currentRow.push_back(currentRect);
            } else {
                groupedRows.push_back(currentRow);
                currentRow.clear();
                currentRow.push_back(currentRect);
                base_y_for_row = currentRect->y;
            }
        }
        if (!currentRow.empty()) groupedRows.push_back(currentRow);
    }

    // --- 5. 存储匹配结果并关闭底层弹窗 ---
    std::vector<std::tuple<cv::Rect, double, size_t>> sortedMatches;
    for (size_t i = 0; i < allMatchLocations.size(); ++i) {
        if (!allMatchLocations[i].empty()) {
            const int targetIndex = (i < effectiveTargetIndexes.size()) ? effectiveTargetIndexes[i] : static_cast<int>(i);
            if (targetIndex < 0) {
                continue;
            }
            sortedMatches.emplace_back(
                allMatchLocations[i][0],     // 只取第一个匹配矩形
                allMatchScores[i][0],        // 对应的匹配分数
                static_cast<size_t>(targetIndex) // 目标字符位置索引
            );
        }
    }

    // 按矩形的左上角坐标排序
    std::sort(sortedMatches.begin(), sortedMatches.end(),
        [](const auto& a, const auto& b) {
            const cv::Rect& rectA = std::get<0>(a);
            const cv::Rect& rectB = std::get<0>(b);
            return (rectA.y < rectB.y) || (rectA.y == rectB.y && rectA.x < rectB.x);
    });

    // ================= 核心修改 =================
    // 将排序后的结果保存到类成员，供 Widget 读取，绝不在这里进行任何 imshow 操作！
    this->lastMatchResults = sortedMatches;
    // ============================================

    // 计算实际的矩形框数量
    int totalDetectedRects = 0;
    for (const auto& locs : allMatchLocations) {
        totalDetectedRects += static_cast<int>(locs.size());
    }

    return totalDetectedRects;
}

//int TemplateMatch::run3(std::vector<cv::Mat> digitTemplates) {

//    Mat targetImage;
//    imgshibie->copyTo(targetImage);
//    // --- 1. 基础检查与图像预处理 ---
//    if (digitTemplates.empty() || targetImage.empty()) {
//        qDebug() << "[ERROR] Templates or target image is empty, cannot proceed with matching.";
//        return 0.0;
//    }

//    // 将目标图像转换为灰度图，以进行模板匹配
//    cv::Mat grayTarget;
//    cv::cvtColor(targetImage, grayTarget, cv::COLOR_BGR2GRAY);

//    // 将所有数字模板转换为灰度图
//    std::vector<cv::Mat> grayTemplates;
//    for (const cv::Mat& tmpl : digitTemplates) {
//        if (tmpl.empty()) {
//            grayTemplates.emplace_back(); // 如果模板为空，添加一个空Mat
//            continue;
//        }
//        cv::Mat grayTmpl;
//        // 如果模板是彩色图，转换为灰度图；否则直接复制
//        if (tmpl.channels() == 3) {
//            cv::cvtColor(tmpl, grayTmpl, cv::COLOR_BGR2GRAY);
//        } else {
//            grayTmpl = tmpl.clone();
//        }
//        grayTemplates.push_back(grayTmpl);
//    }

//    // 配置匹配阈值，将其从百分比转换为0-1之间的浮点数
//    const double threshold = static_cast<double>(value) / 100.0;

//    // 存储所有模板的匹配位置和分数
//    // allMatchLocations[i] 存储了第i个模板在目标图像中的所有匹配矩形
//    // allMatchScores[i] 存储了对应的匹配分数
//    std::vector<std::vector<cv::Rect>> allMatchLocations(grayTemplates.size());
//    std::vector<std::vector<double>> allMatchScores(grayTemplates.size());

//    // =========================================================================
//    // --- 2. 遍历模板进行匹配并收集结果 (CPU 多线程并行加速版) ---
//    // =========================================================================
//    cv::parallel_for_(cv::Range(0, grayTemplates.size()), [&](const cv::Range& range) {
//        for (int i = range.start; i < range.end; ++i) {
//            const cv::Mat& tmpl = grayTemplates[i];

//            // 检查模板是否有效，并且尺寸是否小于目标图像，否则跳过
//            if (tmpl.empty() || grayTarget.cols < tmpl.cols || grayTarget.rows < tmpl.rows) {
//                // 多线程内部去掉了 qDebug 打印，防止控制台 I/O 争抢拖慢速度
//                continue;
//            }

//            // 执行模板匹配：使用TM_CCOEFF_NORMED方法 (局部变量 result 保证线程安全)
//            cv::Mat result;
//            cv::matchTemplate(grayTarget, tmpl, result, cv::TM_CCOEFF_NORMED);

//            // 遍历匹配结果，收集所有分数超过阈值的匹配位置和分数
//            for (int y = 0; y < result.rows; ++y) {
//                const float* row = result.ptr<float>(y);
//                for (int x = 0; x < result.cols; ++x) {
//                    if (row[x] >= threshold) {
//                        // 独占索引 i，无需加锁，绝对安全
//                        allMatchLocations[i].emplace_back(x, y, tmpl.cols, tmpl.rows);
//                        allMatchScores[i].push_back(row[x]);
//                    }
//                }
//            }
//        }
//    });

//    // =========================================================================
//    // --- 3. 为每个模板按匹配分数排序（降序） (CPU 多线程并行加速版) ---
//    // =========================================================================
//    cv::parallel_for_(cv::Range(0, allMatchLocations.size()), [&](const cv::Range& range) {
//        for (int i = range.start; i < range.end; ++i) {
//            if (allMatchLocations[i].empty()) continue;

//            // 组合分数和位置
//            std::vector<std::pair<double, cv::Rect>> matches;
//            matches.reserve(allMatchLocations[i].size()); // 提前分配内存，提升速度
//            for (size_t j = 0; j < allMatchLocations[i].size(); j++) {
//                matches.emplace_back(allMatchScores[i][j], allMatchLocations[i][j]);
//            }

//            // 按分数降序排序
//            std::sort(matches.begin(), matches.end(),
//                [](const auto& a, const auto& b) {
//                    return a.first > b.first;
//                });

//            // 将排序后的结果写回
//            allMatchLocations[i].clear();
//            allMatchScores[i].clear();
//            for (const auto& match : matches) {
//                allMatchLocations[i].push_back(match.second);
//                allMatchScores[i].push_back(match.first);
//            }
//        }
//    });

//    // =========================================================================
//    // 以下部分保持串行，因为它们存在逻辑依赖或 UI 更新操作，不适合多线程
//    // =========================================================================

//    // --- 4. 非重叠匹配位置选择 ---
//    std::vector<std::vector<cv::Rect>> filteredLocations(grayTemplates.size());
//    std::vector<std::vector<double>> filteredScores(grayTemplates.size());
//    std::vector<cv::Rect> selectedLocations;  // 存储已选择的矩形

//    // 按模板顺序处理（前面模板优先）
//    for (size_t i = 0; i < allMatchLocations.size(); ++i) {
//        bool found = false;

//        // 遍历当前模板的所有匹配位置（已按分数降序排列）
//        for (size_t j = 0; j < allMatchLocations[i].size(); ++j) {
//            const cv::Rect& candidateRect = allMatchLocations[i][j];
//            bool overlap = false;

//            // 检查是否与任何已选位置重叠
//            for (const cv::Rect& selectedRect : selectedLocations) {
//                // 计算IoU
//                double iou = calculateIOU(candidateRect, selectedRect);
//                if (iou > 0.3) {  // IoU阈值设为0.3
//                    overlap = true;
//                    break;
//                }
//            }

//            // 如果没重叠则选择该位置
//            if (!overlap) {
//                // 添加到最终结果
//                filteredLocations[i].push_back(candidateRect);
//                filteredScores[i].push_back(allMatchScores[i][j]);

//                // 加入已选择集合
//                selectedLocations.push_back(candidateRect);
//                found = true;
//                break;  // 只需当前模板的一个位置
//            }
//        }

//        // 可选：如果当前模板没有找到不重叠的位置，记录日志
//        if (!found) {
//            qDebug() << "[INFO] No non-overlapping match found for template index: " << i;
//        }
//    }

//    // 将过滤后的结果赋值回原始变量
//    allMatchLocations = std::move(filteredLocations);
//    allMatchScores = std::move(filteredScores);


//    // --- 4. 尺寸统一与行内高度对齐的核心逻辑 ---

//    // 4.1 计算所有模板的平均尺寸（包含宽度和高度）。
//    double totalAvgWidth = 0.0;
//    double totalAvgHeight = 0.0;
//    int validTemplateCount = 0;

//    for (const cv::Mat& tmpl : grayTemplates) {
//        if (!tmpl.empty() && tmpl.rows > 0 && tmpl.cols > 0) {
//            totalAvgWidth += tmpl.cols;
//            totalAvgHeight += tmpl.rows;
//            validTemplateCount++;
//        }
//    }

//    const int globalAvgWidth = std::max(1, static_cast<int>(std::round(totalAvgWidth / (validTemplateCount > 0 ? validTemplateCount : 1))));
//    const int globalAvgHeight = std::max(1, static_cast<int>(std::round(totalAvgHeight / (validTemplateCount > 0 ? validTemplateCount : 1))));

//    // 4.2 将所有矩形框的尺寸统一为计算出的全局平均尺寸。
//    for (auto& locs : allMatchLocations) {
//        for (cv::Rect& rect : locs) {
//            const cv::Point currentCenter = rect.tl() + cv::Point(rect.width / 2, rect.height / 2);
//            rect = cv::Rect(
//                currentCenter.x - globalAvgWidth / 2,   // 新的 x 坐标
//                currentCenter.y - globalAvgHeight / 2,  // 新的 y 坐标
//                globalAvgWidth,                         // 新的宽度，统一为平均宽度
//                globalAvgHeight                         // 新的高度，统一为平均高度
//            );
//        }
//    }

//    // 4.3 按行统一矩形高度（高度差小于10像素的矩形视为同一行）。
//    const int rowThreshold = 10; // 行距阈值，Y坐标差小于此值视为同一行

//    std::vector<cv::Rect*> allRectPtrs;
//    for (auto& locs : allMatchLocations) {
//        for (cv::Rect& rect : locs) {
//            allRectPtrs.push_back(&rect);
//        }
//    }

//    if (allRectPtrs.empty()) {
//        qDebug() << "[DEBUG] No rectangles found for row-wise height unification.";
//    } else {
//        std::sort(allRectPtrs.begin(), allRectPtrs.end(), [](const cv::Rect* a, const cv::Rect* b) {
//            return a->y < b->y;
//        });

//        std::vector<std::vector<cv::Rect*>> groupedRows;
//        std::vector<cv::Rect*> currentRow;
//        currentRow.push_back(allRectPtrs[0]);
//        int base_y_for_row = allRectPtrs[0]->y;

//        for (size_t i = 1; i < allRectPtrs.size(); ++i) {
//            cv::Rect* currentRect = allRectPtrs[i];
//            if (std::abs(currentRect->y - base_y_for_row) <= rowThreshold) {
//                currentRow.push_back(currentRect);
//            } else {
//                groupedRows.push_back(currentRow);
//                currentRow.clear();
//                currentRow.push_back(currentRect);
//                base_y_for_row = currentRect->y;
//            }
//        }
//        if (!currentRow.empty()) groupedRows.push_back(currentRow);
//    }

//// --- 5. 存储匹配结果并关闭底层弹窗 ---
//        std::vector<std::tuple<cv::Rect, double, size_t>> sortedMatches;
//        for (size_t i = 0; i < allMatchLocations.size(); ++i) {
//            if (!allMatchLocations[i].empty()) {
//                sortedMatches.emplace_back(
//                    allMatchLocations[i][0],     // 只取第一个匹配矩形
//                    allMatchScores[i][0],        // 对应的匹配分数
//                    i                            // 原始模板的索引
//                );
//            }
//        }

//        // 按矩形的左上角坐标排序
//        std::sort(sortedMatches.begin(), sortedMatches.end(),
//            [](const auto& a, const auto& b) {
//                const cv::Rect& rectA = std::get<0>(a);
//                const cv::Rect& rectB = std::get<0>(b);
//                return (rectA.y < rectB.y) || (rectA.y == rectB.y && rectA.x < rectB.x);
//        });

//        // ================= 核心修改 =================
//        // 将排序后的结果保存到类成员，供 Widget 读取，绝不在这里进行任何 imshow 操作！
//        this->lastMatchResults = sortedMatches;
//        // ============================================

//        // 计算实际的矩形框数量
//        int totalDetectedRects = 0;
//        for (const auto& locs : allMatchLocations) {
//            totalDetectedRects += static_cast<int>(locs.size());
//        }

//        return totalDetectedRects;
//}




//------------------------【6】其他--------------------------------//
void TemplateMatch::jianceshibiestr(String string1)
{
    input = string1;
}
