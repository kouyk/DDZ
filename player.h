#ifndef PLAYER_H
#define PLAYER_H

#include <QWidget>
#include <QGraphicsView>
#include <QPushButton>
#include <QMessageBox>

#include "opponent.h"
#include "card.h"
#include "role.h"
#include "carditem.h"
#include "handcategory.h"
#include "cardscene.h"

namespace Ui {
class Player;
}

class Player : public QWidget
{
    Q_OBJECT
    typedef DDZ::CardType Card;

public:
    enum State {UNKNOWN, BIDDING, CONFIRMATION, PLAYING, FINISHED};
    Q_ENUM(State);
    explicit Player(QWidget *parent = nullptr);
    ~Player();

    void bwDecisionReceived(const bool &black);
    void takeover(Opponent *b, Opponent *c, const bool &lastToJoin);
    void snatch();

private:
    Ui::Player *ui;

    QPushButton *bidButton;
    QPushButton *noBidButton;
    QPushButton *dealButton;
    QPushButton *passButton;

    Opponent *nextPlayer;
    Opponent *prevPlayer;

    QVector<Card> m_cards;
    QVector<Card> publicCards;
    DDZ::Role m_role;
    bool firstBidder;
    bool defaultBidder;
    bool needSnatchBid;
    State gameState;
    QVector<bool> blacks;
    HandCategory hand;
    int oppoReady;

    void createButtons();

    void playBW();
    void makeBid(const bool &bid);
    void shuffleDistribute();
    void play();
    void displayCards();
    void updateLastDeal(const HandCategory &latestHand);

    void gameStartInit();
    
private slots:
    void receiveCards(const QVector<Card> &visible, const QVector<Card> &hidden);
    void processBid(const bool &bid);
    void confirmRoles();
    void chooseCard(const Card &c, const bool &select);
    void dealHand();
    void opponentDealt();
    void lostGame();
    void playerReadyForBW();

signals:
    void beginGame();
    void bidDecided(bool bid);
    void quitGame();
};

#endif // PLAYER_H
