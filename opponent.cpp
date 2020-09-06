#include "opponent.h"

// help functions for reading and writing to socket
static inline QByteArray IntToArray(qint32 source)
{
    QByteArray temp;
    QDataStream data(&temp, QIODevice::ReadWrite);
    data << source;
    return temp;
}

static inline qint32 ArrayToInt(QByteArray source)
{
    qint32 temp;
    QDataStream data(&source, QIODevice::ReadWrite);
    data >> temp;
    return temp;
}


Opponent::Opponent(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , sz(0)
    , m_role(DDZ::NIL)
{
}

Opponent::~Opponent()
{
}

/**
 * @brief Opponent::writeData
 * send data by first sending the size of the raw QByteArray
 * @param data
 * @return true on write success, false otherwise
 */
bool Opponent::writeData(const QByteArray &data)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(IntToArray(data.size()));
        m_socket->write(data);
        return m_socket->waitForBytesWritten();
    }
    else
        return false;
}

void Opponent::receiveData()
{
    qint32 dataSize = sz;
    while (m_socket->bytesAvailable() > 0) {
        // store all incoming data into buffer
        buffer.append(m_socket->readAll());
        while ((dataSize == 0 && buffer.size() >= 4) || (dataSize > 0 && buffer.size() >= dataSize)) {
            // read in size of current data array, and subsequently size of the
            // next data array if it happens to come together
            if (dataSize == 0 && buffer.size() >= 4) {
                dataSize = ArrayToInt(buffer.left(4));
                sz = dataSize;
                buffer.remove(0, 4);
            }

            // if the data for the current array is fully received, pass it on
            if (dataSize > 0 && buffer.size() >= dataSize) {
                QByteArray data = buffer.left(dataSize);
                buffer.remove(0, dataSize);
                dataSize = 0;
                sz = dataSize;
                processData(data);
            }
        }
    }
}

bool Opponent::connectTo(const QString &server_addr, const int &server_port)
{
    m_socket->connectToHost(server_addr, server_port);
    if (m_socket->waitForConnected(1000)) {
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
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        delete m_socket;
        m_socket = newSock;
        QObject::connect(m_socket, SIGNAL(readyRead()), this, SLOT(receiveData()));
    }
}

/**
 * @brief Opponent::initServer
 * request receiver to start a tcp server
 */
void Opponent::initServer()
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("Welcome");
    json[QLatin1String("serverStart")] = true;
    writeData(json);
}

/**
 * @brief Opponent::notifyServerUp
 * inform receiver the connection details of the sender's server
 * @param addr
 * @param port
 */
void Opponent::notifyServerUp(const QHostAddress &addr, const quint16 port)
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("updateAddress");
    json[QLatin1String("addr")] = addr.toString();
    json[QLatin1String("port")] = port;
    writeData(json);
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

QJsonObject Opponent::getConnInfo()
{
    QJsonObject json;
    json[QLatin1String("addr")] = serverAddr;
    json[QLatin1String("port")] = serverPort;
    return json;
}

void Opponent::initialHandshake(const QJsonObject &json)
{
    if (json.contains(QLatin1String("serverStart")) && json[QLatin1String("serverStart")].isBool()) {
        if (json[QLatin1String("serverStart")].toBool())
            emit serverStart(0);
        else {
            QString serverAddr;
            if (json.contains(QLatin1String("addr"))
                    && json[QLatin1String("addr")].isString())
                serverAddr.append(json["addr"].toString());

            if (json.contains(QLatin1String("port"))
                    && json[QLatin1String("port")].isDouble()) {
                serverAddr.append(QLatin1Char(':'));
                serverAddr.append(json[QLatin1String("port")].toVariant().toString());
            }
            emit addressReceived(serverAddr);
        }
    }
}

/**
 * @brief Opponent::secondConnect
 * ask receiver to connect to second server
 * @param connInfo
 */
void Opponent::secondConnect(QJsonObject connInfo)
{
    connInfo[QLatin1String("intent")] = QLatin1String("Welcome");
    connInfo[QLatin1String("serverStart")] = false;
    writeData(connInfo);
}

/**
 * @brief Opponent::announceReadyBW
 * starting message to indicate that sender is ready, along with BW info
 * @param black
 */
void Opponent::announceReadyBW(bool black)
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("start");
    json[QLatin1String("black")] = black;
    writeData(json);
}

/**
 * @brief Opponent::giveDeck
 * sender passes the shuffled cards to the receiver
 * @param hand
 * @param hide
 */
void Opponent::giveDeck(const QVector<Card> &hand, const QVector<Card> &hide)
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("distribute");

    QJsonArray handArray;
    for (const auto &c : hand) {
        QJsonObject cardObj;
        cardObj[QLatin1String("value")] = static_cast<int>(c);
        handArray.append(cardObj);
    }
    json[QLatin1String("hand")] = handArray;

    QJsonArray hideArray;
    for (const auto &c : hide) {
        QJsonObject cardObj;
        cardObj[QLatin1String("value")] = static_cast<int>(c);
        hideArray.append(cardObj);
    }
    json[QLatin1String("hide")] = hideArray;

    writeData(json);
}

void Opponent::receiveDeck(const QJsonObject &json)
{
    QVector<Card> shownDeck, hiddenDeck;
    if (json.contains(QLatin1String("hand")) && json[QLatin1String("hand")].isArray()) {
        QJsonArray handArray = json[QLatin1String("hand")].toArray();
        shownDeck.reserve(handArray.size());
        for (const auto &c : handArray) {
            QJsonObject cardObj = c.toObject();
            if (cardObj.contains(QLatin1String("value")) && cardObj[QLatin1String("value")].isDouble())
                shownDeck.append(Card(cardObj[QLatin1String("value")].toDouble()));
        }
    }
    if (json.contains(QLatin1String("hide")) && json[QLatin1String("hide")].isArray()) {
        QJsonArray hideArray = json[QLatin1String("hide")].toArray();
        hiddenDeck.reserve(hideArray.size());
        for (const auto &c : hideArray) {
            QJsonObject cardObj = c.toObject();
            if (cardObj.contains(QLatin1String("value")) && cardObj[QLatin1String("value")].isDouble())
                hiddenDeck.append(Card(cardObj[QLatin1String("value")].toDouble()));
        }
    }
    emit incomingDecks(shownDeck, hiddenDeck);
}

/**
 * @brief Opponent::announceBid
 * any calls for landlord are sent to the receiver
 * @param bid
 */
void Opponent::announceBid(bool bid)
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("landlord");
    json[QLatin1String("bid")] = bid;
    writeData(json);
}

void Opponent::receiveBid(const QJsonObject &json)
{
    if (json.contains(QLatin1String("bid")) && json[QLatin1String("bid")].isBool())
        emit incomingBid(json[QLatin1String("bid")].toBool());
}

/**
 * @brief Opponent::confirmRoles
 * ask bidding for landlord is over and time to start game
 */
void Opponent::confirmRoles()
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("roles");
    writeData(json);
}

/**
 * @brief Opponent::informCardsDealt
 * inform receivers the set of cards dealt by the sender
 * @param hand
 */
void Opponent::informCardsDealt(const HandCategory &hand)
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("move");
    QJsonObject dealtHand;
    hand.write(dealtHand);
    json[QLatin1String("hand")] = dealtHand;
    writeData(json);
}

void Opponent::updateLastDealt(const QJsonObject &json)
{
    if (json.contains(QLatin1String("hand")) && json[QLatin1String("hand")].isObject()) {
        lastDealtHand.read(json[QLatin1String("hand")].toObject());
        emit dealt();
    }
}

/**
 * @brief Opponent::informGameOver
 * sender (winner) will let others know when game has ended
 */
void Opponent::informGameOver()
{
    QJsonObject json;
    json[QLatin1String("intent")] = QLatin1String("gameover");
    writeData(json);
}

/**
 * @brief Opponent::nextRoundReady
 * should the sender want to continue playing, it will be made known this way
 * @return
 */
bool Opponent::nextRoundReady()
{
    lastDealtHand.setPass();
    QJsonObject json;
    json["intent"] = QLatin1String("nextgame");
    return writeData(json);
}

/**
 * @brief Opponent::processData
 * process all fully received data array
 * @param rawData
 */
void Opponent::processData(const QByteArray &rawData)
{
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    QJsonObject &&json = doc.object();

    if (json.contains(QLatin1String("intent")) && json[QLatin1String("intent")].isString())
    {
        auto intent = json[QLatin1String("intent")].toString();
        if (intent == QLatin1String("Welcome"))
            initialHandshake(json);
        else if (intent == QLatin1String("updateAddress"))
            setRemoteServer(json);
        else if (intent == QLatin1String("start")) {
            if (json.contains("black") && json["black"].isBool())
                emit blackAndWhite(json["black"].toBool());
        }
        else if (intent == QLatin1String("distribute"))
            receiveDeck(json);
        else if (intent == QLatin1String("landlord"))
            receiveBid(json);
        else if (intent == QLatin1String("roles"))
            emit rolesConfirmed();
        else if (intent == QLatin1String("move"))
            updateLastDealt(json);
        else if (intent == QLatin1String("gameover"))
            emit wonGame();
        else if (intent == QLatin1String("nextgame"))
            emit canStart();
    }
}

HandCategory Opponent::getLastDealt() const
{
    return lastDealtHand;
}

DDZ::Role Opponent::getRole() const
{
    return m_role;
}

void Opponent::setRole(const DDZ::Role &role)
{
    m_role = role;
}
