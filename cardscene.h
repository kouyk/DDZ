#ifndef CARDSCENE_H
#define CARDSCENE_H

#include <QObject>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

#include "card.h"
#include "carditem.h"

class CardScene : public QGraphicsScene
{
    Q_OBJECT
public:
    using QGraphicsScene::QGraphicsScene;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

signals:
    void chosen(DDZ::CardType ctype, bool selected);
};

#endif // CARDSCENE_H
