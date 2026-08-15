#include "runtime/inspection_camera_operations.h"

#include <QDebug>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {

InspectionCameraParameterResult queryIntegerRange(
    ICameraDevice *cameraDevice,
    const char *key,
    const QString &missingDiagnostic,
    const QString &readDiagnostic,
    const QString &invalidDiagnostic)
{
    InspectionCameraParameterResult output;
    if (!cameraDevice) {
        output.diagnostic = missingDiagnostic;
        return output;
    }

    CameraFloatValue value;
    const CameraOperationResult result =
        cameraDevice->getFloatValue(key, &value);
    output.nativeErrorCode = result.nativeErrorCode;
    if (!result.isSuccess()) {
        output.diagnostic = readDiagnostic.arg(result.nativeErrorCode);
        return output;
    }

    const double integerMinimum = std::ceil(value.minimumValue);
    const double integerMaximum = std::floor(value.maximumValue);
    if (!std::isfinite(value.minimumValue)
            || !std::isfinite(value.maximumValue)
            || !std::isfinite(value.currentValue)
            || integerMinimum > integerMaximum
            || integerMinimum
               < static_cast<double>((std::numeric_limits<int>::min)())
            || integerMaximum
               > static_cast<double>((std::numeric_limits<int>::max)())) {
        output.diagnostic = invalidDiagnostic
            .arg(value.minimumValue)
            .arg(value.maximumValue);
        return output;
    }

    output.success = true;
    output.minimumValue = static_cast<int>(integerMinimum);
    output.maximumValue = static_cast<int>(integerMaximum);
    output.actualValue = value.currentValue;
    return output;
}

} // namespace

InspectionCameraOperations::InspectionCameraOperations(
    ICameraDevice *cameraDevice)
    : m_cameraDevice(cameraDevice)
{
}

void InspectionCameraOperations::setCameraDevice(
    ICameraDevice *cameraDevice)
{
    m_cameraDevice = cameraDevice;
}

InspectionCameraParameterResult
InspectionCameraOperations::queryExposureRange() const
{
    return queryIntegerRange(
        m_cameraDevice,
        "ExposureTime",
        QStringLiteral("\u76f8\u673a\u672a\u521d\u59cb\u5316\uff0c\u65e0\u6cd5\u8bfb\u53d6\u66dd\u5149\u8303\u56f4"),
        QStringLiteral("\u8bfb\u53d6\u76f8\u673a\u66dd\u5149\u8303\u56f4\u5931\u8d25\uff0c\u9519\u8bef\u7801\uff1a%1"),
        QStringLiteral("\u76f8\u673a\u66dd\u5149\u8303\u56f4\u65e0\u6cd5\u8f6c\u6362\u4e3a\u6574\u6570\uff1a%1 ~ %2"));
}

InspectionCameraParameterResult
InspectionCameraOperations::queryGainRange() const
{
    return queryIntegerRange(
        m_cameraDevice,
        "Gain",
        QStringLiteral("\u76f8\u673a\u672a\u521d\u59cb\u5316\u6216\u672a\u6253\u5f00\uff0c\u65e0\u6cd5\u8bfb\u53d6\u589e\u76ca\u8303\u56f4\uff01"),
        QStringLiteral("\u65e0\u6cd5\u83b7\u53d6\u76f8\u673a\u589e\u76ca\u652f\u6301\u7684\u8303\u56f4\uff01\u9519\u8bef\u7801\uff1a%1"),
        QStringLiteral("\u76f8\u673a\u589e\u76ca\u8303\u56f4\u65e0\u6cd5\u8f6c\u6362\u4e3a\u6574\u6570\uff1a%1 ~ %2"));
}

InspectionCameraParameterResult
InspectionCameraOperations::applyExposure(int exposureValue) const
{
    InspectionCameraParameterResult output = queryExposureRange();
    if (!output.success) {
        return output;
    }
    if (exposureValue < output.minimumValue
            || exposureValue > output.maximumValue) {
        output.success = false;
        output.diagnostic = QStringLiteral(
            "\u66dd\u5149\u503c %1 \u8d85\u51fa\u5f53\u524d\u76f8\u673a\u5141\u8bb8\u8303\u56f4\uff1a%2 ~ %3")
            .arg(exposureValue)
            .arg(output.minimumValue)
            .arg(output.maximumValue);
        return output;
    }

    CameraOperationResult result = m_cameraDevice->setFloatValue(
        "ExposureTime", static_cast<float>(exposureValue));
    output.nativeErrorCode = result.nativeErrorCode;
    if (!result.isSuccess()) {
        output.success = false;
        output.diagnostic = QStringLiteral(
            "\u76f8\u673a\u66dd\u5149\u8bbe\u7f6e\u5931\u8d25\uff0c\u9519\u8bef\u7801\uff1a%1")
            .arg(result.nativeErrorCode);
        return output;
    }

    CameraFloatValue readBack;
    result = m_cameraDevice->getFloatValue("ExposureTime", &readBack);
    output.nativeErrorCode = result.nativeErrorCode;
    output.actualValue = readBack.currentValue;
    if (!result.isSuccess()
            || !std::isfinite(output.actualValue)
            || std::fabs(output.actualValue - exposureValue) > 0.5) {
        output.success = false;
        output.diagnostic = QStringLiteral(
            "\u76f8\u673a\u66dd\u5149\u5199\u5165\u9a8c\u8bc1\u5931\u8d25\uff1a\u8bbe\u7f6e %1\uff0c\u5b9e\u9645 %2\uff0c\u9519\u8bef\u7801 %3")
            .arg(exposureValue)
            .arg(output.actualValue)
            .arg(result.nativeErrorCode);
        return output;
    }

    qDebug() << "SetExposureTime verified:"
             << exposureValue
             << "range:" << output.minimumValue
             << "~" << output.maximumValue
             << "actual:" << output.actualValue;
    return output;
}

InspectionCameraParameterResult
InspectionCameraOperations::applyGain(int gainValue) const
{
    InspectionCameraParameterResult output = queryGainRange();
    if (!output.success) {
        return output;
    }
    if (gainValue < output.minimumValue
            || gainValue > output.maximumValue) {
        output.success = false;
        output.diagnostic = QStringLiteral(
            "\u8f93\u5165\u7684\u589e\u76ca\u503c\u8d85\u51fa\u9650\u5236\uff01\u5f53\u524d\u76f8\u673a\u5141\u8bb8\u8303\u56f4\uff1a%1 ~ %2")
            .arg(output.minimumValue)
            .arg(output.maximumValue);
        return output;
    }
    const CameraOperationResult result = m_cameraDevice->setFloatValue(
        "Gain", static_cast<float>(gainValue));
    output.nativeErrorCode = result.nativeErrorCode;
    if (!result.isSuccess()) {
        output.success = false;
        output.diagnostic = QStringLiteral(
            "\u76f8\u673a\u589e\u76ca\u8bbe\u7f6e\u5931\u8d25\uff01\u9519\u8bef\u7801\uff1a%1")
            .arg(result.nativeErrorCode);
        return output;
    }
    output.actualValue = gainValue;
    return output;
}

InspectionCameraParameterResult
InspectionCameraOperations::applySavedExposure(
    int savedExposure,
    const std::function<bool(int, QString *)> &persistAdjustedExposure) const
{
    const InspectionCameraParameterResult range = queryExposureRange();
    if (!range.success) {
        return range;
    }
    const int adjusted = qBound(
        range.minimumValue, savedExposure, range.maximumValue);
    InspectionCameraParameterResult output = applyExposure(adjusted);
    if (!output.success) {
        return output;
    }
    output.actualValue = adjusted;
    if (adjusted != savedExposure && persistAdjustedExposure) {
        QString error;
        if (!persistAdjustedExposure(adjusted, &error)) {
            output.success = false;
            output.diagnostic = error.isEmpty()
                ? QStringLiteral("\u66dd\u5149\u503c\u5df2\u6839\u636e\u76f8\u673a\u8303\u56f4\u8c03\u6574\uff0c\u4f46\u516c\u5171\u914d\u7f6e\u4fdd\u5b58\u5931\u8d25")
                : error;
        }
    }
    return output;
}

InspectionCameraOpenResult InspectionCameraOperations::openFirstCamera(
    int savedExposure,
    const std::function<bool(int, QString *)> &persistAdjustedExposure,
    const std::function<void()> &ensureWorkersReady,
    int knownDeviceCount) const
{
    InspectionCameraOpenResult output;
    if (!m_cameraDevice) {
        output.issue = InspectionCameraOpenIssue::CameraUnavailable;
        output.diagnostic = QStringLiteral("Camera device is unavailable.");
        return output;
    }
    if (knownDeviceCount >= 0) {
        output.deviceCount = knownDeviceCount;
    } else {
        const CameraOperationResult result =
            m_cameraDevice->enumerateDevices(&output.deviceCount);
        if (!result.isSuccess()) {
            output.issue = InspectionCameraOpenIssue::DeviceNotFound;
            output.diagnostic = QString::number(result.nativeErrorCode);
            return output;
        }
    }
    if (output.deviceCount <= 0) {
        output.issue = InspectionCameraOpenIssue::DeviceNotFound;
        return output;
    }
    const CameraOperationResult open = m_cameraDevice->openDevice(0);
    if (!open.isSuccess()) {
        output.issue = InspectionCameraOpenIssue::DeviceOpenFailed;
        output.diagnostic = QString::number(open.nativeErrorCode);
        return output;
    }

    m_cameraDevice->setEnumValue("TriggerMode", 1);
    m_cameraDevice->setEnumValue("TriggerSource", 0);
    const InspectionCameraParameterResult exposure = applySavedExposure(
        savedExposure, persistAdjustedExposure);
    if (!exposure.success) {
        m_cameraDevice->close();
        output.issue = InspectionCameraOpenIssue::ExposureFailed;
        output.diagnostic = exposure.diagnostic;
        return output;
    }
    output.appliedExposure = static_cast<int>(exposure.actualValue);
    output.exposureMinimum = exposure.minimumValue;
    output.exposureMaximum = exposure.maximumValue;
    output.exposureAdjusted = output.appliedExposure != savedExposure;
    if (output.exposureAdjusted) {
        output.adjustmentMessage = QStringLiteral(
            "\u539f\u66dd\u5149\u503c %1 \u8d85\u51fa\u5f53\u524d\u76f8\u673a\u5141\u8bb8\u8303\u56f4\uff08%2 ~ %3\uff09\uff0c\u5df2\u8c03\u6574\u4e3a %4\u3002")
            .arg(savedExposure)
            .arg(exposure.minimumValue)
            .arg(exposure.maximumValue)
            .arg(output.appliedExposure);
    }
    m_cameraDevice->setFloatValue("TriggerDelay", 0);
    m_cameraDevice->registerImageCallback();
    m_cameraDevice->startGrabbing();
    if (ensureWorkersReady) {
        ensureWorkersReady();
    }
    return output;
}
