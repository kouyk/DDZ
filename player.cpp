#include "player.h"
#include "ui_player.h"

Player::Player(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Player)
    , nextPlayer(nullptr), prevPlayer(nullptr)
    , m_role(DDZ::NIL)
    , firstBidder(false), defaultBidder(false), needSnatchBid(false)
    , gameState(UNKNOWN)
    , blacks(3)
{
    ui->setupUi(this);
    blacks.reserve(3);
    blacks.append(QRandomGenerator::system()->generateDouble() < 0.5);

    auto *scene = new CardScene;
    ui->myCardsView->setScene(scene);
    ui->myCardsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->myCardsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(scene, &CardScene::chosen, this, &Player::chooseCard);

    ui->threeCardsView->setScene(new QGraphicsScene);
    ui->lastDealView->setScene(new QGraphicsScene);

    createButtons();
    gameStartInit();

    auto retainSize = [](QWidget *wid) {
        auto policy = wid->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        wid->setSizePolicy(policy);
    };
    retainSize(ui->myCardsView);
    retainSize(ui->threeCardsView);
    retainSize(ui->lastDealView);
    retainSize(ui->roleLabel);
    retainSize(ui->choiceLabel);
    retainSize(ui->prevPlayerChoiceLabel);
    retainSize(ui->prevPlayerRoleLabel);
    retainSize(ui->nextPlayerChoiceLabel);
    retainSize(ui->nextPlayerRoleLabel);
    retainSize(ui->buttonBox);
}

Player::~Player()
{
    delete ui;
}

void Player::createButtons()
{
    bidButton = ui->buttonBox->addButton(tr("Call"), QDialogButtonBox::YesRole);
    connect(bidButton, &QAbstractButton::clicked, [this](){ makeBid(true); });

    noBidButton = ui->buttonBox->addButton(tr("Don't call"), QDialogButtonBox::NoRole);
    connect(noBidButton, &QAbstractButton::clicked, [this](){ makeBid(false); });

    dealButton = ui->buttonBox->addButton(tr("Deal"), QDialogButtonBox::AcceptRole);
    ui->buttonBox->removeButton(dealButton);
    connect(dealButton, &QAbstractButton::clicked, this, &Player::dealHand);

    passButton = ui->buttonBox->addButton(tr("Pass"), QDialogButtonBox::RejectRole);
    ui->buttonBox->removeButton(passButton);
    connect(passButton, &QAbstractButton::clicked, this, &Player::dealHand);
}

void Player::gameStartInit()
{
    blacks.clear();
    blacks.append(QRandomGenerator::system()->generateDouble() < 0.5);

    ui->myCardsView->scene()->clear();
    ui->myCardsView->setEnabled(false);
    ui->threeCardsView->scene()->clear();
    for (int i = 0; i < 3; ++i) {
        QImage image(QLatin1Literal(":/images/cardback.png"));
        image = image.scaledToHeight(80, Qt::SmoothTransformation);
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setPos(image.width() * i, 0);
        ui->threeCardsView->scene()->addItem(item);
    }
    ui->lastDealView->hide();
    ui->roleLabel->hide();
    ui->choiceLabel->hide();
    ui->prevPlayerChoiceLabel->hide();
    ui->prevPlayerRoleLabel->hide();
    ui->p_cardsRemainingNumber->display(17);
    ui->nextPlayerChoiceLabel->hide();
    ui->nextPlayerRoleLabel->hide();
    ui->n_cardsRemainingNumber->display(17);
}

/**
 * @brief Player::takeover
 * Upon connection of other 2 players, mainwindow will handover the connections
 * to player
 * @param b: the next player who moves after the current player
 * @param c: the other player who is always before the current player
 * @param if the player is last to join, can help break the tie during B&W
 */
void Player::takeover(Opponent *b, Opponent *c, const bool &lastToJoin)
{
    prevPlayer = c;
    nextPlayer = b;
    defaultBidder = lastToJoin;

    connect(prevPlayer, &Opponent::incomingDecks, this, &Player::receiveCards);
    connect(nextPlayer, &Opponent::incomingDecks, this, &Player::receiveCards);
    connect(prevPlayer, &Opponent::incomingBid, this, &Player::processBid);
    connect(nextPlayer, &Opponent::incomingBid, this, &Player::processBid);
    connect(prevPlayer, &Opponent::rolesConfirmed, this, &Player::confirmRoles);
    connect(prevPlayer, &Opponent::dealt, this, &Player::opponentDealt);
    connect(nextPlayer, &Opponent::dealt, this, &Player::opponentDealt);
    connect(prevPlayer, &Opponent::wonGame, this, &Player::lostGame);
    connect(nextPlayer, &Opponent::wonGame, this, &Player::lostGame);
    connect(prevPlayer, &Opponent::canStart, this, &Player::playerReadyForBW);
    connect(nextPlayer, &Opponent::canStart, this, &Player::playerReadyForBW);
    playBW();
}

/**
 * @brief Player::playBW
 * inform other players of the current decision
 */
void Player::playBW()
{
    prevPlayer->announceReadyBW(blacks[0]);
    nextPlayer->announceReadyBW(blacks[0]);
}

/**
 * @brief Player::bwDecisionReceived
 * opponent send in their decision
 * @param black: opponent's choice
 */
void Player::bwDecisionReceived(const bool &black)
{
    blacks.append(black);

    // player needs to be different to win, or the default will be selected
    // winner will distribute card and also start bidding first
    if (blacks.size() == 3 && blacks[1] == blacks[2])
        if (blacks[0] != blacks[1] || defaultBidder)
            shuffleDistribute();
}

/**
 * @brief Player::shuffleDistribute
 * randomise cards and split into 3 decks of 17 each, pass on to other players
 */
void Player::shuffleDistribute()
{
    firstBidder = true;
    QVector<Card> deck, myCards, nCards, pCards;
    for (int i = 0; i < 54; ++i)
        deck.append(Card(i));

    // shuffle and distribute cards
    std::shuffle(deck.begin(), deck.end(), std::default_random_engine(QRandomGenerator::system()->generate()));
    for (int i = 0; i < 17; ++i) {
        myCards.append(deck.takeLast());
        nCards.append(deck.takeLast());
        pCards.append(deck.takeLast());
    }
    receiveCards(myCards, deck);
    prevPlayer->giveDeck(pCards, deck);
    nextPlayer->giveDeck(nCards, deck);
}

void Player::receiveCards(const QVector<Card> &visible, const QVector<Card> &hidden)
{
    gameState = BIDDING;
    m_cards = visible;
    publicCards = hidden;
    std::sort(m_cards.begin(), m_cards.end());
    std::sort(publicCards.begin(), publicCards.end());
    displayCards();

    // last to call for landlord wins, hence first bidder is landlord by default
    m_role = DDZ::LANDLORD;
    prevPlayer->setRole(DDZ::PEASANT);
    nextPlayer->setRole(DDZ::PEASANT);
    if (!firstBidder) {
        m_role = DDZ::PEASANT;
        static_cast<Opponent*>(sender())->setRole(DDZ::LANDLORD);
    }

    ui->buttonBox->setVisible(firstBidder);
    emit beginGame();
}

/**
 * @brief Player::displayCards
 * show the cards that the current player have on hand
 */
void Player::displayCards()
{
    auto *scene = ui->myCardsView->scene();
    scene->clear();
    for (int i = 0; i < m_cards.size(); ++i) {
        auto *item = new CardItem(m_cards[i]);
        item->setPos(i * 32, 0);
        scene->addItem(item);
    }

    // QGraphicsView automatically expands but does not contract,
    // workaround to ensure no whitespace is left behind by the cards dealt
    scene->setSceneRect(scene->itemsBoundingRect());
    ui->myCardsView->setFixedSize(scene->width(), scene->height());
}

void Player::makeBid(const bool &bid)
{
    if (bid) {
        ui->choiceLabel->setText(needSnatchBid ? tr("Snatching") : tr("Calling"));
        m_role = DDZ::LANDLORD;
        prevPlayer->setRole(DDZ::PEASANT);
        nextPlayer->setRole(DDZ::PEASANT);
    }
    else
        ui->choiceLabel->setText(needSnatchBid ? tr("Not snatching") : tr("Not calling"));

    ui->choiceLabel->show();
    ui->buttonBox->removeButton(bidButton);
    ui->buttonBox->removeButton(noBidButton);
    ui->buttonBox->addButton(dealButton, QDialogButtonBox::AcceptRole);
    ui->buttonBox->addButton(passButton, QDialogButtonBox::RejectRole);
    ui->buttonBox->hide();
    dealButton->setEnabled(false);
    passButton->setEnabled(false);
    gameState = CONFIRMATION;

    nextPlayer->announceBid(bid);
    prevPlayer->announceBid(bid);
}

/**
 * @brief Player::updateLastDeal
 * if opponent did not pass, display the cards they have just dealt
 * @param latestHand
 */
void Player::updateLastDeal(const HandCategory &latestHand)
{
    if (latestHand.isPass())
        return;

    auto *scene = ui->lastDealView->scene();
    auto cards = latestHand.getRawHand();

    scene->clear();
    for (int i = 0; i < cards.size(); ++i) {
        QImage image(QStringLiteral(":/images/%1.svg").arg(DDZ::cardToString(cards[i])));
        image = image.scaledToHeight(150, Qt::SmoothTransformation);
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setPos(image.width() * i / 5, 0);
        scene->addItem(item);
    }
    scene->setSceneRect(scene->itemsBoundingRect());
    ui->lastDealView->setFixedSize(scene->width(), scene->height());
    ui->lastDealView->show();
}

/**
 * @brief Player::processBid
 * process calls for landlord
 * @param incomingBid
 */
void Player::processBid(const bool &incomingBid)
{
    // update the choice of the sender
    auto *oppoChoiceLabel = (sender() == nextPlayer) ? ui->nextPlayerChoiceLabel
                                                     : ui->prevPlayerChoiceLabel;
    if (!needSnatchBid) {
        if (incomingBid) {
            // once anyone bids, current player needs to snatch
            needSnatchBid = incomingBid;
            bidButton->setText(tr("Snatch"));
            noBidButton->setText(tr("Don't snatch"));

            oppoChoiceLabel->setText(tr("Calling"));
        }
        else
            oppoChoiceLabel->setText(tr("Not calling"));
    }
    else
        oppoChoiceLabel->setText(incomingBid ? tr("Snatching") : tr("Not snatching"));

    // anyone who bids becomes the landlord
    if (incomingBid) {
        m_role = DDZ::PEASANT;
        if (sender() == nextPlayer) {
            prevPlayer->setRole(DDZ::PEASANT);
            nextPlayer->setRole(DDZ::LANDLORD);
        }
        else if (sender() == prevPlayer) {
            prevPlayer->setRole(DDZ::LANDLORD);
            nextPlayer->setRole(DDZ::PEASANT);
        }
    }

    if (sender() == prevPlayer)
        play();
}

/**
 * @brief Player::confirmRoles
 * all roles are updated in real-time, determined by the bids thus far
 * once all bids are in, the roles would have be set for the current game
 */
void Player::confirmRoles()
{
    if (gameState != CONFIRMATION)
        return;

    gameState = PLAYING;
    nextPlayer->confirmRoles(); // get next player to switch state as well

    // update both player's and opponents' choice and role labels
    ui->choiceLabel->setText(tr("Pass"));
    ui->choiceLabel->hide();
    ui->roleLabel->setText((m_role == DDZ::PEASANT) ? tr("Peasant") : tr("Landlord"));
    ui->roleLabel->show();

    ui->prevPlayerChoiceLabel->setText(tr("Pass"));
    ui->prevPlayerChoiceLabel->hide();
    ui->prevPlayerRoleLabel->setText((prevPlayer->getRole() == DDZ::PEASANT) ? tr("Peasant") : tr("Landlord"));
    ui->prevPlayerRoleLabel->show();

    ui->nextPlayerChoiceLabel->setText(tr("Pass"));
    ui->nextPlayerChoiceLabel->hide();
    ui->nextPlayerRoleLabel->setText((nextPlayer->getRole() == DDZ::PEASANT) ? tr("Peasant") : tr("Landlord"));
    ui->nextPlayerRoleLabel->show();

    // display the 3 hidden cards
    auto *scene = ui->threeCardsView->scene();
    scene->clear();
    for (int i = 0; i < publicCards.size(); ++i) {
        QImage image(QStringLiteral(":/images/%1.svg").arg(DDZ::cardToString(publicCards[i])));
        image = image.scaledToHeight(80, Qt::SmoothTransformation);
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setPos(image.width() * i, 0);
        scene->addItem(item);
    }
    scene->setSceneRect(scene->itemsBoundingRect());
    ui->threeCardsView->setFixedSize(scene->width(), scene->height());
    ui->threeCardsView->show();

    // if is player is landlord, take the 3 cards
    if (m_role == DDZ::LANDLORD) {
        std::copy(publicCards.begin(), publicCards.end(), std::back_inserter(m_cards));
        std::inplace_merge(m_cards.begin(), m_cards.begin()+17, m_cards.end());
        displayCards();
        play();
    }
    else {
        auto *lcd = (nextPlayer->getRole() == DDZ::LANDLORD) ? ui->n_cardsRemainingNumber
                                                             : ui->p_cardsRemainingNumber;
        lcd->display(20);
    }
}

/**
 * @brief Player::chooseCard
 * update the state of the deal button based on currently selected cards
 * @param c: actual card chosen
 * @param select
 */
void Player::chooseCard(const Player::Card &c, const bool &selecting)
{
    if (gameState != PLAYING)
        return;

    if (selecting)
        hand.insert(c);
    else
        hand.remove(c);

    auto prevHand = prevPlayer->getLastDealt();
    auto prev2Hand = nextPlayer->getLastDealt();

    if (prevHand.isPass()) {
        if (prev2Hand.isPass())
            dealButton->setEnabled(hand.isLegal());
        else
            dealButton->setEnabled(hand > prev2Hand);
    }
    else
        dealButton->setEnabled(hand > prevHand);
}

/**
 * @brief Player::dealHand
 * deal the currently selected cards and make it known to other players
 */
void Player::dealHand()
{
    if (sender() == passButton) {
        hand.setPass();
        ui->choiceLabel->show();
        if (prevPlayer->getLastDealt().isPass())
            ui->lastDealView->hide();
    }
    else {
        auto dealt = hand.getRawHand();
        QVector<Card> newHand(m_cards.size() - dealt.size());
        std::set_difference(m_cards.begin(), m_cards.end(), dealt.begin(), dealt.end(), newHand.begin());
        m_cards = newHand;
        updateLastDeal(hand);
    }
    displayCards();
    ui->buttonBox->hide();
    ui->myCardsView->setEnabled(false);

    prevPlayer->informCardsDealt(hand);
    nextPlayer->informCardsDealt(hand);

    // check for winning condition
    if (m_cards.size() == 0) {
        gameState = FINISHED;
        oppoReady = 0;
        defaultBidder = true;
        prevPlayer->informGameOver();
        nextPlayer->informGameOver();

        // ask if user wants to continue
        auto reply = QMessageBox::question(this, tr("You've won :)"), tr("Do you want to play another round?"));
        if (reply == QMessageBox::Yes)
            play();
        else
            emit quitGame();
    }
}

/**
 * @brief Player::opponentDealt
 * display the cards dealt by opponents and update counter accordingly
 * let current player choose cards if necessary
 */
void Player::opponentDealt()
{
    Opponent *opponent = static_cast<Opponent*>(sender());
    const auto &latestOppoHand = opponent->getLastDealt();
    auto totalCardsDealt = latestOppoHand.getRawHand().size();
    QLCDNumber *lcd;
    QLabel *choiceLabel;

    if (!latestOppoHand.isPass())
        updateLastDeal(latestOppoHand);

    if (opponent == prevPlayer) {
        choiceLabel = ui->prevPlayerChoiceLabel;
        lcd = ui->p_cardsRemainingNumber;
        play();
    }
    else {
        choiceLabel = ui->nextPlayerChoiceLabel;
        lcd = ui->n_cardsRemainingNumber;

        // if both current player and next player passed, the cards last dealt is of no concern
        if (latestOppoHand.isPass() && hand.isPass())
            ui->lastDealView->hide();
    }
    choiceLabel->setVisible(latestOppoHand.isPass());
    lcd->display(lcd->intValue() - totalCardsDealt);
}

/**
 * @brief Player::lostGame
 * slot for when opponent wins and sends notification
 */
void Player::lostGame() {
    oppoReady = 0;
    gameState = FINISHED;
    defaultBidder = false;
    ui->buttonBox->hide();
    ui->myCardsView->setEnabled(false);
    auto reply = QMessageBox::question(this, tr("You've lost :("), tr("Do you want to play another round?"));
    if (reply == QMessageBox::Yes)
        play();
    else
        emit quitGame();
}

void Player::playerReadyForBW()
{
    ++oppoReady;
    if (oppoReady == 3) {
        gameState = BIDDING;
        playBW();
    }
}

void Player::play()
{
    switch(gameState) {
    case BIDDING: {
        ui->buttonBox->show();
        break;
    }
    case CONFIRMATION: {
        if (firstBidder)
            confirmRoles();
        break;
    }
    case PLAYING: {
        hand.clear();
        ui->buttonBox->show();
        ui->myCardsView->setEnabled(true);
        if (nextPlayer->getLastDealt().isPass() && prevPlayer->getLastDealt().isPass()) {
            passButton->setEnabled(false);
            ui->lastDealView->hide();
        }
        else
            passButton->setEnabled(true);

        dealButton->setEnabled(false);
        ui->choiceLabel->hide();
        break;
    }
    case FINISHED: {
        m_cards.clear();
        publicCards.clear();
        needSnatchBid = false;
        hand.clear();
        firstBidder = false;

        gameStartInit();

        ui->buttonBox->removeButton(dealButton);
        ui->buttonBox->removeButton(passButton);
        bidButton->setText(tr("Call"));
        ui->buttonBox->addButton(bidButton, QDialogButtonBox::YesRole);
        noBidButton->setText(tr("Don't call"));
        ui->buttonBox->addButton(noBidButton, QDialogButtonBox::NoRole);
        ui->buttonBox->hide();

        playerReadyForBW();
        prevPlayer->nextRoundReady();
        nextPlayer->nextRoundReady();
        break;
    }
    default: {
        qDebug() << "Something went wrong!";
    }
    }
}
