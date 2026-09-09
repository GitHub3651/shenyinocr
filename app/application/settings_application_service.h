// 文件作用：本文件用于组织软件设置读取与保存用例。
// 主要职责：维护当前软件设置，并协调设置与模板配置持久化。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "application/application_result.h"
#include "system_support/settings/app_settings_store.h"

#include <memory>

// 组件说明：SettingsApplicationService 组件封装对应业务职责和生命周期边界。
class SettingsApplicationService
{
public:
    SettingsApplicationService(
        const std::shared_ptr<AppSettingsStore> &store,
        const AppSettings &loadedSettings);

    const AppSettings &current() const;
    OperationResult saveConfiguration(const AppSettings &candidate);
    OperationResult commitAppliedHardwareSettings(
        const AppSettings &appliedSettings);
    OperationResult saveTemplatePaths(DetectionMode mode,
                                      const QStringList &paths);
    OperationResult removeTemplatePaths(
        DetectionMode mode,
        const QStringList &paths);
    OperationResult saveTemplatePathsAndDirectory(
        DetectionMode mode,
        const QStringList &paths,
        const QString &directoryPath);
    OperationResult saveTissueThreshold(double value);
    QString applicationDataRoot() const;

private:
    static OperationResult storeFailure(
        const AppSettingsStoreError &error);

    std::shared_ptr<AppSettingsStore> m_store;
    AppSettings m_current;
};
