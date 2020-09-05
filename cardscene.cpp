#include "cardscene.h"

void CardScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    auto *item = itemAt(event->scenePos(), QTransform());
    CardItem *citem = dynamic_cast<CardItem*>(item);
    if (citem != nullptr)
    {
        citem->toggle();
        update();
        emit chosen(citem->getCardType(), citem->getChosen());
    }
}
