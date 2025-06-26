#pragma once

#include <QtWidgets/QMainWindow>
#include <qmainwindow.h>
#include <qvector>
#include <qimage>
#include <qtimer>
#include <QtConcurrent/QtConcurrent>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <optional>
#include "ui_AnimStudio.h"
#include "Animation/AnimationData.h"
#include "Animation/AnimationController.h"
#include "Widgets/SpinnerWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AnimStudioClass; }
QT_END_NAMESPACE

class AnimStudio : public QMainWindow {
    Q_OBJECT

public:
    AnimStudio(QWidget* parent = nullptr);
    ~AnimStudio();

    static constexpr char const* Version = "0.8.5";

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // Menu Handlers
    void on_actionExit_triggered();
    void on_actionAbout_triggered();

    // Toolbar Handlers
    void on_actionOpenImageSequence_triggered();
    void on_actionImport_Animation_triggered();
    void on_actionClose_Image_Sequence_triggered();
    void on_actionExport_Animation_triggered();
    void on_actionExport_All_Frames_triggered();
    void on_actionExport_Current_Frame_triggered();
    void on_actionReduce_Colors_triggered();
    void on_actionShow_Reduced_Colors_toggled(bool checked);
    void on_actionCancel_Reduce_Colors_triggered();
    void on_actionCycle_Transparency_Mode_triggered();
    void on_actionToggle_Animation_Resizing_toggled(bool checked);
    
    // Animation Control
    void on_playPauseButton_clicked();
    void on_jumpStartButton_clicked();
    void on_jumpLoopButton_clicked();
    void on_previousFrameButton_clicked();
    void on_nextFrameButton_clicked();
    void on_timelineSlider_valueChanged(int value);

    // Metadata Updates
    void on_fpsSpinBox_valueChanged(int value);
    void on_loopFrameSpinBox_valueChanged(int value);
    void on_nameEdit_editingFinished();
    void on_keyframeAllCheckBox_toggled(bool checked);

    // Status Bar Updates
    void updateMemoryUsage();

private:
    Ui::AnimStudioClass ui;
    AnimationController* animCtrl = nullptr;
    SpinnerWidget* spinner = nullptr;
    QLabel* m_rightStatusLabel;
    QTimer* m_memTimer;
    bool m_autoResize = true;
    bool m_taskRunning = false;

    enum class BackgroundMode { Checker, SolidGreen, None };

    BackgroundMode m_bgMode = BackgroundMode::None;

    void updateMetadata(std::optional<AnimationData> data);

    void setBackgroundMode(BackgroundMode);

    void resetInterface();

    void createSpinner(QWidget* parent);
    void deleteSpinner();

    void adjustPreviewSize();

    void updateFrameTimeDisplay();

    void toggleToolebarControls();

    // Show memory usage
    qint64 currentMemoryBytes();
};

