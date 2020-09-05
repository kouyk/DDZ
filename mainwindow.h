#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QInputDialog>
#include <QDebug>
#include <QtNetwork>
#include <QStringList>
#include <QRandomGenerator>

#include "opponent.h"
#include "connectionwizard.h"
#include "card.h"
#include "player.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    typedef DDZ::CardType Card;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    QTcpServer *tcpServer;

    Player *player;
    Opponent *opponentB;
    Opponent *opponentC;

    int connCount;
    QVector<bool> blacks;

    void startServer(quint16 port);
    void incConnCount();
    void decConnCount();
    void showGameInterface();

private slots:
    void acceptPeer();
    void addrReceived(const QString &serverAddr);
signals:
    void quitGame();
};
#endif // MAINWINDOW_H
