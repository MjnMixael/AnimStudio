#include "AnimStudio.h"
#include "Animation/AnimationData.h"
#include "ui_AnimStudio.h"
#include "Animation/Quantizer.h"
#include "Formats/Import/AniImporter.h"
#include "Formats/Import/ApngImporter.h"
#include "Formats/Import/EffImporter.h"
#include "Formats/Import/RawImporter.h"
#include "Widgets/spinnerwidget.h"

#include <QFileDialog>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QMessageBox>
#include <optional>
#include <QFormLayout>

AnimStudio::AnimStudio(QWidget* parent)
    : QMainWindow(parent)
    , ui()
{
    ui.setupUi(this);

    // 1) construct your controller
    animCtrl = new AnimationController(this);

    setupMetadataDock();

    // 2) Controller -> UI
    connect(animCtrl, &AnimationController::frameReady,
        this, [&](const QImage& img, int idx) {
            ui.previewLabel->setPixmap(QPixmap::fromImage(img));
            ui.timelineSlider->blockSignals(true);
            ui.timelineSlider->setValue(idx);
            ui.timelineSlider->blockSignals(false);
        });

    connect(animCtrl, &AnimationController::metadataChanged,
        this, [&](const AnimationData& d) {
            // stop spinner & show preview:
            deleteSpinner();
            ui.previewLabel->show();

            // update UI for new data:
            updateMetadata(d);
            ui.timelineSlider->setMaximum(d.frameCount - 1);
            ui.timelineSlider->setValue(0);
        });

    connect(animCtrl, &AnimationController::errorOccurred,
        this, [&](const QString& msg) {
            deleteSpinner();
            QMessageBox::critical(this, "Import Failed", msg);
        });

    connect(animCtrl, &AnimationController::playStateChanged,
        this, [&](bool playing) {
            ui.playPauseButton->setText(playing ? "Pause" : "Play");
        });

    // No scrollbars in the animation preview area
    ui.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

AnimStudio::~AnimStudio()
{
}

void AnimStudio::createSpinner(QWidget* parent) {
    if (spinner) {
        deleteSpinner();
    }
    spinner = new SpinnerWidget(parent);
    spinner->move((parent->width() - spinner->width()) / 2,
        (parent->height() - spinner->height()) / 2);
    spinner->show();
    QApplication::processEvents();
}

void AnimStudio::deleteSpinner() {
    if (spinner) {
        spinner->hide();
        spinner->deleteLater();
        spinner = nullptr;
    }
}

void AnimStudio::on_actionOpenImageSequence_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Image Sequence Directory");
    if (dir.isEmpty())
        return;

    // hide the old preview and show spinner while loading
    ui.previewLabel->hide();
    resetControls();
    createSpinner(ui.scrollArea);

    animCtrl->loadRawSequence(dir);
}

void AnimStudio::on_playPauseButton_clicked() {
    // Toggle based on the button’s current label
    if (animCtrl->isPlaying()) {
        animCtrl->pause();
    } else {
        animCtrl->play();
    }
}

void AnimStudio::on_fpsSpinBox_valueChanged(int value) {
    animCtrl->setFps(value); // FPS to ms
}

void AnimStudio::on_timelineSlider_valueChanged(int value) {
    // Jump the controller to the requested frame
    animCtrl->seekFrame(value);
}

void AnimStudio::resetControls(){
    // Stop playback
    animCtrl->pause();
    ui.playPauseButton->setText("Play");

    // Reset timeline slider
    ui.timelineSlider->setValue(0);
    ui.timelineSlider->setMaximum(0);

    // Reset animation preview area
    resizeEvent(nullptr);

    // Reset preview label
    ui.previewLabel->clear();
    ui.previewLabel->setText("Open an image sequence...");
    ui.previewLabel->setMinimumSize(0, 0);
    ui.previewLabel->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    //ui.previewLabel->setFixedSize(QSize());
    ui.previewLabel->adjustSize();
    ui.previewLabel->setAlignment(Qt::AlignCenter);
}

void AnimStudio::on_actionClose_Image_Sequence_triggered()
{
    resetControls();
    animCtrl->clear();
    updateMetadata(std::nullopt);
}

void AnimStudio::on_actionExit_triggered()
{
    close();
}

void AnimStudio::on_actionImport_Animation_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Import Animation",
        QString(),
        "Animation Files (*.ani *.eff *.png);;All Files (*.*)"
    );
    if (filePath.isEmpty())
        return;

    // show spinner while loading
    ui.previewLabel->hide();
    resetControls();
    createSpinner(ui.scrollArea);

    // dispatch to controller
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "ani") {
        animCtrl->loadAniFile(filePath);
    } else if (ext == "eff") {
        animCtrl->loadEffFile(filePath);
    } else if (ext == "png") {
        animCtrl->loadApngFile(filePath);
    } else {
        deleteSpinner();
        QMessageBox::warning(this, "Unsupported Format",
            "The selected file format is not supported.");
    }
}

void AnimStudio::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    QSize resolution = animCtrl->getResolution();

    if (resolution.isEmpty()) return;

    // Get the current size of the scroll area viewport
    QSize available = ui.scrollArea->viewport()->size();

    // Calculate scaled size that fits while preserving aspect ratio
    QSize scaled = resolution;
    scaled.scale(available, Qt::KeepAspectRatio);

    // Apply new size to preview label
    ui.previewLabel->setFixedSize(scaled);
}

void AnimStudio::setupMetadataDock() {
    metadataDock = new QDockWidget(tr("Metadata"), this);
    QWidget* container = new QWidget(metadataDock);
    QFormLayout* form = new QFormLayout(container);

    nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("");
    form->addRow(tr("Name:"), nameEdit);
    nameEdit->setEnabled(false);

    typeLabel = new QLabel; form->addRow(tr("Type:"), typeLabel);
    
    fpsSpin = new QSpinBox;
    fpsSpin->setRange(1, 240);
    fpsSpin->setEnabled(false);
    form->addRow(tr("FPS:"), fpsSpin);

    framesLabel = new QLabel; form->addRow(tr("Frames:"), framesLabel);
    resolutionLabel = new QLabel; form->addRow(tr("Resolution:"), resolutionLabel);
    
    // Loop-back spin:
    loopPointSpin = new QSpinBox;
    loopPointSpin->setRange(0, 0);            // will be set when data loads
    loopPointSpin->setEnabled(false);
    form->addRow(tr("Loop-back frame:"), loopPointSpin);
    connect(loopPointSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &AnimStudio::onLoopPointChanged);

    // “All frames are keyframes”:
    allKeyframesCheck = new QCheckBox(tr("All frames keyframes"));
    allKeyframesCheck->setEnabled(false);
    form->addRow(QString(), allKeyframesCheck);
    connect(allKeyframesCheck, &QCheckBox::toggled,
        this, &AnimStudio::onAllKeyframesToggled);

    // Quantization buttons:
    quantizeBtn = new QPushButton(tr("Quantize"));
    undoQuantBtn = new QPushButton(tr("Undo"));
    QHBoxLayout* hlay = new QHBoxLayout;
    hlay->addWidget(quantizeBtn);
    hlay->addWidget(undoQuantBtn);
    form->addRow(QString(), hlay);

    connect(nameEdit, &QLineEdit::editingFinished, this, &AnimStudio::onNameEditFinished);
    connect(fpsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &AnimStudio::onMetadataFpsChanged);

    // Connect quantization buttons
    connect(quantizeBtn, &QPushButton::clicked, this, &AnimStudio::onQuantizeClicked);
    connect(undoQuantBtn, &QPushButton::clicked, this, &AnimStudio::onUndoQuantize);

    container->setLayout(form);
    metadataDock->setWidget(container);
    addDockWidget(Qt::RightDockWidgetArea, metadataDock);
    metadataDock->setTitleBarWidget(new QWidget(metadataDock));
    metadataDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    metadataDock->show();
}

void AnimStudio::updateMetadata(std::optional<AnimationData> anim) {
    const bool hasData = anim.has_value();
    nameEdit->setEnabled(hasData);
    fpsSpin->setEnabled(hasData);
    loopPointSpin->setEnabled(!animCtrl->getAllKeyframes());
    allKeyframesCheck->setEnabled(hasData);
    quantizeBtn->setEnabled(hasData);
    undoQuantBtn->setEnabled(hasData && animCtrl->canUndoQuantize());

    if (hasData) {
        auto data = anim.value();

        nameEdit->setText(data.baseName);

        QString typeLabelText;
        switch (data.animationType) {
            case AnimationType::Eff:
                typeLabelText = "Eff: " + data.type;
                break;
            case AnimationType::Ani:
                typeLabelText = "Ani";
                break;
            case AnimationType::Apng:
                typeLabelText = "Apng";
                break;
            case AnimationType::Raw:
                typeLabelText = "Sequence: " + data.type;
                break;
            default:
                typeLabelText = "";
        }

        typeLabel->setText(typeLabelText);

        fpsSpin->setValue(data.fps);

        framesLabel->setText(QString::number(data.frameCount));
        resolutionLabel->setText(QString("%1 x %2")
            .arg(data.originalSize.width())
            .arg(data.originalSize.height()));
        // comma-list of keyframes
        QStringList keys;
        for (int i : data.keyframeIndices) keys << QString::number(i);
        loopPointSpin->setRange(0, data.frameCount - 1);

        bool all = (int)data.keyframeIndices.size() == data.frameCount;
        allKeyframesCheck->setChecked(all);

        if (all) {
            // when “All” is on, spin tells nothing
        } else if (data.keyframeIndices.empty()) {
            loopPointSpin->setValue(0);
        } else {
            // pick the first (there should only ever be one)
            loopPointSpin->setValue(data.keyframeIndices[0]);
        }
    } else {
        nameEdit->clear();
        typeLabel->clear();
        fpsSpin->setValue(0);
        framesLabel->clear();
        resolutionLabel->clear();
        loopPointSpin->setValue(0);
        allKeyframesCheck->setChecked(false);
    }
}

void AnimStudio::onNameEditFinished() {
    animCtrl->setBaseName(nameEdit->text());
}

void AnimStudio::onMetadataFpsChanged(int fps)
{
    animCtrl->setFps(fps);
}

void AnimStudio::onLoopPointChanged(int frame) {
    if (allKeyframesCheck->isChecked())
        return;

    // single keyframe mode:
    animCtrl->setLoopPoint(frame);
}

void AnimStudio::onAllKeyframesToggled(bool all) {
    animCtrl->setAllKeyframes(all);
}

void AnimStudio::onQuantizeClicked() {
    animCtrl->quantize();

    // 4) update button states
    undoQuantBtn->setEnabled(true);
    quantizeBtn->setEnabled(false);
}

void AnimStudio::onUndoQuantize() {
    if (!animCtrl->canUndoQuantize()) return;

    // restore
    animCtrl->undoQuantize();

    // reset button states
    undoQuantBtn->setEnabled(false);
    quantizeBtn->setEnabled(true);
}