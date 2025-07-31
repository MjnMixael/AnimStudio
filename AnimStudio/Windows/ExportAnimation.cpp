#include "ExportAnimation.h"
#include "ui_ExportAnimation.h"
#include "Formats/ImageFormats.h"

ExportAnimationDialog::ExportAnimationDialog(const QString& defaultBaseName, QWidget* parent)
    : QDialog(parent), ui(new Ui::ExportAnimationDialog)
{
    ui->setupUi(this);

    // Populate export format choices
    const auto exportableTypes = getExportableTypes();
    for (const auto& type : exportableTypes) {
        ui->typeComboBox->addItem(getTypeString(type), static_cast<int>(type));
    }

    // Populate image format choices (used only for EFF)
    ui->formatComboBox->addItems(availableExtensions());
    ui->formatComboBox->setEnabled(false);  // Hidden unless EFF is selected

    // Populate compression format choices (used only for EFF/DDS)
    ui->compressionComboBox->addItems(availableCompressionFormats());
    ui->compressionComboBox->setEnabled(false);  // Hidden unless EFF/DDS is selected

    // Set base name
    ui->nameLineEdit->setText(defaultBaseName);

    // Connect format change to toggle visibility of image format selector
    connect(ui->typeComboBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ExportAnimationDialog::onTypeChanged);

    // Connect format change to toggle visibility of compression format selector
    connect(ui->formatComboBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ExportAnimationDialog::onFormatChanged);
}

ExportAnimationDialog::~ExportAnimationDialog()
{
    delete ui;
}

void ExportAnimationDialog::onTypeChanged(int index)
{
    Q_UNUSED(index);
    const QString type = ui->typeComboBox->currentText().toLower();
    const QString format = ui->formatComboBox->currentText().toLower();
    ui->formatComboBox->setEnabled(type == "eff");
    ui->compressionComboBox->setEnabled(type == "eff" && format == "dds");

    if (type == "apng") {
        ui->warningLabel->setText("Be aware that APNGs do not support special loop keyframes. If set, the keyframe will be ignored.");
        ui->warningLabel->setVisible(true);
    } else {
        ui->warningLabel->clear();
        ui->warningLabel->setVisible(false);
    }
}

void ExportAnimationDialog::onFormatChanged(int index)
{
    Q_UNUSED(index);
    const QString type = ui->typeComboBox->currentText().toLower();
    const QString format = ui->formatComboBox->currentText().toLower();
    ui->compressionComboBox->setEnabled(type == "eff" && format == "dds");
}

AnimationType ExportAnimationDialog::selectedAnimationType() const
{
    const QString fmt = ui->typeComboBox->currentText().toLower();
    if (fmt == "ani") return AnimationType::Ani;
    if (fmt == "eff") return AnimationType::Eff;
    return AnimationType::Apng;  // default fallback
}

ImageFormat ExportAnimationDialog::selectedImageFormat() const
{
    const QString ext = ui->formatComboBox->currentText().toLower();
    return ext.isEmpty() ? ImageFormat::Png : formatFromExtension(ext);
}

CompressionFormat ExportAnimationDialog::selectedCompressionFormat() const
{
    const QString selectedText = ui->compressionComboBox->currentText();
    return getCompressionFormatFromDescription(selectedText);
}

QString ExportAnimationDialog::chosenBaseName() const
{
    return ui->nameLineEdit->text().trimmed();
}
