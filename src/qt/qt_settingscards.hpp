#ifndef QT_SETTINGSCARDS_HPP
#define QT_SETTINGSCARDS_HPP

#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QMenu;
class QPushButton;
class QVBoxLayout;
class DeviceConfigPanel;
struct _device_;
struct machine_slot_t;

/* The cards the settings hold. Nothing is kept here: the cards are read
   from, and set through, the card controls of the classic pages (a combo
   box per card the machine can have, or a check box for a card that is
   there or not), which save as they always have. */
namespace Cards {

enum Category {
    CATEGORY_VIDEO = 0,
    CATEGORY_SOUND,
    CATEGORY_NETWORK,
    CATEGORY_HDC,
    CATEGORY_SCSI,
    CATEGORY_FDC,
    CATEGORY_CDROM_INTERFACE,
    CATEGORY_MEMORY,
    CATEGORY_ROM,
    CATEGORY_RTC,
    CATEGORY_GAMEPORT,
    CATEGORY_PORTS,
    CATEGORY_EXPANSION,
    CATEGORY_OTHER,
    CATEGORY_COUNT
};

struct Card {
    int             category;
    QString         control;  /* the combo box or check box holding it */
    bool            check;
    int             index;    /* which of its category's controls */
    QString         name;
    QString         bus;      /* ISA, EISA, PCI...; empty on the board */
    bool            onboard;
    const _device_ *dev;      /* what its options are read from; may be null */
    int             instance;
};

/* A card that could go in: the control it would be set in, and the value
   (a combo box's entry; a check box is checked). */
struct Choice {
    int             category;
    QString         control;
    bool            check;
    int             id;
    QString         name;
    const _device_ *dev;
    bool            onboard = false; /* the board's own device of the kind */
};

void beginSession();
int placement(const _device_ *dev, int instance);
void assign(const Card &card, int slot);
bool slotsChanged();
void saveSlots();
QString slotError();
QString     categoryTitle(int category);
/* The "internal" entry of each card control named for the chip it is:
   [Onboard] and the chip. */
void        nameOnboardEntries();
QList<Card> list(int category = -1);

/* A floppy controller the settings have: its name, the first of its four
   places in the drive list (the second controller's are 4 to 7), and how
   many drives it has connectors for. */
struct FloppyController {
    QString name;
    int     first;
    int     drives;
};
QList<FloppyController> floppyControllers();
/* The controller a floppy drive of the list is on, or -1 where the drive
   has none; the drive's number on it in *number. */
int floppyControllerOf(const QList<FloppyController> &controllers, int drive, int *number = nullptr);
QWidget    *control(const QString &name);
QString     busOf(const _device_ *dev);
QString     chipsetName(int mach);
bool        fits(const QString &slot, const QString &card);

/* The cards of a category (any, with -1) that fit a bus (any the machine
   has, when bus is empty) and have a free control to go in; with no bus
   given, the board's own devices not in use as well. */
QList<Choice> choices(int category, const QString &bus, const Card *replacing = nullptr);
/* The board's own device of a category, when the board has one and it is
   not in use, as a choice; false otherwise. Where every control that can
   hold it holds a card, the card it would take the place of is put in
   *displaced (when given; without it, there is no choice). */
bool onboardChoice(int category, Choice *choice, Card *displaced = nullptr);
/* Why a card cannot go in (in place of another, when given), or empty
   when there is a slot for it. */
QString fitError(const _device_ *adding, const Card *without = nullptr);
/* Put a card in, replacing one (when given); false, having said why, when
   it does not fit. */
bool put(const Choice &choice, const Card *replacing, QWidget *parent, int slot = -1);
/* Add > card; false when the category has no free control. */
bool fillAddMenu(QMenu *menu, int category, const QString &bus, QWidget *parent);
/* The cards that could go in, a category's (all, with -1, grouped by
   kind), in a scrolling list under a filter box, below the anchor. */
void pickCard(int category, const QString &bus, QWidget *anchor);

/* An entry of such a list: the groups it is under, its name, and what
   choosing it does. */
struct PickItem {
    QStringList           groups;
    QString               name;
    std::function<void()> choose;
};
void pickFrom(const QList<PickItem> &items, QWidget *anchor, bool dropdown = false);
void remove(const Card &card);
bool removable(const Card &card);
void configure(const Card &card);
/* The input devices on a serial port (COM n, from 1), which choose their
   port in their own options: a serial mouse, a tablet, a touchscreen. */
QStringList portUsers(int com);
bool configurable(const Card &card);

/* One options panel per card, shared by the pages that show it, kept
   until the card goes. */
DeviceConfigPanel *panel(const Card &card);
void               setPanelHolder(QWidget *holder);
void               releasePanel(DeviceConfigPanel *panel);
void               prunePanels();

/* The board's slots in the order 86Box fills them, with the cards in
   them: what the Overview draws and what the card pages say a card is in. */
struct Node {
    enum Kind { BUS, BRIDGE, DEVICE, SLOT };
    Kind    kind;
    int     parent;
    QString label;
    QString bus;     /* a slot's bus */
    int     pciType; /* an on-board PCI device's type */
    int     card;    /* index into the list, or -1 */
    int     slot = -1; /* stable board slot identity, never a display index */
    int     bridge = 0; /* a PCI-PCI bridge put in by hand: its number, from 1 */
};
QList<Node> layout(const QList<Card> &cards);

/* A view to be told when the layout changes with no card control
   changing, as when a bridge goes in or out, for as long as it lives. */
void follow(QWidget *view, const std::function<void()> &changed);

/* PCI-PCI bridges put in by hand, each in a board slot of the machine's
   own PCI buses, nine slots behind it: whether one can go in a slot (an
   empty one, unless a card there may move behind it), put one there, and
   take the nth (from 1) out, the cards behind it going where 86Box puts
   them. bridgeSlot() is where 86Box would put one: the last empty slot,
   else the last, whose card moves behind it. */
bool canAddBridge(int slot, bool occupied = false);
int  bridgeSlot();
void addBridge(int slot);
void removeBridge(int n);
QString     location(const QList<Node> &nodes, int card);
}

/* The Cards page of Basic: the cards, to add and remove. */
class SettingsCards : public QWidget {
    Q_OBJECT

public:
    explicit SettingsCards(QWidget *parent = nullptr);

    void refresh();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QTreeWidget *tree;
    QPushButton *buttonAdd;
    QPushButton *buttonRemove;
    bool removing = false;

    void addCardRow(QTreeWidgetItem *item, const Cards::Card &card, const QString &location);
};

/* A page of one kind of card (video, sound, network): each card with its
   options beneath it, then Add New, then the page's own settings. */
class SettingsCardPage : public QWidget {
    Q_OBJECT

public:
    SettingsCardPage(int category, QWidget *classic, QWidget *parent = nullptr);

    void refresh();
    void releaseAll();

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct Block {
        QString                     key;
        QWidget                    *box;
        QPointer<DeviceConfigPanel> panel;
        QPointer<QWidget>           extra;
    };

    int          category;
    QWidget     *classic;
    QVBoxLayout *blocks;
    QPushButton *buttonAdd;
    QList<Block> shown;
    QLabel      *missingHardware;

    void     dropBlock(const Block &block);
    Block    makeBlock(const Cards::Card &card, const QString &key, const QString &where);
    QWidget *extraFor(const Cards::Card &card) const;
};

/* Keep a widget of the classic pages somewhere else for a while, and put
   it back where it was. */
namespace Borrow {
void take(QWidget *widget, QWidget *into);
void giveBack(QWidget *widget);
void giveBackAll();
void hideRow(QWidget *widget);
}

#endif // QT_SETTINGSCARDS_HPP
