#include "connectionwizard.h"
#include "ui_connectionwizard.h"

ConnectionWizard::ConnectionWizard(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWizard)
{
    ui->setupUi(this);
}

void ConnectionWizard::accept()
{
    emit addressReceived(ui->lineEdit->text());
    ui->buttonBox->setDisabled(true);
    ui->informationLabel->setText(tr("Waiting for other connections..."));
}

ConnectionWizard::~ConnectionWizard()
{
    delete ui;
}
