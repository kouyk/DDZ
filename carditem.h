#ifndef CARDITEM_H
#define CARDITEM_H

#include <QObject>
#include <QGraphicsObject>
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>

#include "card.h"

class CardItem : public QGraphicsObject
{
    Q_OBJECT
public:
    CardItem(DDZ::CardType card_type, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    DDZ::CardType getCardType() const { return c_type; }
    bool getChosen() const { return selected; }
    void toggle() { selected = !selected; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QPixmap pixmap;
    DDZ::CardType c_type;
    bool selected;
};

#endif // CARDITEM_H
