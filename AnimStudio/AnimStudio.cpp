#include "AnimStudio.h"
#include "AnimationData.h"
#include "ui_AnimStudio.h"
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

AnimStudio::AnimStudio(QWidget* parent)
    : QMainWindow(parent)
    , ui()
    , playbackTimer(new QTimer(this))
    , currentFrameIndex(0)
{
    ui.setupUi(this);

    // Timer for frame cycling
    playbackTimer->setInterval(100); // 10 FPS

    // Connect the widgets to the code
    connect(playbackTimer, &QTimer::timeout,this, &AnimStudio::updatePreviewFrame);
    connect(ui.playPauseButton, &QPushButton::clicked, this, &AnimStudio::on_playPauseButton_clicked);
    connect(ui.fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &AnimStudio::on_fpsSpinBox_valueChanged);
    connect(ui.timelineSlider, &QSlider::valueChanged, this, &AnimStudio::on_timelineSlider_valueChanged);

    // No scrollbars in the animation preview area
    ui.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

AnimStudio::~AnimStudio()
{
}

QVector<QImage> loadImageSequence(const QString& dirPath, const QStringList& filters) {
    QVector<QImage> images;
    QDir directory(dirPath);
    QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);

    for (const QString& file : files) {
        QImage img(dirPath + "/" + file);
        if (!img.isNull()) {
            images.append(img);
        }
    }
    return images;
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

    loadAnimation(AnimationType::Raw, dir);
}

void AnimStudio::updatePreviewFrame()
{
    if (frames.isEmpty())
        return;

    currentFrameIndex = (currentFrameIndex + 1) % frames.size();
    ui.previewLabel->setPixmap(QPixmap::fromImage(frames[currentFrameIndex]));
    ui.timelineSlider->blockSignals(true);
    ui.timelineSlider->setValue(currentFrameIndex);
    ui.timelineSlider->blockSignals(false);
}

void AnimStudio::on_playPauseButton_clicked() {
    if (playbackTimer->isActive()) {
        playbackTimer->stop();
        ui.playPauseButton->setText("Play");
    } else {
        playbackTimer->start();
        ui.playPauseButton->setText("Pause");
    }
}

void AnimStudio::on_fpsSpinBox_valueChanged(int value) {
    playbackTimer->setInterval(1000 / value); // FPS to ms
}

void AnimStudio::on_timelineSlider_valueChanged(int value) {
    if (value < frames.size()) {
        currentFrameIndex = value;
        ui.previewLabel->setPixmap(QPixmap::fromImage(frames[value]));
    }
}

void AnimStudio::resetControls(){
    // Stop playback
    playbackTimer->stop();
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
    frames.clear();
    currentFrameIndex = 0;
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

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    if (extension == "eff") {
        loadAnimation(AnimationType::Eff, filePath);
        return;
    } else if (extension == "ani") {
        loadAnimation(AnimationType::Ani, filePath);
    } else if (extension == "png") {
        loadAnimation(AnimationType::Apng, filePath);
    } else {
        QMessageBox::warning(this, "Unsupported Format", "The selected file format is not supported.");
        return;
    }
}

void AnimStudio::on_actionExit_triggered()
{
    close();
}

void AnimStudio::loadAnimation(AnimationType type, QString path)
{
    ui.previewLabel->hide();
    resetControls();
    createSpinner(ui.scrollArea);
    
    switch (type) {
        case AnimationType::Ani: {
            QFuture<std::optional<AnimationData>> future = QtConcurrent::run(AniImporter::importFromFile, path);
            auto* watcher = new QFutureWatcher<std::optional<AnimationData>>(this);

            connect(watcher, &QFutureWatcher<std::optional<AnimationData>>::finished, this, [=]() {
                std::optional<AnimationData> result = watcher->result();
                watcher->deleteLater();
                if (result.has_value()) {
                    onAnimationLoadFinished(result);
                } else {
                    onAnimationLoadFinished(std::nullopt, "Failed to import the ANI animation.");
                }
                });

            watcher->setFuture(future);
            return;
        }
        case AnimationType::Eff: {
            QFuture<std::optional<AnimationData>> future = QtConcurrent::run(EffImporter::importFromFile, path);
            auto* watcher = new QFutureWatcher<std::optional<AnimationData>>(this);

            connect(watcher, &QFutureWatcher<std::optional<AnimationData>>::finished, this, [=]() {
                std::optional<AnimationData> result = watcher->result();
                watcher->deleteLater();

                if (result.has_value()) {
                    onAnimationLoadFinished(result);
                } else {
                    onAnimationLoadFinished(std::nullopt, "Failed to import the selected animation.");
                }
                });

            watcher->setFuture(future);
            return;
        }
        case AnimationType::Apng: {
            QFuture<std::optional<AnimationData>> future = QtConcurrent::run(ApngImporter::importFromFile, path);
            auto* watcher = new QFutureWatcher<std::optional<AnimationData>>(this);

            connect(watcher, &QFutureWatcher<std::optional<AnimationData>>::finished, this, [=]() {
                std::optional<AnimationData> result = watcher->result();
                watcher->deleteLater();

                if (result.has_value()) {
                    onAnimationLoadFinished(result);
                } else {
                    onAnimationLoadFinished(std::nullopt, "Failed to import the selected animation.");
                }
                });

            watcher->setFuture(future);
            return;
        }
        case AnimationType::Raw: {
            QFuture<AnimationData> future = QtConcurrent::run(RawImporter::importBlocking, path);
            QFutureWatcher<AnimationData>* watcher = new QFutureWatcher<AnimationData>(this);

            connect(watcher, &QFutureWatcher<AnimationData>::finished, this, [=]() {
                deleteSpinner();
                AnimationData result = watcher->result();
                watcher->deleteLater();

                if (result.frameCount > 0) {
                    onAnimationLoadFinished(result);
                } else {
                    onAnimationLoadFinished(std::nullopt, "No valid frames found in the selected directory.");
                }
                });

            watcher->setFuture(future);
            return;
        }
        default: {
            QMessageBox::warning(this, "Unsupported Format", "The selected animation format is not supported.");
            break;
        }
    }
}

void AnimStudio::onAnimationLoadFinished(const std::optional<AnimationData>& data, const QString& errorMessage)
{
    deleteSpinner();
    ui.previewLabel->show();

    if (!data.has_value()) {
        QMessageBox::critical(this, "Import Failed", errorMessage.isEmpty() ? "Unknown error occurred." : errorMessage);
        return;
    }

    // Directly assign frames
    currentFrameIndex = 0;
    frames.clear();
    for (const auto& frame : data->frames) {
        frames.push_back(frame.image);
    }

    if (!frames.isEmpty()) {
        QImage first = frames.first();
        originalFrameSize = first.size();
        resizeEvent(nullptr);
        ui.previewLabel->setPixmap(QPixmap::fromImage(first));

        ui.timelineSlider->setMaximum(frames.size() - 1);
        ui.timelineSlider->setValue(0);
        ui.fpsSpinBox->setValue(data->fps);
        playbackTimer->start();
        ui.playPauseButton->setText("Pause");
    }
}

void AnimStudio::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    if (originalFrameSize.isEmpty()) return;

    // Get the current size of the scroll area viewport
    QSize available = ui.scrollArea->viewport()->size();

    // Calculate scaled size that fits while preserving aspect ratio
    QSize scaled = originalFrameSize;
    scaled.scale(available, Qt::KeepAspectRatio);

    // Apply new size to preview label
    ui.previewLabel->setFixedSize(scaled);
}