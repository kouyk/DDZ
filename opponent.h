#ifndef OPPONENT_H
#define OPPONENT_H

#include <QObject>
#include <QtNetwork>

#include "card.h"
#include "role.h"
#include "handcategory.h"

class Opponent : public QObject
{
    Q_OBJECT
    typedef DDZ::CardType Card;
public:
    Opponent(QObject *parent = nullptr);
    ~Opponent();
    bool connectTo(const QString &server_addr, const int &server_port);
    QAbstractSocket::SocketState getListenState();
    void setSocket(QTcpSocket *newSock);
    void initServer();
    void notifyServerUp(const QHostAddress &addr, const quint16 port);
    void secondConnect(QJsonObject connInfo);
    QJsonObject getConnInfo();

    void giveDeck(const QVector<Card> &hand, const QVector<Card> &hide);
    void announceReadyBW(bool black);
    void announceBid(bool bid);
    void confirmRoles();
    void informCardsDealt(const HandCategory &hand);
    HandCategory getLastDealt() const;
    DDZ::Role getRole() const;
    void setRole(const DDZ::Role &role);
    void informGameOver();
    bool nextRoundReady();

private:
    QString serverAddr;
    quint16 serverPort;
    QTcpSocket *m_socket;
    QByteArray buffer;
    qint32 sz;

    // game related
    DDZ::Role m_role;
    HandCategory lastDealtHand;

    bool writeData(const QByteArray &data);
    void processData(const QByteArray &rawData);
    void initialHandshake(const QJsonObject &json);
    void setRemoteServer(const QJsonObject &json);
    void receiveDeck(const QJsonObject &json);
    void receiveBid(const QJsonObject &json);
    void updateLastDealt(const QJsonObject &json);

    inline bool writeData(const QJsonObject &json) { return writeData(QJsonDocument(json).toJson()); }

private slots:
    void receiveData();

signals:
    // mainwindow related, e.g. server functions
    void serverStart(quint16 port);
    void addressReceived(QString serverAddr);

    // game related
    void blackAndWhite(bool black);
    void incomingDecks(const QVector<Card> &hand, const QVector<Card> &hide);
    void incomingBid(bool bid);
    void rolesConfirmed();
    void dealt();
    void wonGame();
    void canStart();
};

#endif // OPPONENT_H
