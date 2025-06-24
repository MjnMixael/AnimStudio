#include "ReduceColors.h"
#include "ui_ReduceColors.h"
#include "Animation/BuiltInPalettes.h"

#include <iterator>

#include <QPushButton>
#include <QDialogButtonBox>

ReduceColorsDialog::ReduceColorsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ReduceColorsDialog)
{
    ui->setupUi(this);

    // 1) Populate palette dropdown
    ui->paletteComboBox->addItem(tr("Automatic"));
    ui->paletteComboBox->addItem(tr("Shield"));
    ui->paletteComboBox->addItem(tr("HUD"));
    ui->paletteComboBox->addItem(tr("FS1 Ship"));
    ui->paletteComboBox->addItem(tr("FS1 Weapon"));
    ui->paletteComboBox->addItem(tr("FS2 Ship"));
    ui->paletteComboBox->addItem(tr("FS2 Weapon"));

    // Grab the “Apply” button (which we’ll treat as “Reduce”)
    QPushButton* reduceBtn = ui->buttonBox->button(QDialogButtonBox::Apply);
    reduceBtn->setText(tr("Reduce"));

    // When they click "Reduce", emit our signal and close
    connect(reduceBtn, &QPushButton::clicked, this, [this]() {
        emit reduceConfirmed();
        accept();
        });

    // Disable the max-colors spin box whenever a built - in palette is selected
    // and re-enable it when "Automatic" (index 0) is chosen.
    ui->maxColorsSpinBox->setEnabled(ui->paletteComboBox->currentIndex() == 0);
    connect(ui->paletteComboBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int idx) {
            ui->maxColorsSpinBox->setEnabled(idx == 0);
        });
}

// Returns one of our constexpr arrays as a QVector
QVector<QRgb> ReduceColorsDialog::selectedPalette() const {
    switch (ui->paletteComboBox->currentIndex()) {
        case 0: return {}; // No palette selected
        case 1: return QVector<QRgb>(std::begin(ShieldPalette), std::end(ShieldPalette));
        case 2: return QVector<QRgb>(std::begin(HudPalette), std::end(HudPalette));
        case 3: return QVector<QRgb>(std::begin(Fs1SelectShipPalette), std::end(Fs1SelectShipPalette));
        case 4: return QVector<QRgb>(std::begin(Fs1SelectWepPalette), std::end(Fs1SelectWepPalette));
        case 5: return QVector<QRgb>(std::begin(Fs2SelectShipPalette), std::end(Fs2SelectShipPalette));
        case 6: return QVector<QRgb>(std::begin(Fs2SelectWepPalette), std::end(Fs2SelectWepPalette));
        default: return {};
    }
}

int ReduceColorsDialog::getQuality() const {
    return ui->qualitySpinBox->value();
}
int ReduceColorsDialog::getMaxColors() const {
    if (ui->paletteComboBox->currentIndex() > 0) {
        return selectedPalette().size();
    }
    return ui->maxColorsSpinBox->value();
}

ReduceColorsDialog::~ReduceColorsDialog()
{
    delete ui;
}
