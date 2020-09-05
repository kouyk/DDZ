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
    connect(opponentB, &Opponent::blackAndWhite, player, &Player::determineStart);
    connect(opponentC, &Opponent::blackAndWhite, player, &Player::determineStart);
}

MainWindow::~MainWindow()
{
}

void MainWindow::startServer(quint16 port)
{
    tcpServer = new QTcpServer;
    if (tcpServer->listen(QHostAddress::Any, port))
    {
        qDebug() << "Server started on port:" << tcpServer->serverPort();
        QObject::connect(tcpServer, SIGNAL(newConnection()), SLOT(acceptPeer()));
        if (opponentC->getListenState() == QAbstractSocket::ConnectedState)
        {
            auto host = QHostAddress("127.0.0.233");
            auto hostport = tcpServer->serverPort();
            opponentC->notifyServerUp(host, hostport);
        }
    }
    else
    {
        qDebug() << "Server start failed!";
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
    qDebug() << "Number connected:" << connCount;
    if (connCount == 2)
    {
        player->takeover(opponentB, opponentC, tcpServer == nullptr);
    }
}

void MainWindow::acceptPeer()
{
    while (tcpServer->hasPendingConnections())
    {
        if (opponentB->getListenState() == QAbstractSocket::UnconnectedState)
        {
            opponentB->setSocket(tcpServer->nextPendingConnection());
            qDebug() << "Opponent B connected";
            opponentB->initServer();
            incConnCount();
        }
        else if (opponentC->getListenState() == QAbstractSocket::UnconnectedState)
        {
            opponentC->setSocket(tcpServer->nextPendingConnection());
            qDebug() << "Opponent C connected";
            opponentC->secondConnect(opponentB->getConnInfo());
            incConnCount();
        }
        else
        {
            tcpServer->nextPendingConnection()->disconnectFromHost();
            qDebug() << "Incoming connection rejected";
        }
    }
}

void MainWindow::addrReceived(const QString &serverAddr)
{
    QString addr("127.0.0.233");
    quint16 port = 33333;
    bool connectionSuccess = false;

    if (!serverAddr.isEmpty())
    {
        auto addrList = serverAddr.split(':');
        if (addrList.size() == 2)
        {
            addr = addrList[0];
            port = addrList[1].toInt();

            if (opponentC->getListenState() == QAbstractSocket::UnconnectedState)
            {
                connectionSuccess = opponentC->connectTo(addr, port);
                if (connectionSuccess) {
                    qDebug() << "Connected to opponent C";
                }
            }
            else if (opponentB->getListenState() == QAbstractSocket::UnconnectedState)
            {
                connectionSuccess = opponentB->connectTo(addr, port);
                if (connectionSuccess) {
                    qDebug() << "Connected to opponent B";
                }
                std::swap(opponentB, opponentC);
                qDebug() << "Opponent pointers swapped";
            }
        }
    }

    if (!connectionSuccess && tcpServer == nullptr)
        startServer(port);
    else
    {
        incConnCount();
    }
}
