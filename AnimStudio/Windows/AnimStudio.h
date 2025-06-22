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
    void on_actionOpenImageSequence_triggered();
    void on_actionClose_Image_Sequence_triggered();
    void on_actionImport_Animation_triggered();
    void on_actionExit_triggered();

    void on_playPauseButton_clicked();
    void on_timelineSlider_valueChanged(int value);

private:
    Ui::AnimStudioClass ui;
    AnimationController* animCtrl = nullptr;

    SpinnerWidget* spinner = nullptr;

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
    void updateMetadata(std::optional<AnimationData> data);
    void onNameEditFinished();
    void onMetadataFpsChanged(int fps);
    void onLoopPointChanged(int frame);
    void onAllKeyframesToggled(bool all);
    void onQuantizeClicked();
    void onUndoQuantize();

    // Unused?
    void on_fpsSpinBox_valueChanged(int value);
    

    void resetControls();

    void createSpinner(QWidget* parent);
    void deleteSpinner();
};

