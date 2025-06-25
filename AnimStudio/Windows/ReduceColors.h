#pragma once

#include "Animation/AnimationController.h"

#include <QDialog>
#include <QVector>
#include <QRgb>

namespace Ui { class ReduceColorsDialog; }

class ReduceColorsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReduceColorsDialog(const AnimationController* animCtrl, QWidget* parent = nullptr);
    ~ReduceColorsDialog();
    QVector<QRgb> selectedPalette() const;
    int getQuality() const;
    int getMaxColors() const;

signals:
    /// Emitted when the user confirms reduction.
    void reduceConfirmed();

private slots:
    void onImportPalette();
    void onPreviewPalette();

private:
    Ui::ReduceColorsDialog* ui;
    const AnimationController* m_animCtrl = nullptr;

    void addPalettesToDropdown();
};
