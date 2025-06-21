#include "AnimStudio.h"
#include "ui_AnimStudio.h"
#include "Widgets/spinnerwidget.h"

#include <QFileDialog>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

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

void AnimStudio::on_actionOpenImageSequence_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Image Sequence Directory");
    if (dir.isEmpty())
        return;

    ui.previewLabel->hide();
    resetControls();

    // Create and show spinner
    SpinnerWidget* spinner = new SpinnerWidget(ui.scrollArea);
    spinner->move((ui.scrollArea->width() - spinner->width()) / 2,
        (ui.scrollArea->height() - spinner->height()) / 2);
    spinner->show();
    QApplication::processEvents();

    QStringList filters = { "*.png", "*.bmp", "*.jpg", "*.tga" };
    QDir directory(dir);
    QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);

    QFuture<QVector<QImage>> future = QtConcurrent::run(loadImageSequence, dir, filters);
    QFutureWatcher<QVector<QImage>>* watcher = new QFutureWatcher<QVector<QImage>>(this);

    connect(watcher, &QFutureWatcher<QVector<QImage>>::finished, this, [=]() {
        frames = watcher->result();
        currentFrameIndex = 0;

        spinner->hide();
        spinner->deleteLater();
        watcher->deleteLater();
        ui.previewLabel->show();

        if (!frames.isEmpty()) {
            QImage first = frames.first();
            originalFrameSize = first.size();
            resizeEvent(nullptr);
            ui.previewLabel->setPixmap(QPixmap::fromImage(first));

            ui.timelineSlider->setMaximum(frames.size() - 1);
            ui.timelineSlider->setValue(0);
            playbackTimer->start();
            ui.playPauseButton->setText("Pause");
        }
    });

    watcher->setFuture(future);
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

void AnimStudio::on_actionExit_triggered()
{
    close();
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