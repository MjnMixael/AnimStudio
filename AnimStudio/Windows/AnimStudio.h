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

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // Menu Handlers
    void on_actionExit_triggered();

    // Toolbar Handlers
    void on_actionOpenImageSequence_triggered();
    void on_actionImport_Animation_triggered();
    void on_actionClose_Image_Sequence_triggered();
    void on_actionExport_Animation_triggered();
    void on_actionExport_All_Frames_triggered();
    void on_actionExport_Current_Frame_triggered();
    void on_actionReduce_Colors_triggered();
    void on_actionShow_Reduced_Colors_toggled(bool checked);

    
    // Animation Control
    void on_playPauseButton_clicked();
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

    void updateMetadata(std::optional<AnimationData> data);   

    void resetInterface();

    void createSpinner(QWidget* parent);
    void deleteSpinner();

    void adjustPreviewSize();

    // Show memory usage
    qint64 currentMemoryBytes();
};

