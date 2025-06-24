#include "ReduceColors.h"
#include "ui_ReduceColors.h"

#include <QPushButton>
#include <QDialogButtonBox>

ReduceColorsDialog::ReduceColorsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ReduceColorsDialog)
{
    ui->setupUi(this);

    // Grab the “Apply” button (which we’ll treat as “Reduce”)
    QPushButton* reduceBtn = ui->buttonBox->button(QDialogButtonBox::Apply);
    reduceBtn->setText(tr("Reduce"));

    // When they click “Reduce”, emit our signal and close
    connect(reduceBtn, &QPushButton::clicked, this, [this]() {
        emit reduceConfirmed();
        accept();
        });
}

ReduceColorsDialog::~ReduceColorsDialog()
{
    delete ui;
}
