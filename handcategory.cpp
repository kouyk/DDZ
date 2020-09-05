#include "handcategory.h"

HandCategory::HandCategory()
    : cat(PASS)
    , chainLength(0)
    , biggest(0)
    , raw_cards(54, false)
    , cardCount(15, 0)
{
}

HandCategory::HandCategory(const QJsonObject &json)
    : cat(ILLEGAL)
    , chainLength(0)
    , biggest(0)
    , raw_cards(54, false)
    , cardCount(15, 0)
{
    read(json);
}

void HandCategory::insert(const DDZ::CardType &card)
{
    raw_cards[static_cast<int>(card)] = true;
    if (card < DDZ::CardType::BJ)
        ++cardCount[static_cast<int>(card) / 4];
    else
        ++cardCount[static_cast<int>(card) - static_cast<int>(DDZ::CardType::BJ) + 13];
    parseHand();
}

void HandCategory::insert(const int &idx)
{
    if (idx > 0 && idx < 54)
    {
        raw_cards[idx] = true;
        if (DDZ::CardType(idx) < DDZ::CardType::BJ)
            ++cardCount[idx / 4];
        else
            ++cardCount[idx - static_cast<int>(DDZ::CardType::BJ) + 13];
        parseHand();
    }
}

void HandCategory::remove(const DDZ::CardType &card)
{
    raw_cards[static_cast<int>(card)] = false;
    if (card < DDZ::CardType::BJ)
        --cardCount[static_cast<int>(card) / 4];
    else
        --cardCount[static_cast<int>(card) - static_cast<int>(DDZ::CardType::BJ) + 13];
    parseHand();
}

void HandCategory::remove(const int &idx)
{
    if (idx > 0 && idx < 54)
        raw_cards[idx] = false;
    if (DDZ::CardType(idx) < DDZ::CardType::BJ)
        --cardCount[idx / 4];
    else
        --cardCount[idx - static_cast<int>(DDZ::CardType::BJ) + 13];
    parseHand();
}

void HandCategory::clear()
{
    cat = ILLEGAL;
    chainLength = 0;
    biggest = 0;
    raw_cards.fill(false);
    cardCount.fill(0);
}

HandCategory::Category HandCategory::getCategory() const
{
    return cat;
}

QVector<DDZ::CardType> HandCategory::getRawHand() const
{
    QVector<DDZ::CardType> rawHand;
    for (int i = 0; i < raw_cards.size(); ++i)
    {
        if (raw_cards[i])
            rawHand.append(DDZ::CardType(i));
    }
    return rawHand;
}

bool HandCategory::operator>(const HandCategory &right) const
{
    bool result = false;
    if (cat == ROCKET)
        result = true;
    else if (cat == BOMB && right.cat < BOMB)
        result = true;
    else if (cat == right.cat && chainLength == right.chainLength)
        result = biggest > right.biggest;

    return result;
}

void HandCategory::write(QJsonObject &json) const
{
    json["category"] = cat;
    json["biggest"] = static_cast<int>(biggest);
    json["chainLength"] = chainLength;
    QJsonArray rawHandArray;
    for (const auto &c : raw_cards)
        rawHandArray.append(c);
    json["raw_cards"] = rawHandArray;
}

bool HandCategory::read(const QJsonObject &json)
{
    if (json.contains("category") && json["category"].isDouble())
        cat = Category(json["category"].toDouble());
    if (json.contains("biggest") && json["biggest"].isDouble())
        biggest = json["biggest"].toDouble();
    if (json.contains("chainLength") && json["chainLength"].isDouble())
        chainLength = json["chainLength"].toDouble();
    if (json.contains("raw_cards") && json["raw_cards"].isArray())
    {
        QJsonArray rawHandArray = json["raw_cards"].toArray();
        raw_cards.clear();
        for (const auto &c : rawHandArray)
            raw_cards.append(c.toBool());
        cardCount.fill(0);
        for (int i = 0; i < 52; ++i)
            if (raw_cards[i])
                ++cardCount[i/4];
        if (raw_cards[52])
            ++cardCount[13];
        if (raw_cards[53])
            ++cardCount[14];
    }
    return true;
}

void HandCategory::setPass()
{
    cat = PASS;
    chainLength = 0;
    biggest = 0;
    raw_cards.fill(false);
    cardCount.fill(0);
}

bool HandCategory::isChain(const quint8 &m_total, const quint8 &linkSize) const
{
    auto linkCount = m_total / linkSize;
    auto end = (linkCount == 1) ? cardCount.end() : cardCount.begin()+12;
    auto it = std::search_n(cardCount.begin(), end, linkCount, linkSize);
    return it != end;
}

bool HandCategory::isSoloChain(const quint8 &m_total) const
{
    return isChain(m_total, 1);
}

bool HandCategory::isPairChain(const quint8 &m_total) const
{
    return isChain(m_total, 2);
}

bool HandCategory::isAirplaneNoWings(const quint8 &m_total) const
{
    return isChain(m_total, 3);
}

bool HandCategory::isAirplaneSmallWings(const quint8 &m_total) const
{
    auto numSetsReq = m_total / 4;
    auto jokers = cardCount[13] + cardCount[14];
    return isAirplaneNoWings(m_total - numSetsReq)
           && std::count(cardCount.begin(), cardCount.end(), 1) == numSetsReq
           && jokers <= 1;
}

bool HandCategory::isAirplaneBigWings(const quint8 &m_total) const
{
    auto numSetsReq = m_total / 5;
    return isAirplaneNoWings(m_total - numSetsReq * 2)
            && std::count(cardCount.begin(), cardCount.end(), 2) == numSetsReq;
}

void HandCategory::updateInfo(const HandCategory::Category &newCat, const quint8 &l, const quint8 &bigN)
{
    cat = newCat;
    chainLength = l;
    auto it = std::find(cardCount.rbegin(), cardCount.rend(), bigN);
    biggest = 14 - std::distance(cardCount.rbegin(), it);
}

void HandCategory::parseHand()
{
    const auto n = std::accumulate(cardCount.begin(), cardCount.end(), 0);
    if (n == 0)
        updateInfo(ILLEGAL, 0, 255);
    else if (n == 2 && cardCount[13] == 1 && cardCount[14] == 1)
        updateInfo(ROCKET, 1, 1);
    else if (n == 4 && std::any_of(cardCount.begin(), cardCount.end(),
                                   [](const auto &value){ return value == 4; }))
        updateInfo(BOMB, 1, 4);
    else if (n % 5 == 0 && isAirplaneBigWings(n))
        updateInfo((n == 5) ? TRIO_WITH_PAIR : AIRPLANE_BIG_WINGS, n / 5, 3);
    else if (n % 4 == 0 && isAirplaneSmallWings(n))
        updateInfo((n == 4) ? TRIO_WITH_SOLO : AIRPLANE_SMALL_WINGS, n / 4, 3);
    else if (n % 3 == 0 && isAirplaneNoWings(n))
        updateInfo((n == 3) ? TRIO : AIRPLANE_NO_WINGS, n / 3, 3);
    else if (n % 2 == 0 && n != 4 && isPairChain(n))
        updateInfo((n == 2) ? PAIR : PAIR_CHAIN, n / 2, 2);
    else if (n >= 5 && n <= 12 && isSoloChain(n))
        updateInfo(SOLO_CHAIN, n, 1);
    else if (n == 1)
        updateInfo(SOLO, 1, 1);
    else if (n == 6)
    {
        auto jokers = cardCount[13] + cardCount[14];
        if (std::any_of(cardCount.begin(), cardCount.end(),
                        [](const auto &value){ return value == 4; })
            && std::count(cardCount.begin(), cardCount.end(), 1) == 2
            && jokers < 2)
            updateInfo(FOUR_OF_A_KIND_DUAL_SOLO, 1, 4);
        else
            updateInfo(ILLEGAL, 0, 255);
    }
    else if (n == 8)
    {
        if (std::count(cardCount.begin(), cardCount.end(), 4) == 1
                && std::count(cardCount.begin(), cardCount.end(), 2) == 1)
            updateInfo(FOUR_OF_A_KIND_DUAL_PAIR, 1, 4);
        else
            updateInfo(ILLEGAL, 0, 255);
    }
    else
        updateInfo(ILLEGAL, 0, 255);
//    for (const auto &c : cardCount)
//        qDebug() << c;

//    qDebug() << cat << endl;
}
