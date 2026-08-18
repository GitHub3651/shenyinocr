// 文件作用：本文件用于在启动检测前检查相机、PLC、配方和当前运行状态是否满足条件。
// 主要职责：在启动检测前检查相机、PLC、配方和当前运行状态是否满足条件。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "application/inspection_start_preflight.h"

namespace {
// 函数说明：rejected 函数实现名称所表示的处理步骤。
InspectionStartPreflightResult rejected(
    InspectionStartIssue issue,
    const QStringList &details = QStringList())
{
    InspectionStartPreflightResult result;
    result.issue = issue;
    result.details = details;
    return result;
}

// 函数说明：profileName 函数实现名称所表示的处理步骤。
QString profileName(const InspectionStartProfileReadiness &profile)
{
    return profile.displayName;
}
}

// 函数说明：evaluateAccess 函数实现名称所表示的处理步骤。
InspectionStartPreflightResult InspectionStartPreflight::evaluateAccess(
    const InspectionStartAccessInput &input)
{
    if (input.templateOperationActive) {
        return rejected(InspectionStartIssue::TemplateOperationActive);
    }
    if (input.runtimeBusy) {
        return rejected(InspectionStartIssue::RuntimeBusy);
    }
    if (!input.cameraOpen) {
        return rejected(InspectionStartIssue::CameraClosed);
    }
    if (input.dirtySettings) {
        return rejected(
                    InspectionStartIssue::DirtySettingsConfirmationRequired);
    }
    if (input.plcTriggerEnabled && !input.plcConnected) {
        return rejected(InspectionStartIssue::PlcDisconnected);
    }
    return InspectionStartPreflightResult();
}

// 函数说明：evaluateResources 函数实现名称所表示的处理步骤。
InspectionStartPreflightResult InspectionStartPreflight::evaluateResources(
    const InspectionStartResourceInput &input)
{
    if (!input.preparedRecipeReady) {
        return rejected(InspectionStartIssue::PreparedRecipeMissing);
    }

    if (input.modeKind == InspectionStartModeKind::Tissue) {
        return InspectionStartPreflightResult();
    }

    const bool wordProfileMode =
            input.modeKind == InspectionStartModeKind::WordProfiles
            || input.modeKind
               == InspectionStartModeKind::BarcodeWordProfiles;
    if (wordProfileMode && input.profiles.isEmpty()) {
        return rejected(InspectionStartIssue::WordProfilesMissing);
    }

    if (input.modeKind
            == InspectionStartModeKind::BarcodeWordProfiles) {
        QStringList errors;
        if (!input.barcodeDecoderReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"\u8bfb\u7801\u7ec4\u4ef6\u4e0d\u53ef\u7528\uff1a%1")
                        .arg(input.barcodeDecoderError));
        }

        for (const InspectionStartProfileReadiness &profile
             : input.profiles) {
            QStringList profileErrors;
            if (!profile.trackingTemplateReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"\u5b9a\u4f4d\u6a21\u677f tracking_template.bmp "
                                L"\u7f3a\u5931\u6216\u65e0\u6cd5\u8bfb\u53d6"));
            }
            if (!profile.calibrationReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"calibrate_config.yaml "
                                L"\u7f3a\u5931\u6216\u65e0\u6cd5\u8bfb\u53d6"));
            } else {
                if (!profile.barcodeRegionReady) {
                    profileErrors.append(
                                QString::fromWCharArray(
                                    L"\u4e8c\u7ef4\u7801\u533a\u57df barcode_poly "
                                    L"\u5fc5\u987b\u5305\u542b4\u4e2a\u70b9"));
                }
                if (!profile.dateRegionReady) {
                    profileErrors.append(
                                QString::fromWCharArray(
                                    L"\u65e5\u671f\u533a\u57df date_poly "
                                    L"\u81f3\u5c11\u9700\u89813\u4e2a\u70b9"));
                }
            }
            if (!profile.targetTextReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"\u76ee\u6807\u5b57\u7b26\u5c1a\u672a\u8bbe\u7f6e"));
            }
            if (!profile.characterTemplatesReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"\u5b57\u7b26\u6a21\u677f\u7f3a\u5931\u6216"
                                L"\u7d22\u5f15\u914d\u7f6e\u65e0\u6548"));
            }

            if (!profileErrors.isEmpty()) {
                errors.append(
                            QString::fromWCharArray(
                                L"\u6a21\u677f\u201c%1\u201d\uff1a%2")
                            .arg(profileName(profile))
                            .arg(profileErrors.join(
                                     QString::fromWCharArray(L"\uff1b"))));
            }
        }

        if (!errors.isEmpty()) {
            return rejected(
                        InspectionStartIssue::BarcodeResourcesInvalid,
                        errors);
        }
    }

    if (input.modeKind == InspectionStartModeKind::SingleTemplate) {
        QStringList errors;
        if (!input.trackingTemplateReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"\u5b9a\u4f4d\u6a21\u677f\u56fe\u7247 tracking_template.bmp "
                            L"\u7f3a\u5931\u6216\u8bfb\u53d6\u5931\u8d25"));
        }
        if (!input.dateRegionReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"\u55b7\u7801\u68c0\u6d4b\u533a\u57df "
                            L"calibrate_config.yaml/date_poly "
                            L"\u7f3a\u5931\u6216\u8bfb\u53d6\u5931\u8d25"));
        }
        if (input.targetTextRequired && !input.targetTextReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"\u76ee\u6807\u5b57\u7b26\u5c1a\u672a\u8bbe\u7f6e"));
        }
        if (input.characterTemplatesRequired
                && !input.characterTemplatesReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"\u5b57\u7b26\u6a21\u677f\u7f3a\u5931\u6216"
                            L"\u7d22\u5f15\u914d\u7f6e\u65e0\u6548"));
        }
        if (!errors.isEmpty()) {
            return rejected(
                        InspectionStartIssue::ProductTemplateIncomplete,
                        errors);
        }
    }

    if (wordProfileMode) {
        QStringList incompleteProfiles;
        for (const InspectionStartProfileReadiness &profile
             : input.profiles) {
            if (!profile.targetTextReady
                    || !profile.characterTemplatesReady) {
                incompleteProfiles.append(profileName(profile));
            }
        }
        if (!incompleteProfiles.isEmpty()) {
            return rejected(
                        InspectionStartIssue::WordProfilesIncomplete,
                        incompleteProfiles);
        }
    }

    return InspectionStartPreflightResult();
}
