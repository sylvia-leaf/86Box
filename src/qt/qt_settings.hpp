#ifndef QT_SETTINGS_HPP
#define QT_SETTINGS_HPP

#include <QDialog>
#include <QList>
#include <QModelIndex>

namespace Ui {
class Settings;
}

class SettingsMachine;
class SettingsDisplay;
class SettingsInput;
class SettingsSound;
class SettingsNetwork;
class SettingsPorts;
class SettingsStorageControllers;
class SettingsHarddisks;
class SettingsFloppyCDROM;
class SettingsOtherRemovable;
class SettingsOtherPeripherals;

class Settings : public QDialog {
    Q_OBJECT

public:
    enum { ReopenClassic = 2 };
    explicit Settings(QWidget *parent = nullptr);
    ~Settings();
    void save(int soft);
    bool usesSlotLayout() const { return slotLayout; }

    /* What the dialog holds now, or what is saved for a page not built. */
    int currentMachine() const;
    int currentHdc(int i) const;
    int currentSoundCard(int i) const;
    int currentScsiCard(int i) const;

    static Settings *settings;
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

protected slots:
    void accept() override;
    void reject() override;

private:
    /* The pages in the order of the list beside them. */
    enum {
        PAGE_MACHINE = 0,
        PAGE_DISPLAY,
        PAGE_INPUT,
        PAGE_SOUND,
        PAGE_NETWORK,
        PAGE_PORTS,
        PAGE_STORAGE,
        PAGE_HARDDISKS,
        PAGE_FLOPPYCDROM,
        PAGE_REMOVABLE,
        PAGE_OTHER,
        PAGE_COUNT
    };

    void placePage(int index, QWidget *page);
    void ensurePage(int index);
    void ensureAllPages();
    void setupSlotLayout();
    void switchToClassic();
    bool hasPendingEdits();
    void showListPage(const QModelIndex &current);
    void fitPageList();

    Ui::Settings               *ui;
    const bool                  slotLayout;
    void                       *configurationSnapshot = nullptr;
    SettingsMachine            *machine;
    SettingsDisplay            *display;
    SettingsInput              *input;
    SettingsSound              *sound;
    SettingsNetwork            *network;
    SettingsPorts              *ports;
    SettingsStorageControllers *storageControllers;
    SettingsHarddisks          *harddisks;
    SettingsFloppyCDROM        *floppyCdrom;
    SettingsOtherRemovable     *otherRemovable;
    SettingsOtherPeripherals   *otherPeripherals;

    /* The slot-based layout's own pages. */
    class SettingsOverview      *overview    = nullptr;
    QWidget                    *panelHolder = nullptr;

    friend class SettingsMachine;
    friend class SettingsDisplay;
    friend class SettingsInput;
    friend class SettingsSound;
    friend class SettingsNetwork;
    friend class SettingsPorts;
    friend class SettingsStorageControllers;
    friend class SettingsHarddisks;
    friend class SettingsFloppyCDROM;
    friend class SettingsOtherRemovable;
    friend class SettingsOtherPreipherals;
    friend class DeviceConfig;
    friend class DeviceConfigPanel;
};

#endif // QT_SETTINGS_HPP
