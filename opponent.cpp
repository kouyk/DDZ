#include "opponent.h"

static inline QByteArray IntToArray(qint32 source);

static inline qint32 ArrayToInt(QByteArray source);

Opponent::Opponent(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , buffer(new QByteArray())
    , sz(new qint32(0))
    , m_role(DDZ::NIL)
{

}

bool Opponent::connectTo(const QString &server_addr, const int &server_port)
{
    m_socket->connectToHost(server_addr, server_port);
    if (m_socket->waitForConnected(1000))
    {
        QObject::connect(m_socket, SIGNAL(readyRead()), this, SLOT(receiveData()));
        return true;
    }
    return false;
}

QAbstractSocket::SocketState Opponent::getListenState()
{
    return m_socket->state();
}

void Opponent::setSocket(QTcpSocket *newSock)
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState)
    {
        delete m_socket;
        m_socket = newSock;
        QObject::connect(m_socket, SIGNAL(readyRead()), this, SLOT(receiveData()));
    }
}

void Opponent::initServer()
{
    // send message to player instance for it to start its server
    QJsonObject json;
    json["intent"] = QStringLiteral("Welcome");
    json["serverStart"] = true;
    writeData(json);
}

void Opponent::notifyServerUp(const QHostAddress &addr, const quint16 port)
{
    QJsonObject json;
    json["intent"] = QStringLiteral("updateAddress");
    json["addr"] = addr.toString();
    json["port"] = port;
    writeData(QJsonDocument(json).toJson());
}

void Opponent::secondConnect(QJsonObject connInfo)
{
    connInfo["intent"] = QStringLiteral("Welcome");
    connInfo["serverStart"] = false;
    writeData(connInfo);
}

QJsonObject Opponent::getConnInfo()
{
    QJsonObject json;
    json["addr"] = serverAddr;
    json["port"] = serverPort;
    return json;
}

void Opponent::giveDeck(const QVector<Card> &hand, const QVector<Card> &hide)
{
    QJsonObject json;
    json["intent"] = QStringLiteral("distribute");

    QJsonArray handArray;
    for (const auto &c : hand)
    {
        QJsonObject cardObj;
        cardObj["value"] = static_cast<int>(c);
        handArray.append(cardObj);
    }
    json["hand"] = handArray;

    QJsonArray hideArray;
    for (const auto &c : hide)
    {
        QJsonObject cardObj;
        cardObj["value"] = static_cast<int>(c);
        hideArray.append(cardObj);
    }
    json["hide"] = hideArray;

    writeData(json);
}

bool Opponent::writeData(const QByteArray &data)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        m_socket->write(IntToArray(data.size()));
        m_socket->write(data);
        return m_socket->waitForBytesWritten();
    }
    else
        return false;
}

void Opponent::receiveData()
{
    qint32 m_size = *sz;
    while (m_socket->bytesAvailable() > 0)
    {
        buffer->append(m_socket->readAll());
        while ((m_size == 0 && buffer->size() >= 4)
               || (m_size > 0 && buffer->size() >= m_size))
        {
            if (m_size == 0 && buffer->size() >= 4) // read in size of data
            {
                m_size = ArrayToInt(buffer->left(4));
                *sz = m_size;
                buffer->remove(0, 4);
            }
            if (m_size > 0 && buffer->size() >= m_size) // read in data if all received
            {
                QByteArray data = buffer->left(m_size);
                buffer->remove(0, m_size);
                m_size = 0;
                *sz = m_size;
                processData(data);
            }
        }
    }
}

void Opponent::announceReady(bool black)
{
    QJsonObject json;
    json["intent"] = QStringLiteral("start");
    json["black"] = black;
    writeData(json);
}

void Opponent::announceBid(bool bid)
{
    QJsonObject json;
    json["intent"] = QStringLiteral("landlord");
    json["bid"] = bid;
    writeData(json);
}

void Opponent::confirmRoles()
{
    QJsonObject json;
    json["intent"] = QStringLiteral("roles");
    writeData(json);
}

void Opponent::informHand(const HandCategory &hand)
{
    QJsonObject json;
    json["intent"] = QStringLiteral("move");
    QJsonObject dealtHand;
    hand.write(dealtHand);
    json["hand"] = dealtHand;
    writeData(json);
}

HandCategory Opponent::getLastHand() const
{
    return lastHand;
}

DDZ::Role Opponent::getRole() const
{
    return m_role;
}

void Opponent::setRole(const DDZ::Role &role)
{
    m_role = role;
}

void Opponent::informGameOver()
{
    QJsonObject json;
    json["intent"] = QStringLiteral("gameover");
    writeData(json);
}

bool Opponent::nextRoundReady()
{
    lastHand.setPass();
    QJsonObject json;
    json["intent"] = QStringLiteral("nextgame");
    return writeData(json);
}

void Opponent::processData(const QByteArray &rawData)
{
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    QJsonObject &&json = doc.object();

    if (json.contains("intent") && json["intent"].isString())
    {
        auto intent = json["intent"].toString();
        if (intent == QStringLiteral("Welcome"))
        {
            initialHandshake(json);
        }
        else if (intent == QStringLiteral("updateAddress"))
        {
            setRemoteServer(json);
        }
        else if (intent == QStringLiteral("start"))
        {
            qDebug() << "One start packet";
            if (json.contains("black") && json["black"].isBool())
                emit blackAndWhite(json["black"].toBool());
        }
        else if (intent == QStringLiteral("distribute"))
        {
            receiveCards(json);
        }
        else if (intent == QStringLiteral("landlord"))
        {
            receiveBid(json);
        }
        else if (intent == QStringLiteral("roles"))
        {
            emit rolesConfirmed();
        }
        else if (intent == QStringLiteral("move"))
        {
            updateLastHand(json);
        }
        else if (intent == QStringLiteral("gameover"))
        {
            emit wonGame();
        }
        else if (intent == QStringLiteral("nextgame"))
        {
            emit canStart();
        }
    }
}

void Opponent::initialHandshake(const QJsonObject &json)
{
    if (json.contains("serverStart") && json["serverStart"].isBool())
    {
        if (json["serverStart"].toBool())
        {
            emit serverStart(0);
        }
        else
        {
            QString serverAddr;
            if (json.contains("addr") && json["addr"].isString())
            {
                serverAddr.append(json["addr"].toString());
            }
            if (json.contains("port") && json["port"].isDouble())
            {
                serverAddr.append(':');
                serverAddr.append(json["port"].toVariant().toString());
            }
            emit addressReceived(serverAddr);
        }
    }
}

void Opponent::setRemoteServer(const QJsonObject &json)
{
    if (json.contains("addr") && json["addr"].isString())
    {
        serverAddr = json["addr"].toString();
    }
    if (json.contains("port") && json["port"].isDouble())
    {
        serverPort = json["port"].toInt();
    }
}

void Opponent::receiveCards(const QJsonObject &json)
{
    QVector<Card> mainDeck;
    QVector<Card> threeDeck;
    if (json.contains("hand") && json["hand"].isArray())
    {
        QJsonArray handArray = json["hand"].toArray();
        mainDeck.reserve(handArray.size());
        for (const auto &c : handArray)
        {
            QJsonObject cardObj = c.toObject();
            if (cardObj.contains("value") && cardObj["value"].isDouble())
                mainDeck.append(Card(cardObj["value"].toDouble()));
        }
    }
    if (json.contains("hide") && json["hide"].isArray())
    {
        QJsonArray hideArray = json["hide"].toArray();
        threeDeck.reserve(hideArray.size());
        for (const auto &c : hideArray)
        {
            QJsonObject cardObj = c.toObject();
            if (cardObj.contains("value") && cardObj["value"].isDouble())
                threeDeck.append(Card(cardObj["value"].toDouble()));
        }
    }
    emit incomingDecks(mainDeck, threeDeck);
}

void Opponent::receiveBid(const QJsonObject &json)
{
    if (json.contains("bid") && json["bid"].isBool())
    {
        emit incomingBid(json["bid"].toBool());
    }
}

void Opponent::updateLastHand(const QJsonObject &json)
{
    if (json.contains("hand") && json["hand"].isObject())
    {
        lastHand.read(json["hand"].toObject());
        emit dealt();
    }
}

Opponent::~Opponent()
{
    delete buffer;
    delete sz;
}

QByteArray IntToArray(qint32 source)
{
    QByteArray temp;
    QDataStream data(&temp, QIODevice::ReadWrite);
    data << source;
    return temp;
}

qint32 ArrayToInt(QByteArray source)
{
    qint32 temp;
    QDataStream data(&source, QIODevice::ReadWrite);
    data >> temp;
    return temp;
}
