#ifndef HANDCATEGORY_H
#define HANDCATEGORY_H

#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "card.h"

class HandCategory
{
public:
    enum Category
    {
        ILLEGAL, SOLO, SOLO_CHAIN, PAIR, PAIR_CHAIN,
        TRIO, TRIO_WITH_SOLO, TRIO_WITH_PAIR, AIRPLANE_NO_WINGS,
        AIRPLANE_SMALL_WINGS, AIRPLANE_BIG_WINGS, FOUR_OF_A_KIND,
        FOUR_OF_A_KIND_DUAL_SOLO, FOUR_OF_A_KIND_DUAL_PAIR, BOMB, ROCKET, PASS
    };
    explicit HandCategory();
    HandCategory(const QJsonObject &json);
    void insert(const DDZ::CardType &card);
    void insert(const int &idx);
    void remove(const DDZ::CardType &card);
    void remove(const int &idx);
    void clear();
    bool isPass() const;
    bool isLegal() const;
    QVector<DDZ::CardType> getRawHand() const;
    bool operator>(const HandCategory &right) const;
    void write(QJsonObject &json) const;
    void read(const QJsonObject &json);
    void setPass();

private:
    Category cat;
    quint8 chainLength;
    quint8 biggest;
    QVector<bool> raw_cards;
    QVector<int> cardCount;

    bool isChain(const quint8 &m_total, const quint8 &linkSize) const;
    bool isSoloChain(const quint8 &m_total) const;
    bool isPairChain(const quint8 &m_total) const;
    bool isAirplaneNoWings(const quint8 &m_total) const;
    bool isAirplaneSmallWings(const quint8 &m_total) const;
    bool isAirplaneBigWings(const quint8 &m_total) const;
    void updateInfo(const Category &newCat, const quint8 &l, const quint8 &bigN);
    void parseHand();
};

#endif // HANDCATEGORY_H
