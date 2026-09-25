#pragma once

#include "startup/runtime_guard.h"

#include <QDialog>

#include <memory>

namespace Ui {
class ActivationDialog;
}

class ActivationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ActivationDialog(const RuntimeGuardResult &initialResult,
                              QWidget *parent = nullptr);
    ~ActivationDialog() override;

    RuntimeGuardResult activationResult() const;

private:
    void copyRequestCode();
    void activateLicense();

    std::unique_ptr<Ui::ActivationDialog> ui;
    RuntimeGuardResult m_activationResult;
};
