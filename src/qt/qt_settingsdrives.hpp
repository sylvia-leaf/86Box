#ifndef QT_SETTINGSDRIVES_HPP
#define QT_SETTINGSDRIVES_HPP

#include <QList>
#include <QString>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTreeView;
class QTreeWidget;
class SettingsHarddisks;

/* Drives: every drive of the machine in one list, Add Drive to put one in,
   and the selected drive's settings beneath. It keeps nothing of its own:
   the list is read from the drive pages' lists, and the settings are the
   drive pages' own, which save as they always have. */
class SettingsDrives : public QWidget {
    Q_OBJECT

public:
    SettingsDrives(SettingsHarddisks *harddisks, QWidget *parent = nullptr);

    void refresh();
    void releaseAll();

    /* Name every drive's channel again, in the lists and the channel
       boxes: who has a channel follows the machine and the controllers
       chosen elsewhere, and the drive pages themselves are never shown. */
    static void refreshChannelNames();

protected:
    void showEvent(QShowEvent *event) override;

private:
    /* A kind of drive: its page's list, the settings beneath it, and the
       control that puts one in or takes it out (a bus, or a floppy's
       type), whose first entry is "none". Hard disks and the cassette go
       their own ways. */
    struct Kind {
        QString title;
        QString view;
        QString controls;
        QString setter;
    };

    SettingsHarddisks *harddisks;
    QTreeWidget       *list;
    QStackedWidget    *settings;
    QWidget           *empty;
    QLabel            *selectionTitle;
    QLabel            *missingHardware;
    QLabel            *connectionNote;
    QPushButton       *buttonAdd;
    QPushButton       *buttonRemove;
    QList<QWidget *>   shown;
    bool               building = false;
    bool               dirty = true;
    bool               refreshQueued = false;
    bool               renaming = false;
    QList<QTreeView *>  sourceViews;
    QList<QWidget *>    sourceControls;
    void refreshSoon();

    static const QList<Kind> &kinds();

    QTreeView *viewOf(int kind) const;
    QWidget   *controlsOf(int kind) const;
    QCheckBox *cassette() const;

    bool connectable(int kind, int row) const;
    void select(int kind, int row);
    void addFixed(int kind, int setterIndex);
    void remove(int kind, int row);
    void showAddList();
    void showImages();
};

#endif // QT_SETTINGSDRIVES_HPP
