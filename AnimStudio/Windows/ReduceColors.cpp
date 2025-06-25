#include "ReduceColors.h"
#include "ui_ReduceColors.h"
#include "Animation/BuiltInPalettes.h"
#include "Animation/Palette.h"

#include <iterator>

#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>

ReduceColorsDialog::ReduceColorsDialog(const AnimationController* animCtrl, QWidget* parent)
    : QDialog(parent), ui(new Ui::ReduceColorsDialog), m_animCtrl(animCtrl)
{
    ui->setupUi(this);

    // Populate palette dropdown
    addPalettesToDropdown();

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
            ui->previewPaletteButton->setEnabled(idx != 0);
            ui->transparencyCheckBox->setEnabled(idx != 0);
            if (idx == 0) {
                ui->transparencyCheckBox->setChecked(true);
            }
        });

    connect(ui->importPaletteButton, &QPushButton::clicked, this, &ReduceColorsDialog::onImportPalette);
    connect(ui->previewPaletteButton, &QPushButton::clicked, this, &ReduceColorsDialog::onPreviewPalette);
}

void ReduceColorsDialog::addPalettesToDropdown()
{
    ui->paletteComboBox->clear();

    // Index 0: Automatic
    ui->paletteComboBox->addItem(tr("Automatic"));

    // Index 1: Current Palette (if ANI and palette is valid)
    const QVector<QRgb>* currentPalette =
        (m_animCtrl && m_animCtrl->isQuantized())
        ? m_animCtrl->getCurrentPalette()
        : nullptr;

    if (currentPalette && !currentPalette->isEmpty()) {
        ui->paletteComboBox->addItem("Current Palette");
    }

    int insertOffset = 1 + ((m_animCtrl && m_animCtrl->getType() == AnimationType::Ani && currentPalette) ? 1 : 0);

    // Built-in palettes
    const auto& builtins = getBuiltInPalettes();
    for (const auto& bp : builtins)
        ui->paletteComboBox->addItem(bp.name);

    // User palettes
    for (const auto& up : Palette::userPalettes)
        ui->paletteComboBox->addItem("User: " + up.name);

    // Re-enable max color box if Automatic selected
    ui->maxColorsSpinBox->setEnabled(ui->paletteComboBox->currentIndex() == 0);
    ui->previewPaletteButton->setEnabled(ui->paletteComboBox->currentIndex() != 0);
}


void ReduceColorsDialog::onImportPalette() {
    QString file = QFileDialog::getOpenFileName(this, tr("Import Palette"), QString(), tr("Palette Files (*.pal *.gpl *.act *.ase *.txt)"));
    if (file.isEmpty())
        return;

    QVector<QRgb> loaded;
    if (!Palette::loadPaletteAuto(file, loaded)) {
        QMessageBox::warning(this, tr("Import Failed"), tr("Could not load the selected palette file."));
        return;
    }

    QString name = QFileInfo(file).baseName();
    Palette::addUserPalette(name, loaded);

    // Refresh dropdown
    addPalettesToDropdown();

    bool hasCurrent = (m_animCtrl &&
        m_animCtrl->isQuantized() &&
        m_animCtrl->getCurrentPalette() &&
        !m_animCtrl->getCurrentPalette()->isEmpty());

    int insertOffset = 1 + (hasCurrent ? 1 : 0);

    // Select the newly added one
    int index = insertOffset + getNumBuiltInPalettes() + (Palette::userPalettes.size() - 1);
    ui->paletteComboBox->setCurrentIndex(index); // index of last added

    ui->maxColorsSpinBox->setEnabled(false);  // Disable since it's now fixed
}

void ReduceColorsDialog::onPreviewPalette() {
    QVector<QRgb> pal = selectedPalette();
    if (pal.isEmpty()) {
        QMessageBox::information(this, tr("No Palette"), tr("No palette is currently selected or imported."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Palette Preview");
    dlg.setMinimumSize(300, 300);
    QGridLayout* grid = new QGridLayout(&dlg);
    grid->setSpacing(2);
    grid->setContentsMargins(10, 10, 10, 10);

    const int columns = 16;
    const int cellSize = 20;

    for (int i = 0; i < pal.size(); ++i) {
        auto* lbl = new QLabel;
        lbl->setFixedSize(cellSize, cellSize);
        QPixmap pix(cellSize, cellSize);
        pix.fill(QColor::fromRgba(pal[i]));
        lbl->setPixmap(pix);
        grid->addWidget(lbl, i / columns, i % columns);
    }

    dlg.exec();
}

// Returns one of our constexpr arrays as a QVector
QVector<QRgb> ReduceColorsDialog::selectedPalette() const {
    int idx = ui->paletteComboBox->currentIndex();
    if (idx == 0) return {}; // Automatic

    // Check if "Current Palette" is present at index 1
    bool hasCurrent = (m_animCtrl &&
        m_animCtrl->isQuantized() &&
        m_animCtrl->getCurrentPalette() &&
        !m_animCtrl->getCurrentPalette()->isEmpty());

    if (hasCurrent && idx == 1) {
        return *m_animCtrl->getCurrentPalette(); // Dereference raw pointer
    }

    int offset = hasCurrent ? 2 : 1; // Adjust for optional "Current Palette"

    int builtinIndex = idx - offset;
    const auto& builtins = getBuiltInPalettes();
    if (builtinIndex >= 0 && builtinIndex < builtins.size()) {
        return builtins[builtinIndex].colors;
    }

    int userIndex = builtinIndex - builtins.size();
    if (userIndex >= 0 && userIndex < Palette::userPalettes.size()) {
        return Palette::userPalettes[userIndex].colors;
    }

    return {};
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

bool ReduceColorsDialog::useTransparencyOverride() const {
    return ui->transparencyCheckBox->isChecked();
}

ReduceColorsDialog::~ReduceColorsDialog()
{
    delete ui;
}
