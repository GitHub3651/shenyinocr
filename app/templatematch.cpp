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

double TemplateMatch::calculateIOU(const cv::Rect& rectA, const cv::Rect& rectB) {
    cv::Rect intersection = rectA & rectB;
    if (intersection.area() <= 0) return 0.0;
    double unionArea = rectA.area() + rectB.area() - intersection.area();
    return intersection.area() / unionArea;
}




TemplateMatchPreparedTemplates TemplateMatch::prepareDigitTemplates(
    const std::vector<cv::Mat> &digitTemplates)
{
    return CharacterTemplateMatcher::prepare(digitTemplates);

#if 0 // Kept until the shared matcher passes the Stage 3 parity gate.
    TemplateMatchPreparedTemplates prepared;
    prepared.grayTemplates.reserve(digitTemplates.size());
    prepared.smallTemplates.reserve(digitTemplates.size());

    const double scale = 0.5;
    for (const cv::Mat &digitTemplate : digitTemplates) {
        cv::Mat grayTemplate;
        if (digitTemplate.empty()) {
            prepared.grayTemplates.emplace_back();
            prepared.smallTemplates.emplace_back();
            continue;
        }

        if (digitTemplate.channels() == 3) {
            cv::cvtColor(
                        digitTemplate,
                        grayTemplate,
                        cv::COLOR_BGR2GRAY);
        } else if (digitTemplate.channels() == 4) {
            cv::cvtColor(
                        digitTemplate,
                        grayTemplate,
                        cv::COLOR_BGRA2GRAY);
        } else {
            grayTemplate =
                    digitTemplate.clone();
        }

        cv::Mat smallTemplate;
        if (!grayTemplate.empty()) {
            cv::resize(
                        grayTemplate,
                        smallTemplate,
                        cv::Size(),
                        scale,
                        scale,
                        cv::INTER_LINEAR);
        }
        prepared.grayTemplates.push_back(
                    grayTemplate);
        prepared.smallTemplates.push_back(
                    smallTemplate);
    }

    return prepared;
#endif
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
    if (!imgshibie) {
        qDebug() << "[ERROR] Target image pointer is null, cannot proceed with matching.";
        lastMatchResults.clear();
        return 0;
    }

    Mat targetImage;
    imgshibie->copyTo(targetImage);
    const TemplateMatchPreparedTemplates preparedTemplates =
            prepareDigitTemplates(
                digitTemplates);
    return run3(
                targetImage,
                preparedTemplates,
                templateTargetIndexes);
}

int TemplateMatch::run3(
    const cv::Mat &targetImage,
    const TemplateMatchPreparedTemplates &preparedTemplates,
    const std::vector<int> &templateTargetIndexes)
{
    const CharacterTemplateMatchResult matchResult =
            CharacterTemplateMatcher::match(
                targetImage,
                preparedTemplates,
                templateTargetIndexes,
                value);
    lastMatchResults = matchResult.matches;
    return matchResult.detectedCount;

#if 0 // Kept until the shared matcher passes the Stage 3 parity gate.
    cv::Mat targetGrayImage;

    // --- 1. 基础检查与图像预处理 ---
    if (!preparedTemplates.isValid() || targetImage.empty()) {
        qDebug() << "[ERROR] Templates or target image is empty, cannot proceed with matching.";
        lastMatchResults.clear();
        return 0.0;
    }

    // Keep the original run3 input rule: the date ROI is always BGR.
    cv::cvtColor(
                targetImage,
                targetGrayImage,
                cv::COLOR_BGR2GRAY);

    const std::vector<cv::Mat> &grayTemplates =
            preparedTemplates.grayTemplates;
    const std::vector<cv::Mat> &smallTemplates =
            preparedTemplates.smallTemplates;

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

    // =========================================================================
    // --- 2. 降采样 + CPU 多线程并行加速版 ---
    // =========================================================================

    // 设定降采样比例（0.5 表示宽高各缩小一半，计算量直接降为原来的 1/4 到 1/16）
    const double scale = 0.5;

    cv::Mat smallTarget;
    // 缩小目标大图
    cv::resize(targetGrayImage, smallTarget, cv::Size(), scale, scale, cv::INTER_LINEAR);

    std::vector<cv::Mat> matchScoreMaps(grayTemplates.size());
    cv::parallel_for_(cv::Range(0, grayTemplates.size()), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; ++i) {
            const cv::Mat& smallTmpl = smallTemplates[i];

            // 检查模板是否有效，并且尺寸是否小于缩小后的目标图像，否则跳过
            if (smallTmpl.empty() || smallTarget.cols < smallTmpl.cols || smallTarget.rows < smallTmpl.rows) {
                continue;
            }

            cv::matchTemplate(
                        smallTarget,
                        smallTmpl,
                        matchScoreMaps[static_cast<size_t>(i)],
                        cv::TM_CCOEFF_NORMED);
        }
    });

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

    std::vector<std::vector<cv::Rect>> allMatchLocations(grayTemplates.size());
    std::vector<std::vector<double>> allMatchScores(grayTemplates.size());
    std::vector<cv::Rect> selectedLocations;

    // Scan score maps directly. This keeps the same threshold, target order
    // and IoU rule without allocating and sorting every threshold candidate.
    for (int targetIndex = 0; targetIndex < targetGroupCount; ++targetIndex) {
        bool found = false;
        double bestScore = threshold - 1.0;
        cv::Rect bestRect;
        size_t bestTemplateIndex = 0;

        for (size_t templateIndex : templateIndexesByTarget[static_cast<size_t>(targetIndex)]) {
            const cv::Mat &scoreMap =
                    matchScoreMaps[templateIndex];
            const cv::Mat &originalTemplate =
                    grayTemplates[templateIndex];
            if (scoreMap.empty() || originalTemplate.empty()) {
                continue;
            }

            for (int y = 0; y < scoreMap.rows; ++y) {
                const float *scoreRow =
                        scoreMap.ptr<float>(y);
                for (int x = 0; x < scoreMap.cols; ++x) {
                    const double score =
                            scoreRow[x];
                    if (score < threshold || score <= bestScore) {
                        continue;
                    }

                    const cv::Rect candidateRect(
                                static_cast<int>(
                                    std::round(x / scale)),
                                static_cast<int>(
                                    std::round(y / scale)),
                                originalTemplate.cols,
                                originalTemplate.rows);
                    bool overlapsSelected = false;
                    for (const cv::Rect &selectedRect : selectedLocations) {
                        if (calculateIOU(
                                    candidateRect,
                                    selectedRect) > 0.3) {
                            overlapsSelected = true;
                            break;
                        }
                    }
                    if (overlapsSelected) {
                        continue;
                    }

                    bestScore = score;
                    bestRect = candidateRect;
                    bestTemplateIndex = templateIndex;
                    found = true;
                }
            }
        }

        if (found) {
            allMatchLocations[bestTemplateIndex].push_back(
                        bestRect);
            allMatchScores[bestTemplateIndex].push_back(
                        bestScore);
            selectedLocations.push_back(
                        bestRect);
        } else {
            qDebug() << "[INFO] No non-overlapping match found for target index: " << targetIndex;
        }
    }


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
#endif
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
