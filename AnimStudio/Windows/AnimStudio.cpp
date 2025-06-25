#include "AnimStudio.h"
#include "Animation/AnimationData.h"
#include "Formats/ImageFormats.h"
#include "ui_AnimStudio.h"
#include "Animation/Quantizer.h"
#include "Formats/Import/AniImporter.h"
#include "Formats/Import/ApngImporter.h"
#include "Formats/Import/EffImporter.h"
#include "Formats/Import/RawImporter.h"
#include "Widgets/spinnerwidget.h"
#include "Windows/ReduceColors.h"
#include "Windows/ExportAnimation.h"

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
#include <QInputDialog>
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
            updateFrameTimeDisplay();
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
        this, [&](const QString& title, const QString& msg) {
            deleteSpinner();
            QMessageBox::critical(this, title, msg);
        });

    connect(animCtrl, &AnimationController::playStateChanged,
        this, [&](bool playing) {
            ui.playPauseButton->setIcon(playing ? QIcon(":/AnimStudio/Resources/pause.png") : QIcon(":/AnimStudio/Resources/play.png"));
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
        this, [&](bool success) {
            ui.actionReduce_Colors->setEnabled(true);
            ui.actionCancel_Reduce_Colors->setEnabled(false);
            ui.statusBar->showMessage(success ? "Color reduction complete!" : "Color reduction incomplete.");
        });

    connect(animCtrl, &AnimationController::exportProgress,
        this, [&](float progress) {
            int pct = static_cast<int>(progress * 100.0f);
            ui.statusBar->showMessage(
                QString("Export Running: %1%").arg(pct)
            );
        });

    connect(animCtrl, &AnimationController::exportFinished,
        this, [&](bool success, AnimationType type, ImageFormat imageType, int frames) {
            ui.actionExport_All_Frames->setEnabled(true);
            ui.actionExport_Current_Frame->setEnabled(true);
            ui.actionExport_Animation->setEnabled(true);

            QString label = getTypeString(type).toUpper();

            if (type == AnimationType::Raw || type == AnimationType::Eff) {
                QString ext = extensionForFormat(imageType).mid(1).toUpper();  // remove dot, make uppercase
                label += QString(" (%1 frame%2, %3)")
                    .arg(frames)
                    .arg(frames == 1 ? "" : "s")
                    .arg(ext);
            }

            QString message = success
                ? QString("Export complete: %1.").arg(label)
                : QString("Export failed: %1.").arg(label);

            ui.statusBar->showMessage(message);
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

void AnimStudio::on_jumpStartButton_clicked() {
    animCtrl->seekFrame(0);
}

void AnimStudio::on_jumpLoopButton_clicked() {
    animCtrl->seekFrame(animCtrl->getLoopPoint());
}

void AnimStudio::on_previousFrameButton_clicked() {
    int current = ui.timelineSlider->value();
    if (current > 0) {
        animCtrl->seekFrame(current - 1);
    }
}

void AnimStudio::on_nextFrameButton_clicked() {
    int current = ui.timelineSlider->value();
    int max = ui.timelineSlider->maximum();
    if (current < max) {
        animCtrl->seekFrame(current + 1);
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

void AnimStudio::updateFrameTimeDisplay() {
    if (!animCtrl->isLoaded()) {
        // Nothing loaded -> clear out the views
        ui.currentFrameView->setText("0/0");
        ui.currentTimecodeView->setText("0:00.000");
        return;
    }

    // zero based
    int idx = ui.timelineSlider->value() - 1;
    int total = animCtrl->getFrameCount() - 1;
    int fps = animCtrl->getFPS();

    // — Frame X/Y —
    ui.currentFrameView->setText(
        QString("%1/%2").arg(idx + 1).arg(total)
    );

    // — Timecode mm:ss.mmm —
    double seconds = idx / double(fps);
    int wholeSec = int(seconds);
    int mins = wholeSec / 60;
    int secs = wholeSec % 60;
    int msecs = int((seconds - wholeSec) * 1000);

    ui.currentTimecodeView->setText(
        QString("%1:%2.%3")
        .arg(mins)
        .arg(secs, 2, 10, QChar('0'))
        .arg(msecs, 3, 10, QChar('0'))
    );
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
        "Animation Files (*.ani *.eff *.png *.apng);;All Files (*.*)"
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
    animCtrl->cancelQuantization();
    animCtrl->clear();
    resetInterface();
    updateMetadata(std::nullopt);
    ui.statusBar->clearMessage();
}

void AnimStudio::on_actionExport_Animation_triggered()
{
    ExportAnimationDialog dlg(animCtrl->getBaseName(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // Now prompt for output directory
    QString outDir = QFileDialog::getExistingDirectory(this, "Select Output Folder", QDir::homePath());
    if (outDir.isEmpty())
        return;

    // Dispatch
    animCtrl->exportAnimation(outDir, dlg.selectedAnimationType(), dlg.selectedImageFormat(), dlg.chosenBaseName());

}

void AnimStudio::on_actionExport_All_Frames_triggered()
{
    // Let the user pick a directory
    QString folder = QFileDialog::getExistingDirectory(
        this,
        "Select Folder to Export All Frames",
        QDir::homePath(),                          // starting location
        QFileDialog::ShowDirsOnly                   // only dirs, not files
    );
    if (folder.isEmpty())
        return;

    // Let the user pick the image format (png/jpg/etc)
    QStringList formats = availableExtensions();
    bool ok = false;
    QString ext = QInputDialog::getItem(
        this,
        "Choose Export Format",
        "Format:",
        formats,
        0,      // default to first
        false,  // editable?
        &ok
    );
    if (!ok || ext.isEmpty())
        return;

    // Dispatch directory + extension to your controller
    animCtrl->exportAllFrames(folder, ext);
}

void AnimStudio::on_actionExport_Current_Frame_triggered()
{
    // Build a list of “PNG Images (*.png)” etc. from availableExtensions()
    QStringList extList = availableExtensions();  // e.g. ["png","jpg","tga","pcx","dds"]
    QStringList filterEntries;
    for (const QString& e : extList) {
        filterEntries << QString("%1 Images (*.%2)").arg(e.toUpper()).arg(e);
    }
    filterEntries << "All Files (*.*)";

    // 2) Join them with the “;;” separator that Qt expects
    QString filter = filterEntries.join(";;");

    int frameCount = animCtrl->getFrameCount();      // total frames
    int maxIndex = frameCount - 1;                // last index
    int digits = QString::number(maxIndex).length();

    int frameIdx = ui.timelineSlider->value();
    QString defaultName = QString("%1_%2.png")
        .arg(animCtrl->getBaseName())
        .arg(frameIdx, digits, 10, QChar('0'));

    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Current Frame",
        defaultName,
        filter
    );
    if (filePath.isEmpty())
        return;

    // Determine format from the file extension
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();

    // dispatch to controller – make sure your controller method
    // signature matches: (const QString&, Format)
    animCtrl->exportCurrentFrame(filePath, ext);
}

void AnimStudio::on_actionReduce_Colors_triggered()
{
    ReduceColorsDialog dlg(animCtrl, this);
    // whenever user confirms, grab the palette and call quantize(palette)
    connect(&dlg, &ReduceColorsDialog::reduceConfirmed,
        this, [&dlg, this]() {
            // enable the cancel button
            ui.actionReduce_Colors->setEnabled(false);
            ui.actionCancel_Reduce_Colors->setEnabled(true);

            // show non quantized and disable the toggle button
            animCtrl->toggleShowQuantized(false);
            ui.actionShow_Reduced_Colors->setChecked(false);
            ui.actionShow_Reduced_Colors->setEnabled(false);

            // pull the user’s choice from the dialog
            animCtrl->quantize(dlg.selectedPalette(), dlg.getQuality(), dlg.getMaxColors());
        });
    dlg.exec();
}

void AnimStudio::on_actionShow_Reduced_Colors_toggled(bool checked)
{
    animCtrl->toggleShowQuantized(checked);
}

void AnimStudio::on_actionCancel_Reduce_Colors_triggered()
{
    animCtrl->cancelQuantization();
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

    // Playback controls
    ui.playPauseButton->setEnabled(hasData);
    ui.jumpStartButton->setEnabled(hasData);
    ui.jumpLoopButton->setEnabled(hasData);
    ui.previousFrameButton->setEnabled(hasData);
    ui.nextFrameButton->setEnabled(hasData);
    ui.timelineSlider->setEnabled(hasData);

    // Toolbar buttons
    ui.actionExport_Animation->setEnabled(hasData);
    ui.actionExport_All_Frames->setEnabled(hasData);
    ui.actionExport_Current_Frame->setEnabled(hasData);
    ui.actionReduce_Colors->setEnabled(hasData && !animCtrl->isQuantizeRunning());
    ui.actionShow_Reduced_Colors->setEnabled(hasData && anim.value().quantized);
    ui.actionCancel_Reduce_Colors->setEnabled(hasData && animCtrl->isQuantizeRunning());

    updateFrameTimeDisplay();

    if (hasData) {
        auto data = anim.value();

        ui.actionShow_Reduced_Colors->setChecked(data.quantized);

        ui.nameEdit->setText(data.baseName);

        QString typeLabelText;
        switch (data.animationType) {
            case AnimationType::Eff:
                typeLabelText = "Eff (" + extensionForFormat(data.type.value()) + ")";
                break;
            case AnimationType::Ani:
                typeLabelText = "Ani";
                break;
            case AnimationType::Apng:
                typeLabelText = "Apng";
                break;
            case AnimationType::Raw:
                typeLabelText = "Sequence (" + extensionForFormat(data.type.value()) + ")";
                break;
            default:
                typeLabelText = "";
        }

        ui.typeView->setText(typeLabelText);

        ui.fpsSpinBox->setValue(data.fps);

        float totalSec = data.totalLength;
        int totalMs = static_cast<int>(totalSec * 1000 + 0.5f);
        int mins = totalMs / 60000;
        int secs = (totalMs / 1000) % 60;
        int msecs = totalMs % 1000;
        QString dur = QString("%1:%2.%3")
            .arg(mins)
            .arg(secs, 2, 10, QChar('0'))   // width=2, pad with '0'
            .arg(msecs, 3, 10, QChar('0'));  // width=3, pad with '0'
        ui.lengthView->setText(dur);

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