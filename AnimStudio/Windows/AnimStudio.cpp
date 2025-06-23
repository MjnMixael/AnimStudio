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
#include <QFile>
#ifdef Q_OS_WIN
#  include <windows.h>
#  include <psapi.h>
#elif defined(Q_OS_LINUX)
#  include <unistd.h>
#endif

AnimStudio::AnimStudio(QWidget* parent)
    : QMainWindow(parent)
    , ui()
{
    ui.setupUi(this);

    // 1) construct your controller
    animCtrl = new AnimationController(this);

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

            // only update the slider's maximum if it actually changed
            int newMax = d.frameCount - 1;
            if (ui.timelineSlider->maximum() != newMax) {
                ui.timelineSlider->setMaximum(newMax);
                ui.timelineSlider->setValue(0);
            }
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

    connect(animCtrl, &AnimationController::animationLoaded,
        this, [&]() {
            adjustPreviewSize();
        });

    connect(animCtrl, &AnimationController::quantizationProgress,
        this, [&](int pct) {
            ui.statusBar->showMessage(
                QString("Reducing colors: %1%").arg(pct)
            );
        });

    connect(animCtrl, &AnimationController::quantizationFinished,
        this, [&]() {
            ui.statusBar->showMessage("Color reduction complete!");
        });

    // Create a permanent label on the right side of the statusbar
    m_rightStatusLabel = new QLabel(this);
    m_rightStatusLabel->setText("Ready");
    ui.statusBar->addPermanentWidget(m_rightStatusLabel);
    m_memTimer = new QTimer(this);
    m_memTimer->setInterval(5000);
    connect(m_memTimer, &QTimer::timeout,
        this, &AnimStudio::updateMemoryUsage);
    m_memTimer->start();
    updateMemoryUsage();

    updateMetadata(std::nullopt);

    ui.statusBar->showMessage("Ready!");
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

void AnimStudio::on_loopFrameSpinBox_valueChanged(int value) {
    animCtrl->setLoopPoint(value);
}

void AnimStudio::on_nameEdit_editingFinished() {
    animCtrl->setBaseName(ui.nameEdit->text());
}

void AnimStudio::on_keyframeAllCheckBox_toggled(bool all) {
    animCtrl->setAllKeyframesActive(all);
}

void AnimStudio::on_timelineSlider_valueChanged(int value) {
    // Jump the controller to the requested frame
    animCtrl->seekFrame(value);
}

void AnimStudio::resetInterface(){
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

void AnimStudio::on_actionExit_triggered()
{
    close();
}

void AnimStudio::on_actionOpenImageSequence_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Image Sequence Directory");
    if (dir.isEmpty())
        return;

    // hide the old preview and show spinner while loading
    ui.previewLabel->hide();
    resetInterface();
    createSpinner(ui.playerScrollArea);

    animCtrl->loadRawSequence(dir);
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
    resetInterface();
    createSpinner(ui.playerScrollArea);

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

void AnimStudio::on_actionClose_Image_Sequence_triggered()
{
    resetInterface();
    animCtrl->clear();
    updateMetadata(std::nullopt);
}

void AnimStudio::on_actionExport_Animation_triggered()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Animation",
        QString(),
        "Animation Files (*.ani *.eff *.png);;All Files (*.*)"
    );
    if (filePath.isEmpty())
        return;
    // dispatch to controller
    //animCtrl->exportAnimation(filePath);
}

void AnimStudio::on_actionExport_All_Frames_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Export All Frames Directory");
    if (dir.isEmpty())
        return;
    // dispatch to controller
    //animCtrl->exportAllFrames(dir);
}

void AnimStudio::on_actionExport_Current_Frame_triggered()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Current Frame",
        QString(),
        "Image Files (*.png *.jpg *.bmp);;All Files (*.*)"
    );
    if (filePath.isEmpty())
        return;
    // dispatch to controller
    //animCtrl->exportCurrentFrame(filePath);
}

void AnimStudio::on_actionReduce_Colors_triggered()
{
    // show spinner while quantizing
    //createSpinner(ui.playerScrollArea);
    animCtrl->quantize();
}

void AnimStudio::on_actionShow_Reduced_Colors_toggled(bool checked)
{
    animCtrl->toggleShowQuantized(checked);
}

void AnimStudio::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    adjustPreviewSize();
}

void AnimStudio::adjustPreviewSize() {
    QSize resolution = animCtrl->getResolution();
    if (resolution.isEmpty()) return;

    QSize available = ui.playerScrollArea->viewport()->size();
    QSize scaled = resolution;
    scaled.scale(available, Qt::KeepAspectRatio);
    ui.previewLabel->setFixedSize(scaled);
}

void AnimStudio::updateMetadata(std::optional<AnimationData> anim) {
    const bool hasData = animCtrl->isLoaded() && anim.has_value();
    ui.nameEdit->setEnabled(hasData);
    ui.fpsSpinBox->setEnabled(hasData);
    ui.loopFrameSpinBox->setEnabled(!animCtrl->getAllKeyframesActive());
    ui.keyframeAllCheckBox->setEnabled(hasData);
    ui.playPauseButton->setEnabled(hasData);
    ui.timelineSlider->setEnabled(hasData);

    // Toolbar buttons
    ui.actionExport_Animation->setEnabled(hasData);
    ui.actionExport_All_Frames->setEnabled(hasData);
    ui.actionExport_Current_Frame->setEnabled(hasData);
    ui.actionReduce_Colors->setEnabled(hasData);
    ui.actionShow_Reduced_Colors->setEnabled(hasData && anim.value().quantized);

    if (hasData) {
        auto data = anim.value();

        ui.actionShow_Reduced_Colors->setChecked(data.quantized);

        ui.nameEdit->setText(data.baseName);

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

        ui.typeView->setText(typeLabelText);

        ui.fpsSpinBox->setValue(data.fps);

        ui.lengthView->setText(QString("%1:%2").arg(data.totalLength / 60).arg(data.totalLength %60, 2, 10, QChar('0')));

        ui.framesView->setText(QString::number(data.frameCount));
        ui.resolutionView->setText(QString("%1 x %2")
            .arg(data.originalSize.width())
            .arg(data.originalSize.height()));

        ui.loopFrameSpinBox->setRange(0, data.frameCount - 1);
        ui.loopFrameSpinBox->setValue(data.loopPoint);
        int keyframeCount = data.keyframeIndices.size();
        ui.keyframeAllCheckBox->setChecked((int)data.keyframeIndices.size() == data.frameCount);
    } else {
        ui.nameEdit->clear();
        ui.typeView->clear();
        ui.fpsSpinBox->setValue(0);
        ui.framesView->clear();
        ui.lengthView->clear();
        ui.resolutionView->clear();
        ui.loopFrameSpinBox->setValue(0);
        ui.keyframeAllCheckBox->setChecked(false);
    }
}

qint64 AnimStudio::currentMemoryBytes() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#elif defined(Q_OS_LINUX)
    QFile f("/proc/self/statm");
    if (!f.open(QIODevice::ReadOnly)) return 0;
    QByteArray data = f.readLine();
    f.close();
    // statm: size resident shared text lib data dt
    qint64 resident = data.split(' ')[1].toLongLong();
    return resident * sysconf(_SC_PAGESIZE);
#else
    return 0; // stub for other platforms
#endif
}

void AnimStudio::updateMemoryUsage() {
    qint64 bytes = currentMemoryBytes();
    double mb = bytes / 1024.0 / 1024.0;
    m_rightStatusLabel->setText(
        QStringLiteral("RAM: %1 MB").arg(mb, 0, 'f', 1)
    );
}