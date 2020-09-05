#ifndef CONNECTIONWIZARD_H
#define CONNECTIONWIZARD_H

#include <QDialog>
#include <QDebug>
#include <QPushButton>

namespace Ui {
class ConnectionWizard;
}

class ConnectionWizard : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionWizard(QWidget *parent = nullptr);
    ~ConnectionWizard();

private:
    Ui::ConnectionWizard *ui;

private slots:
    void accept() override;

signals:
    void addressReceived(QString serverAddr);
};

#endif // CONNECTIONWIZARD_H
