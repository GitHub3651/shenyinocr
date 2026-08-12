#include <QtTest/QtTest>

#include "detection/common/profile_pose_selector.h"

class ProfilePoseSelectorTest : public QObject
{
    Q_OBJECT

private slots:
    void invalidCandidatesAreIgnored();
    void highestScoreCandidateIsSelected();
    void equalScoreRetainsFirstCandidate();
    void selectedBarcodePolygonUsesPoseTransform();
};

namespace {

DetectionPose validPose(float score,
                        const cv::Point2f &center = cv::Point2f())
{
    DetectionPose pose;
    pose.valid = true;
    pose.score = score;
    pose.anchorCenter = center;
    return pose;
}

} // namespace

void ProfilePoseSelectorTest::invalidCandidatesAreIgnored()
{
    ProfilePoseSelector selector;
    DetectionPose invalidPose;

    QVERIFY(!selector.consider(invalidPose,
                               3,
                               QStringLiteral("invalid"),
                               std::vector<cv::Point2f>()));
    QVERIFY(!selector.selection().pose.valid);
    QCOMPARE(selector.selection().pose.wordTemplateProfileIndex, -1);
    QVERIFY(selector.selection().profileName.isEmpty());
}

void ProfilePoseSelectorTest::highestScoreCandidateIsSelected()
{
    ProfilePoseSelector selector;

    QVERIFY(selector.consider(validPose(0.62f),
                              4,
                              QStringLiteral("profile-four"),
                              std::vector<cv::Point2f>()));
    QVERIFY(selector.consider(validPose(0.91f),
                              7,
                              QStringLiteral("profile-seven"),
                              std::vector<cv::Point2f>()));
    QVERIFY(!selector.consider(validPose(0.73f),
                               9,
                               QStringLiteral("profile-nine"),
                               std::vector<cv::Point2f>()));

    QVERIFY(selector.selection().pose.valid);
    QCOMPARE(selector.selection().pose.wordTemplateProfileIndex, 7);
    QCOMPARE(selector.selection().profileName,
             QStringLiteral("profile-seven"));
    QCOMPARE(selector.selection().pose.score, 0.91f);
}

void ProfilePoseSelectorTest::equalScoreRetainsFirstCandidate()
{
    ProfilePoseSelector selector;

    QVERIFY(selector.consider(validPose(0.8f),
                              1,
                              QStringLiteral("first"),
                              std::vector<cv::Point2f>()));
    QVERIFY(!selector.consider(validPose(0.8f),
                               2,
                               QStringLiteral("second"),
                               std::vector<cv::Point2f>()));

    QCOMPARE(selector.selection().pose.wordTemplateProfileIndex, 1);
    QCOMPARE(selector.selection().profileName, QStringLiteral("first"));
}

void ProfilePoseSelectorTest::selectedBarcodePolygonUsesPoseTransform()
{
    ProfilePoseSelector selector;
    DetectionPose pose = validPose(0.75f, cv::Point2f(100.0f, 200.0f));
    pose.angleDeg = 90.0f;

    const std::vector<cv::Point2f> relativeBarcodePoly = {
        cv::Point2f(10.0f, 0.0f),
        cv::Point2f(0.0f, 20.0f)
    };
    QVERIFY(selector.consider(pose,
                              5,
                              QStringLiteral("rotated"),
                              relativeBarcodePoly));

    const std::vector<cv::Point> &barcodePoly =
            selector.selection().pose.barcodePoly;
    QCOMPARE(static_cast<int>(barcodePoly.size()), 2);
    QCOMPARE(barcodePoly[0].x, 100);
    QCOMPARE(barcodePoly[0].y, 190);
    QCOMPARE(barcodePoly[1].x, 120);
    QCOMPARE(barcodePoly[1].y, 200);
}

QTEST_APPLESS_MAIN(ProfilePoseSelectorTest)

#include "profile_pose_selector_test.moc"
