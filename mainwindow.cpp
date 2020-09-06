#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tcpServer(nullptr)
    , player(new Player(this))
    , opponentB(new Opponent(this))
    , opponentC(new Opponent(this))
    , connCount(0)
{
    setWindowTitle(tr("Doudizhu"));
    ConnectionWizard *connWidget = new ConnectionWizard;
    connect(connWidget, &ConnectionWizard::addressReceived,
            this, &MainWindow::addrReceived);
    connect(connWidget, &ConnectionWizard::rejected, this, &MainWindow::close);
    setCentralWidget(connWidget);
    player->hide();

    connect(opponentC, &Opponent::serverStart, this, &MainWindow::startServer);
    connect(opponentC, &Opponent::addressReceived, this, &MainWindow::addrReceived);
    connect(player, &Player::beginGame, this, &MainWindow::showGameInterface);
    connect(player, &Player::quitGame, this, &MainWindow::close);
    connect(opponentB, &Opponent::blackAndWhite, player, &Player::bwDecisionReceived);
    connect(opponentC, &Opponent::blackAndWhite, player, &Player::bwDecisionReceived);
}

MainWindow::~MainWindow()
{
}

void MainWindow::startServer(quint16 port)
{
    tcpServer = new QTcpServer;
    if (tcpServer->listen(QHostAddress::Any, port)) {
        QObject::connect(tcpServer, SIGNAL(newConnection()), SLOT(acceptPeer()));
        if (opponentC->getListenState() == QAbstractSocket::ConnectedState) {
            auto host = QHostAddress(QStringLiteral("127.0.0.233"));
            auto hostport = tcpServer->serverPort();
            opponentC->notifyServerUp(host, hostport);
        }
    }
}

void MainWindow::showGameInterface()
{
    setCentralWidget(player);
    centralWidget()->show();
}

void MainWindow::incConnCount()
{
    ++connCount;
    if (connCount == 2)
        player->takeover(opponentB, opponentC, tcpServer == nullptr);
}

void MainWindow::acceptPeer()
{
    while (tcpServer->hasPendingConnections()) {
        if (opponentB->getListenState() == QAbstractSocket::UnconnectedState) {
            opponentB->setSocket(tcpServer->nextPendingConnection());
            opponentB->initServer();
            incConnCount();
        }
        else if (opponentC->getListenState() == QAbstractSocket::UnconnectedState) {
            opponentC->setSocket(tcpServer->nextPendingConnection());
            opponentC->secondConnect(opponentB->getConnInfo());
            incConnCount();
        }
        // rejects everyone else
        else {
            tcpServer->nextPendingConnection()->disconnectFromHost();
        }
    }
}

void MainWindow::addrReceived(const QString &serverAddr)
{
    QString addr("127.0.0.233");
    quint16 port = 33333;
    bool connectionSuccess = false;

    if (!serverAddr.isEmpty()) {
        auto addrList = serverAddr.split(':');
        if (addrList.size() == 2) {
            addr = addrList[0];
            port = addrList[1].toInt();

            if (opponentC->getListenState() == QAbstractSocket::UnconnectedState)
                connectionSuccess = opponentC->connectTo(addr, port);
            else if (opponentB->getListenState() == QAbstractSocket::UnconnectedState) {
                connectionSuccess = opponentB->connectTo(addr, port);

                /**
                 * only the last player to join will reach here since it will
                 * not have to start a server, and the last player will have the
                 * two opponents connected in reverse order, so a swap is needed
                 * to ensure turn consistency amongst all players
                 */
                std::swap(opponentB, opponentC);
            }
        }
    }

    if (!connectionSuccess && tcpServer == nullptr)
        startServer(port);
    else
        incConnCount();
}
