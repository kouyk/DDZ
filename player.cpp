#include "player.h"
#include "ui_player.h"

Player::Player(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Player)
    , nextPlayer(nullptr), prevPlayer(nullptr)
    , m_role(DDZ::NIL)
    , firstBidder(false), lateComer(false), needSnatchBid(false)
    , gameState(UNKNOWN)
{
    ui->setupUi(this);
    blacks.reserve(3);
    blacks.append(QRandomGenerator::system()->generateDouble() < 0.5);

    auto retainSize = [](QWidget *wid)
    {
        auto policy = wid->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        wid->setSizePolicy(policy);
    };

    auto *scene = new CardScene;
    retainSize(ui->myCardsView);
    ui->myCardsView->setScene(scene);
    ui->myCardsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->myCardsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->myCardsView->show();
    ui->myCardsView->setEnabled(false);
    connect(scene, &CardScene::chosen, this, &Player::chooseCard);

    retainSize(ui->threeCardsView);
    ui->threeCardsView->setScene(new QGraphicsScene);
    for (int i = 0; i < 3; ++i) {
        QImage image(QLatin1Literal(":/images/cardback.png"));
        image = image.scaledToHeight(80, Qt::SmoothTransformation);
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setPos(image.width() * i, 0);
        ui->threeCardsView->scene()->addItem(item);
    }

    retainSize(ui->lastDealView);
    ui->lastDealView->setScene(new QGraphicsScene);
    ui->lastDealView->hide();

    createButtons();

    retainSize(ui->roleLabel);
    ui->roleLabel->hide();

    retainSize(ui->choiceLabel);
    ui->choiceLabel->hide();

    retainSize(ui->prevPlayerChoiceLabel);
    ui->prevPlayerChoiceLabel->hide();

    retainSize(ui->prevPlayerRoleLabel);
    ui->prevPlayerRoleLabel->hide();

    ui->p_cardsRemainingNumber->display(17);

    retainSize(ui->nextPlayerChoiceLabel);
    ui->nextPlayerChoiceLabel->hide();

    retainSize(ui->nextPlayerRoleLabel);
    ui->nextPlayerRoleLabel->hide();

    ui->n_cardsRemainingNumber->display(17);

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

void Player::takeover(Opponent *b, Opponent *c, const bool &lastToJoin)
{
    nextPlayer = b;
    prevPlayer = c;
    lateComer = lastToJoin;

    connect(nextPlayer, &Opponent::incomingDecks, this, &Player::passDeck);
    connect(prevPlayer, &Opponent::incomingDecks, this, &Player::passDeck);
    connect(prevPlayer, &Opponent::incomingBid, this, &Player::processBid);
    connect(nextPlayer, &Opponent::incomingBid, this, &Player::processBid);
    connect(prevPlayer, &Opponent::rolesConfirmed, this, &Player::confirmRoles);
    connect(prevPlayer, &Opponent::dealt, this, &Player::opponentDealt);
    connect(nextPlayer, &Opponent::dealt, this, &Player::opponentDealt);
    connect(prevPlayer, &Opponent::wonGame, this, &Player::lostGame);
    connect(nextPlayer, &Opponent::wonGame, this, &Player::lostGame);
    connect(prevPlayer, &Opponent::canStart, this, &Player::oneReady);
    connect(nextPlayer, &Opponent::canStart, this, &Player::oneReady);
    playBW();
}

void Player::displayCards()
{
    auto *scene = ui->myCardsView->scene();
    scene->clear();
    for (int i = 0; i < m_cards.size(); ++i)
    {
        auto *item = new CardItem(m_cards[i]);
        item->setPos(i * 32, 0);
        scene->addItem(item);
    }
    scene->setSceneRect(scene->itemsBoundingRect());
    ui->myCardsView->setFixedSize(scene->width(), scene->height());
}

void Player::updateLastDeal(const HandCategory &latestHand)
{
    if (latestHand.getCategory() == HandCategory::PASS)
        return;

    auto *scene = ui->lastDealView->scene();
    auto cards = latestHand.getRawHand();

    scene->clear();
    for (int i = 0; i < cards.size(); ++i)
    {
        QImage image(QString(QLatin1Literal(":/images/%1.svg")).arg(DDZ::cardToString(cards[i])));
        image = image.scaledToHeight(150, Qt::SmoothTransformation);
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setPos(image.width() * i / 5, 0);
        scene->addItem(item);
    }
    scene->setSceneRect(scene->itemsBoundingRect());
    ui->lastDealView->setFixedSize(scene->width(), scene->height());
    ui->lastDealView->show();
}

void Player::passDeck(const QVector<Card> &hand, const QVector<Card> &hide)
{
    gameState = BIDDING;
    m_cards = hand;
    hide_cards = hide;
    std::sort(m_cards.begin(), m_cards.end());
    std::sort(hide_cards.begin(), hide_cards.end());
    displayCards();

    m_role = DDZ::LANDLORD;
    nextPlayer->setRole(DDZ::PEASANT);
    prevPlayer->setRole(DDZ::PEASANT);
    if (!firstBidder)
    {
        m_role = DDZ::PEASANT;
        static_cast<Opponent*>(sender())->setRole(DDZ::LANDLORD);
    }

    ui->buttonBox->setVisible(firstBidder);
    emit beginGame();
}

void Player::processBid(const bool &incomingBid)
{
    // TODO update the choice
    auto *oppoChoiceLabel = (sender() == nextPlayer) ? ui->nextPlayerChoiceLabel : ui->prevPlayerChoiceLabel;
    if (!needSnatchBid) // once anyone bids, current player needs to snatch
    {
        if (incomingBid)
        {
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
        if (sender() == nextPlayer)
        {
            prevPlayer->setRole(DDZ::PEASANT);
            nextPlayer->setRole(DDZ::LANDLORD);
        }
        else if (sender() == prevPlayer)
        {
            prevPlayer->setRole(DDZ::LANDLORD);
            nextPlayer->setRole(DDZ::PEASANT);
        }
    }

    if (sender() == prevPlayer)
        play();

    qDebug() << m_role << prevPlayer->getRole() << nextPlayer->getRole();
}

void Player::confirmRoles()
{
    if (gameState != CONFIRMATION)
        return;
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

    gameState = PLAYING;

    nextPlayer->confirmRoles();

    // display the 3 hidden cards
    auto *scene = ui->threeCardsView->scene();
    scene->clear();
    for (int i = 0; i < hide_cards.size(); ++i) {
        QImage image(QString(QLatin1Literal(":/images/%1.svg")).arg(DDZ::cardToString(hide_cards[i])));
        image = image.scaledToHeight(80, Qt::SmoothTransformation);
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setPos(image.width() * i, 0);
        scene->addItem(item);
    }
    scene->setSceneRect(scene->itemsBoundingRect());
    ui->threeCardsView->setFixedSize(scene->width(), scene->height());
    ui->threeCardsView->show();

    // if is landlord, take the 3 cards
    if (m_role == DDZ::LANDLORD)
    {
        qDebug() << "Merging once";
        std::copy(hide_cards.begin(), hide_cards.end(), std::back_inserter(m_cards));
        std::inplace_merge(m_cards.begin(), m_cards.begin()+17, m_cards.end());
        displayCards();
        play();
    }
    else
    {
        auto *lcd = (nextPlayer->getRole() == DDZ::LANDLORD) ? ui->n_cardsRemainingNumber
                                                             : ui->p_cardsRemainingNumber;
        lcd->display(20);
    }
}

void Player::chooseCard(const Player::Card &c, const bool &select)
{
    if (gameState != PLAYING)
        return;

    if (select)
        hand.insert(c);
    else
        hand.remove(c);

    auto prevHand = prevPlayer->getLastHand();
    auto prev2Hand = nextPlayer->getLastHand();

    if (prevHand.getCategory() == HandCategory::PASS)
    {
        // anything will do
        if (prev2Hand.getCategory() == HandCategory::PASS)
            dealButton->setEnabled(hand.getCategory() != HandCategory::ILLEGAL);
        else
            dealButton->setEnabled(hand > prev2Hand);
        return;
    }
    else
        dealButton->setEnabled(hand > prevHand);
}

void Player::dealHand()
{
    if (sender() == passButton)
    {
        hand.setPass();
        ui->choiceLabel->show();
        if (prevPlayer->getLastHand().getCategory() == HandCategory::PASS)
            ui->lastDealView->hide();
    }
    else
    {
        auto dealt = hand.getRawHand();
        QVector<Card> newHand(m_cards.size() - dealt.size());
        std::set_difference(m_cards.begin(), m_cards.end(),
                            dealt.begin(), dealt.end(),
                            newHand.begin());
        m_cards = newHand;
        updateLastDeal(hand);
    }
    displayCards();
    ui->buttonBox->hide();
    ui->myCardsView->setEnabled(false);

    nextPlayer->informHand(hand);
    prevPlayer->informHand(hand);

    if (m_cards.size() == 0)
    {
        gameState = FINISHED;
        oppoReady = 0;
        lateComer = true;
        prevPlayer->informGameOver();
        nextPlayer->informGameOver();
        auto reply = QMessageBox::question(this, tr("You've won :)"), tr("Do you want to play another round?"));
        if (reply == QMessageBox::Yes)
            play();
        else
            emit quitGame();
    }
}

void Player::opponentDealt()
{
    Opponent *opponent = static_cast<Opponent*>(sender());
    const auto &latestHand = opponent->getLastHand();
    auto totalCardsDealt = latestHand.getRawHand().size();
    QLCDNumber *lcd;
    QLabel *choiceLabel;

    if (latestHand.getCategory() != HandCategory::PASS)
        updateLastDeal(latestHand);

    if (opponent == nextPlayer)
    {
        choiceLabel = ui->nextPlayerChoiceLabel;
        lcd = ui->n_cardsRemainingNumber;
        if (latestHand.getCategory() == HandCategory::PASS && hand.getCategory() == HandCategory::PASS)
            ui->lastDealView->hide();
    }
    else
    {
        choiceLabel = ui->prevPlayerChoiceLabel;
        lcd = ui->p_cardsRemainingNumber;
        play();
    }
    choiceLabel->setVisible(latestHand.getCategory() == HandCategory::PASS);
    lcd->display(lcd->intValue() - totalCardsDealt);
}

void Player::lostGame()
{
    gameState = FINISHED;
    oppoReady = 0;
    lateComer = false;
    auto reply = QMessageBox::question(this, tr("You've lost :("), tr("Do you want to play another round?"));
    if (reply == QMessageBox::Yes)
        play();
    else
        emit quitGame();
}

void Player::oneReady()
{
    qDebug() << "One ready";
    ++oppoReady;
    if (oppoReady == 3)
    {
        gameState = BIDDING;
        playBW();
//        if (firstBidder)
//        {
//            shuffleDistribute();
//        }
    }
}

void Player::setBidder(bool bidder)
{
    firstBidder = bidder;
    qDebug() << "Is first bidder:" << firstBidder;
}

void Player::play()
{
    switch(gameState)
    {
    case BIDDING:
    {
        ui->buttonBox->show();
        break;
    }
    case CONFIRMATION:
    {
        if (firstBidder)
        {
            confirmRoles();
        }
        break;
    }
    case PLAYING:
    {
        hand.clear();
        ui->buttonBox->show();
        ui->myCardsView->setEnabled(true);
        if (nextPlayer->getLastHand().getCategory() == HandCategory::PASS
                && prevPlayer->getLastHand().getCategory() == HandCategory::PASS)
        {
            passButton->setEnabled(false);
            ui->lastDealView->hide();
        }
        else
            passButton->setEnabled(true);
        dealButton->setEnabled(false);
        ui->choiceLabel->hide();
        break;
    }
    case FINISHED:
    {
        m_cards.clear();
        hide_cards.clear();
        needSnatchBid = false;
        hand.clear();
        firstBidder = false;
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

        ui->buttonBox->removeButton(dealButton);
        ui->buttonBox->removeButton(passButton);
        bidButton->setText(tr("Call"));
        ui->buttonBox->addButton(bidButton, QDialogButtonBox::YesRole);
        noBidButton->setText(tr("Don't call"));
        ui->buttonBox->addButton(noBidButton, QDialogButtonBox::NoRole);
        ui->buttonBox->hide();

        if (!firstBidder)
            connect(prevPlayer, &Opponent::rolesConfirmed, this, &Player::confirmRoles);
        oneReady();
        prevPlayer->nextRoundReady();
        nextPlayer->nextRoundReady();
        break;
    }
    default:
    {
        qDebug() << "Something went wrong!";
    }
    }
}

void Player::playBW()
{
    qDebug() << "Playing Black and white:" << blacks[0];
    nextPlayer->announceReady(blacks[0]);
    prevPlayer->announceReady(blacks[0]);
}

void Player::makeBid(const bool &bid)
{
    if (bid)
    {
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

    qDebug() << m_role << prevPlayer->getRole() << nextPlayer->getRole();
}

void Player::determineStart(const bool &black)
{
    blacks.append(black);
    qDebug() << "Bid received:" << black;
    if (blacks.size() == 3 && blacks[1] == blacks[2])
    {
        if (blacks[0] != blacks[1] || lateComer)
        {
            qDebug() << "Won black and white, distributing cards";
            firstBidder = true;
            shuffleDistribute();
        }
    }
}

void Player::shuffleDistribute()
{
    QVector<Card> deck;
    QVector<Card> a_deck;
    QVector<Card> b_deck;
    QVector<Card> c_deck;
    for (int i = 0; i < 54; ++i)
        deck.append(Card(i));
    std::shuffle(deck.begin(), deck.end(), std::default_random_engine(QRandomGenerator::system()->generate()));
    for (int i = 0; i < 17; ++i)
    {
        a_deck.append(deck.takeLast());
        b_deck.append(deck.takeLast());
        c_deck.append(deck.takeLast());
    }
    passDeck(a_deck, deck);
    qDebug() << "Giving out cards";
    nextPlayer->giveDeck(b_deck, deck);
    prevPlayer->giveDeck(c_deck, deck);
}
