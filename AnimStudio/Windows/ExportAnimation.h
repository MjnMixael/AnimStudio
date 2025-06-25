#pragma once

#include <QDialog>
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"

namespace Ui { class ExportAnimationDialog; }

class ExportAnimationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportAnimationDialog(const QString& defaultBaseName, QWidget* parent = nullptr);
    ~ExportAnimationDialog();

    AnimationType selectedAnimationType() const;
    ImageFormat selectedImageFormat() const;
    QString chosenBaseName() const;

private slots:
    void onFormatChanged(int index);

private:
    Ui::ExportAnimationDialog* ui;
};
