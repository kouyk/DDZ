#include "carditem.h"

CardItem::CardItem(DDZ::CardType card_type, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , c_type(card_type)
    , selected(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    pixmap.load(QString(QLatin1Literal(":/images/%1.svg")).arg(DDZ::cardToString(c_type)));
}

QRectF CardItem::boundingRect() const
{
    return QRectF(-pixmap.width() / 2, -pixmap.height() / 2, pixmap.width(), pixmap.height());
}

void CardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (!pixmap.isNull())
    {
        painter->drawPixmap(QPointF(-pixmap.width() / 2, -pixmap.height() / 2),
                            pixmap);
        if (selected)
        {
            QPainterPath path;
            path.addRoundedRect(boundingRect(), 9, 9);
            painter->fillPath(path, QBrush(QColor(128, 128, 255, 128)));
        }

    }
    else
        painter->drawRoundedRect(-111, -162, 223, 324, 5, 5);
}

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
}
