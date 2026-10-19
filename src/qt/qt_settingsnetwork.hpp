#ifndef QT_SETTINGSNETWORK_HPP
#define QT_SETTINGSNETWORK_HPP

#include <QWidget>
#include <QHash>
#include <QPointer>

namespace Ui {
class SettingsNetwork;
}

class SettingsNetwork : public QWidget {
    Q_OBJECT

public:
    explicit SettingsNetwork(QWidget *parent = nullptr);
    ~SettingsNetwork();

    int  changed();
    /* The card chosen in row i of the page, as the settings stand now. */
    int netCard(int i) const;

    void restore();
    void save(int soft);

public slots:
    void onCurrentMachineChanged(int machineId);

private slots:
    void on_pushButtonConf1_clicked();
    void on_pushButtonConf2_clicked();
    void on_pushButtonConf3_clicked();
    void on_pushButtonConf4_clicked();
    void on_comboIndexChanged(int index);

    void enableElements(Ui::SettingsNetwork *ui);

private:
    /* The page's controls by name, kept from when it is built: a control
       moved under another widget is still found, as findChild() would not. */
    QHash<QString, QPointer<QWidget>> controls;
    template <typename T>
    T networkControl(const QString &name) const
    {
        return qobject_cast<T>(controls.value(name).data());
    }
    Ui::SettingsNetwork *ui;
    int                  machineId = 0;

    int                  net_card_cfg_changed[4] = { 0, 0, 0, 0 };

    SettingsCompleter   *sc[4];
    SettingsCompleter   *scDevice[4];
};

#endif // QT_SETTINGSNETWORK_HPP
