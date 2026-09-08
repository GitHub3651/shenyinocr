// 文件作用：负责 AppSettings Schema 8 的严格读取、校验和原子保存。
#pragma once

#include "system_support/settings/app_settings.h"

#include <QString>

enum class AppSettingsLoadStatus
{
    Loaded,
    FirstRun,
    ResetRequired
};

struct AppSettingsStoreError
{
    QString code;
    QString userMessage;
    QString diagnostic;

    bool isEmpty() const { return code.isEmpty(); }
};

class AppSettingsStore
{
public:
    explicit AppSettingsStore(const QString &applicationDataRoot);

    QString applicationDataRoot() const;
    QString settingsFilePath() const;

    bool load(AppSettings *settings,
              AppSettingsLoadStatus *status,
              AppSettingsStoreError *error = nullptr) const;
    bool save(const AppSettings &settings,
              AppSettingsStoreError *error = nullptr) const;

private:
    QString m_applicationDataRoot;
};
