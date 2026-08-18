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

    const DetectionModeDescriptor &descriptor =
            detectionModeDescriptor(input.mode);
    if (descriptor.trackingKind == DetectionTrackingKind::WholeFrame) {
        return InspectionStartPreflightResult();
    }

    const bool wordProfileMode =
            descriptor.trackingKind
            == DetectionTrackingKind::MultipleProfiles;
    if (wordProfileMode && input.profiles.isEmpty()) {
        return rejected(InspectionStartIssue::WordProfilesMissing);
    }

    if (descriptor.requiresBarcodeDecoder) {
        QStringList errors;
        if (!input.barcodeDecoderReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"读码组件不可用：%1")
                        .arg(input.barcodeDecoderError));
        }

        for (const InspectionStartProfileReadiness &profile
             : input.profiles) {
            QStringList profileErrors;
            if (!profile.trackingTemplateReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"定位模板 tracking_template.bmp "
                                L"缺失或无法读取"));
            }
            if (!profile.calibrationReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"calibrate_config.yaml "
                                L"缺失或无法读取"));
            } else {
                if (!profile.barcodeRegionReady) {
                    profileErrors.append(
                                QString::fromWCharArray(
                                    L"二维码区域 barcode_poly "
                                    L"必须包含4个点"));
                }
                if (!profile.dateRegionReady) {
                    profileErrors.append(
                                QString::fromWCharArray(
                                    L"日期区域 date_poly "
                                    L"至少需要3个点"));
                }
            }
            if (!profile.targetTextReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"目标字符尚未设置"));
            }
            if (!profile.characterTemplatesReady) {
                profileErrors.append(
                            QString::fromWCharArray(
                                L"字符模板缺失或"
                                L"索引配置无效"));
            }

            if (!profileErrors.isEmpty()) {
                errors.append(
                            QString::fromWCharArray(
                                L"模板“%1”：%2")
                            .arg(profileName(profile))
                            .arg(profileErrors.join(
                                     QString::fromWCharArray(L"；"))));
            }
        }

        if (!errors.isEmpty()) {
            return rejected(
                        InspectionStartIssue::BarcodeResourcesInvalid,
                        errors);
        }
    }

    if (descriptor.trackingKind
            == DetectionTrackingKind::SingleTemplate) {
        QStringList errors;
        if (!input.trackingTemplateReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"定位模板图片 tracking_template.bmp "
                            L"缺失或读取失败"));
        }
        if (!input.dateRegionReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"喷码检测区域 "
                            L"calibrate_config.yaml/date_poly "
                            L"缺失或读取失败"));
        }
        if (input.targetTextRequired && !input.targetTextReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"目标字符尚未设置"));
        }
        if (input.characterTemplatesRequired
                && !input.characterTemplatesReady) {
            errors.append(
                        QString::fromWCharArray(
                            L"字符模板缺失或"
                            L"索引配置无效"));
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
