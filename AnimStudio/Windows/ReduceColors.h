#pragma once
#pragma once

#include <QDialog>
#include <QVector>
#include <QRgb>

namespace Ui { class ReduceColorsDialog; }

class ReduceColorsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReduceColorsDialog(QWidget* parent = nullptr);
    ~ReduceColorsDialog();
    QVector<QRgb> selectedPalette() const;
    int getQuality() const;
    int getMaxColors() const;

signals:
    /// Emitted when the user confirms reduction.
    void reduceConfirmed();

private:
    Ui::ReduceColorsDialog* ui;
};
