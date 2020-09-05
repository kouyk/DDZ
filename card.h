#ifndef CARD_H
#define CARD_H

#include <QObject>
#include <QVariant>

namespace DDZ {
Q_NAMESPACE
enum class CardType
{
    C3, D3, H3, S3,
    C4, D4, H4, S4,
    C5, D5, H5, S5,
    C6, D6, H6, S6,
    C7, D7, H7, S7,
    C8, D8, H8, S8,
    C9, D9, H9, S9,
    C10, D10, H10, S10,
    CJ, DJ, HJ, SJ,
    CQ, DQ, HQ, SQ,
    CK, DK, HK, SK,
    CA, DA, HA, SA,
    C2, D2, H2, S2,
    BJ, RJ
};
Q_ENUM_NS(CardType)

inline QString cardToString(CardType card)
{
    return QVariant::fromValue(card).toString();
}

inline QString cardIdxToString(int idx)
{
    return cardToString(CardType(idx));
}

}

//enum Card
//{
//    C3, D3, H3, S3,
//    C4, D4, H4, S4,
//    C5, D5, H5, S5,
//    C6, D6, H6, S6,
//    C7, D7, H7, S7,
//    C8, D8, H8, S8,
//    C9, D9, H9, S9,
//    C10, D10, H10, S10,
//    CJ, DJ, HJ, SJ,
//    CQ, DQ, HQ, SQ,
//    CK, DK, HK, SK,
//    CA, DA, HA, SA,
//    C2, D2, H2, S2,
//    BJ, RJ
//};

#endif // CARD_H
