#pragma once

#include <QtWidgets/QMainWindow>
#include <qmainwindow.h>
#include <qvector>
#include <qimage>
#include <qtimer>
#include <QtConcurrent>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <optional>
#include "ui_AnimStudio.h"
#include "Animation/AnimationData.h"
#include "Widgets/SpinnerWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AnimStudioClass; }
QT_END_NAMESPACE

class AnimStudio : public QMainWindow {
    Q_OBJECT

public:
    AnimStudio(QWidget* parent = nullptr);
    ~AnimStudio();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void on_actionOpenImageSequence_triggered();
    void on_actionClose_Image_Sequence_triggered();
    void on_actionImport_Animation_triggered();
    void on_actionExit_triggered();

    void updatePreviewFrame();

private:
    Ui::AnimStudioClass ui;
    std::optional<AnimationData> currentData_;
    QTimer* playbackTimer = nullptr;
    int currentFrameIndex = 0;
    QSize originalFrameSize;
    SpinnerWidget* spinner = nullptr;

    QVector<AnimationFrame> backupFrames_;

    // new metadata dock & widgets
    QDockWidget* metadataDock = nullptr;
    QLineEdit* nameEdit = nullptr;
    QLabel* typeLabel = nullptr;
    QSpinBox* fpsSpin = nullptr;
    QLabel* framesLabel = nullptr;
    QLabel* resolutionLabel = nullptr;
    QSpinBox* loopPointSpin = nullptr;
    QCheckBox* allKeyframesCheck = nullptr;

    QPushButton* quantizeBtn = nullptr;
    QPushButton* undoQuantBtn = nullptr;

    void setupMetadataDock();
    void updateMetadata();
    void onNameEditFinished();
    void onMetadataFpsChanged(int fps);
    void onLoopPointChanged(int frame);
    void onAllKeyframesToggled(bool all);
    void onQuantizeClicked();
    void onUndoQuantize();

    void on_playPauseButton_clicked();
    void on_fpsSpinBox_valueChanged(int value);
    void on_timelineSlider_valueChanged(int value);

    void loadAnimation(AnimationType type, QString path);
    void onAnimationLoadFinished(const std::optional<AnimationData>& data, const QString& errorMessage = "");
    void resetControls();

    void createSpinner(QWidget* parent);
    void deleteSpinner();
};

