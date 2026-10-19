/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          The Drives page of the slot-based Settings layout (preview).
 */
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QSplitter>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QStyle>
#include <QTreeView>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include "qt_util.hpp"
#include "qt_harddrive_common.hpp"
extern "C" {
#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/cdrom.h>
#include <86box/fdd.h>
#include <86box/scsi_device.h>
#include <86box/mo.h>
#include <86box/rdisk.h>
#include <86box/scsi_tape.h>
#include <86box/device.h>
#include <86box/hdc_ide.h>
}

#include "qt_settings.hpp"
#include "qt_settingscards.hpp"
#include "qt_settingsdrives.hpp"
#include "qt_harddiskdialog.hpp"
#include "qt_settingsharddisks.hpp"
#include "qt_settingsfloppycdrom.hpp"
#include "qt_settingsotherremovable.hpp"
#include "qt_settingsoverview.hpp"

enum {
    KIND_HARDDISK = 0,
    KIND_FLOPPY,
    KIND_CDROM,
    KIND_MO,
    KIND_RDISK,
    KIND_TAPE,
    KIND_CASSETTE
};

const QList<SettingsDrives::Kind> &
SettingsDrives::kinds()
{
    static const QList<Kind> list = {
        { QT_TRANSLATE_NOOP("SettingsDrives", "Hard disk"), "treeView", "hdControls", QString() },
        { QT_TRANSLATE_NOOP("SettingsDrives", "Floppy drive"), "treeViewFloppy", "floppyControls", "comboBoxFloppyType" },
        { QT_TRANSLATE_NOOP("SettingsDrives", "CD-ROM drive"), "treeViewCDROM", "cdControls", "comboBoxBus" },
        { QT_TRANSLATE_NOOP("SettingsDrives", "MO drive"), "treeViewMO", "moControls", "comboBoxMOBus" },
        { QT_TRANSLATE_NOOP("SettingsDrives", "Removable disk drive"), "treeViewRDisk", "rdiskControls", "comboBoxRDiskBus" },
        { QT_TRANSLATE_NOOP("SettingsDrives", "Tape drive"), "treeViewTape", "tapeControls", "comboBoxTapeBus" },
    };

    return list;
}

SettingsDrives::SettingsDrives(SettingsHarddisks *harddisks, QWidget *parent)
    : QWidget(parent)
    , harddisks(harddisks)
{
    for (int k = 0; k < kinds().size(); ++k) {
        sourceViews.append((k == KIND_HARDDISK ? static_cast<QWidget *>(harddisks) : Settings::settings)
                               ->findChild<QTreeView *>(kinds()[k].view));
        sourceControls.append(Settings::settings->findChild<QWidget *>(kinds()[k].controls));
    }
    auto *layout = new QVBoxLayout(this);
    auto *top    = new QHBoxLayout();

    buttonAdd    = new QPushButton(tr("Add Drive..."), this);
    buttonRemove = new QPushButton(tr("Remove"), this);
    auto *images = new QPushButton(tr("Disk Images..."), this);
    top->addWidget(buttonAdd);

    top->addStretch();
    top->addWidget(images);
    layout->addLayout(top);
    auto *folder = new QLabel(tr("New images: %1").arg(QDir::toNativeSeparators(util::machineDirectory())), this);
    folder->setTextFormat(Qt::PlainText);
    folder->setTextInteractionFlags(Qt::TextSelectableByMouse);
    folder->setWordWrap(true);
    folder->setToolTip(tr("The default folder for new disk images is the folder containing this machine's configuration file."));
    layout->addWidget(folder);
    missingHardware = new QLabel(this);
    missingHardware->setWordWrap(true);
    missingHardware->setStyleSheet(QStringLiteral("color: #b00020; font-weight: bold; padding: 8px 2px;"));
    layout->addWidget(missingHardware);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    layout->addWidget(splitter, 1);

    list = new QTreeWidget(this);
    list->setColumnCount(4);
    list->setHeaderLabels({ tr("Drive"), tr("Connected to"), tr("Image"), tr("Details") });
    list->setRootIsDecorated(false);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setAlternatingRowColors(true);
    list->setMinimumHeight(130);
    list->setUniformRowHeights(true);
    list->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    list->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    list->setColumnWidth(0, 145);
    list->setColumnWidth(1, 195);
    list->setColumnWidth(2, 165);
    splitter->addWidget(list);
    auto *details = new QWidget(splitter);
    auto *detailLayout = new QVBoxLayout(details);
    detailLayout->setContentsMargins(6, 10, 6, 6);
    selectionTitle = new QLabel(details);
    QFont heading = selectionTitle->font();
    heading.setBold(true);
    selectionTitle->setFont(heading);
    detailLayout->addWidget(selectionTitle);
    connectionNote = new QLabel(details);
    connectionNote->setWordWrap(true);
    detailLayout->addWidget(connectionNote);

    /* The selected drive's settings: its page's own, beneath the list. */
    settings = new QStackedWidget(this);
    empty    = new QLabel(tr("Add or select a drive to configure its connection and image."), settings);
    settings->addWidget(empty);
    for (int k = 0; k < kinds().size(); k++) {
        if (auto *controls = controlsOf(k)) {
            Borrow::take(controls, settings);
            settings->addWidget(controls);
        }
    }
    settings->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    detailLayout->addWidget(settings, 0);
    detailLayout->addStretch();
    splitter->addWidget(details);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 270, 240 });
    auto *bottom = new QHBoxLayout();
    buttonRemove->setToolTip(tr("Remove the selected drive from this machine. Its image file is kept."));
    bottom->addWidget(buttonRemove);
    bottom->addStretch();
    layout->addLayout(bottom);

    /* The hard disk page's own New, Existing and Remove are Add Drive and
       Remove here. */
    for (const char *name : { "pushButtonNew", "pushButtonExisting", "pushButtonRemove" }) {
        if (auto *button = harddisks->findChild<QPushButton *>(name))
            button->hide();
    }

    connect(buttonAdd, &QPushButton::clicked, this, [this]() { showAddList(); });
    connect(buttonRemove, &QPushButton::clicked, this, [this]() {
        auto *item = list->currentItem();
        if (item != nullptr)
            remove(item->data(0, Qt::UserRole).toInt(), item->data(0, Qt::UserRole + 1).toInt());
    });
    connect(images, &QPushButton::clicked, this, [this]() { showImages(); });
    connect(list, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item, QTreeWidgetItem *) {
        if (building)
            return;
        if (item == nullptr) {
            settings->setCurrentWidget(empty);
            settings->setMaximumHeight(empty->sizeHint().height());
            buttonRemove->setEnabled(false);
            return;
        }
        select(item->data(0, Qt::UserRole).toInt(), item->data(0, Qt::UserRole + 1).toInt());
    });

    /* The list follows the drive pages' lists. */
    for (int k = 0; k < kinds().size(); k++) {
        if (auto *view = viewOf(k)) {
            auto *model = view->model();
            connect(model, &QAbstractItemModel::rowsInserted, this, [this]() { refreshSoon(); });
            connect(model, &QAbstractItemModel::rowsRemoved, this, [this]() { refreshSoon(); });
            connect(model, &QAbstractItemModel::dataChanged, this, [this]() { refreshSoon(); });
        }
    }
    if (auto *box = cassette())
        connect(box, &QCheckBox::toggled, this, [this]() { refreshSoon(); });

    refresh();
}

void
SettingsDrives::showEvent(QShowEvent *event)
{
    renaming = true;
    refreshChannelNames();
    renaming = false;
    refresh();
    QWidget::showEvent(event);
}

void
SettingsDrives::refreshChannelNames()
{
    if (Settings::settings == nullptr)
        return;
    if (auto *page = Settings::settings->findChild<SettingsHarddisks *>())
        page->refreshChannelNames();
    if (auto *page = Settings::settings->findChild<SettingsFloppyCDROM *>())
        page->refreshChannelNames();
    if (auto *page = Settings::settings->findChild<SettingsOtherRemovable *>())
        page->refreshChannelNames();
}

QTreeView *
SettingsDrives::viewOf(int kind) const
{
    return sourceViews.value(kind);
}

QWidget *
SettingsDrives::controlsOf(int kind) const
{
    return sourceControls.value(kind);
}

QCheckBox *
SettingsDrives::cassette() const
{
    return Settings::settings->findChild<QCheckBox *>("checkBoxCassette");
}

/* Every drive there is: hard disks, then the floppy drives, CD-ROM, MO,
   removable disk and tape drives that are on, then the cassette. */
void
SettingsDrives::refreshSoon()
{
    dirty = true;
    if (renaming || refreshQueued || !isVisible())
        return;
    refreshQueued = true;
    QTimer::singleShot(0, this, [this]() {
        refreshQueued = false;
        if (dirty && isVisible())
            refresh();
    });
}

void
SettingsDrives::refresh()
{
    if (building)
        return;
    building = true;
    dirty = false;
    list->setUpdatesEnabled(false);

    int keepKind = -1;
    int keepRow  = -1;
    if (auto *item = list->currentItem()) {
        keepKind = item->data(0, Qt::UserRole).toInt();
        keepRow  = item->data(0, Qt::UserRole + 1).toInt();
    }
    const int scroll = list->verticalScrollBar() ? list->verticalScrollBar()->value() : 0;

    QSignalBlocker blocked(list);
    list->clear();
    QTreeWidgetItem *keep = nullptr;
    const auto       floppies = Cards::floppyControllers();

    for (int k = 0; k < kinds().size(); k++) {
        auto *view = viewOf(k);
        if (view == nullptr)
            continue;
        auto *model = view->model();
        for (int r = 0; r < model->rowCount(); r++) {
            const int state = model->index(r, 0).data(Qt::UserRole).toInt();
            if ((k != KIND_HARDDISK) && (state == 0))
                continue;

            QString imagePath;
            QString name;
            QString where;
            QStringList details;
            if (k == KIND_HARDDISK) {
                name  = tr("Hard disk %1").arg(r + 1);
                where = model->index(r, 0).data().toString();
                imagePath = model->index(r, 1).data().toString();
                for (int c = 2; c < model->columnCount(); c++)
                    details.append(model->index(r, c).data().toString());
            } else if (k == KIND_FLOPPY) {
                /* Numbered on its controller, not lettered: which letter
                   a drive gets is the operating system's business. */
                int       number     = 0;
                const int controller = Cards::floppyControllerOf(floppies, r, &number);
                name = tr("Floppy drive %1").arg((controller >= 0) ? number : ((r & 3) + 1));
                details.append(model->index(r, 0).data().toString());
                for (int c = 1; c < model->columnCount(); c++)
                    details.append(QString("%1: %2").arg(model->headerData(c, Qt::Horizontal).toString(), model->index(r, c).data().toString()));
                where = (controller >= 0) ? floppies[controller].name : QObject::tr("(none)");
            } else {
                name  = tr("%1 %2").arg(tr(kinds()[k].title.toUtf8().constData())).arg(r + 1);
                where = model->index(r, 0).data().toString();
                for (int c = 1; c < model->columnCount(); c++)
                    details.append(model->index(r, c).data().toString());
            }
            if (k == KIND_FLOPPY)
                imagePath = QString::fromUtf8(drives[r].image_path);
            else if (k == KIND_CDROM)
                imagePath = QString::fromUtf8(cdrom[r].image_path);
            else if (k == KIND_MO)
                imagePath = QString::fromUtf8(mo_drives[r].image_path);
            else if (k == KIND_RDISK)
                imagePath = QString::fromUtf8(rdisk_drives[r].image_path);
            else if (k == KIND_TAPE)
                imagePath = QString::fromUtf8(tape_drives[r].image_path);
            details.removeAll(QString());

            auto *item = new QTreeWidgetItem(list, { name, where, imagePath.isEmpty() ? QString::fromUtf8("—") : QFileInfo(imagePath).fileName(), details.join(QStringLiteral("  ·  ")) });
            item->setToolTip(0, name);
            item->setToolTip(1, where);
            item->setToolTip(2, imagePath);
            item->setToolTip(3, details.join(QStringLiteral("\n")));
            item->setIcon(0, model->index(r, 0).data(Qt::DecorationRole).value<QIcon>());
            /* What the channel names say of a channel no controller has
               (Harddrives::ownerName), in whatever language. */
            const bool unconnected = where.contains(QObject::tr("(none)"));
            item->setData(0, Qt::UserRole + 2, unconnected);
            if (unconnected) {
                item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
                item->setToolTip(1, (k == KIND_FLOPPY) ? tr("No floppy controller has a connector for this drive. Remove it, or choose a controller that takes more drives.")
                                                       : tr("No controller provides this channel. Choose a channel on an installed controller below."));
            }
            item->setData(0, Qt::UserRole, k);
            item->setData(0, Qt::UserRole + 1, r);
            if ((k == keepKind) && (r == keepRow))
                keep = item;
        }
    }

    if (auto *box = cassette(); (box != nullptr) && box->isEnabled() && box->isChecked()) {
        auto *item = new QTreeWidgetItem(list, { tr("Cassette"), tr("Cassette port"), QString() });
        item->setData(0, Qt::UserRole, KIND_CASSETTE);
        item->setData(0, Qt::UserRole + 1, 0);
        if (keepKind == KIND_CASSETTE)
            keep = item;
    }

    /* The drive and its connection whole where they fit: the two columns
       grow to their text, up to a limit, and never shrink below what they
       are, so a width the user set by hand stays. */
    for (int c = 0; c < 2; c++) {
        const int before = list->columnWidth(c);
        list->resizeColumnToContents(c);
        list->setColumnWidth(c, qMax(before, qMin(list->columnWidth(c) + 12, 300)));
    }

    bool diskController = false;
    bool anyController = false;
    for (const auto &card : Cards::list()) {
        diskController |= card.category == Cards::CATEGORY_HDC || card.category == Cards::CATEGORY_SCSI;
        anyController |= diskController || card.category == Cards::CATEGORY_FDC || card.category == Cards::CATEGORY_CDROM_INTERFACE;
    }
    bus_owner_t ideOwners[IDE_BUS_MAX];
    const int channels = Harddrives::idePlan(ideOwners);
    for (int i = 0; i < channels; ++i)
        diskController |= ideOwners[i].onboard || ideOwners[i].device != nullptr;
    QStringList missing;
    if (!diskController)
        missing.append(anyController ? tr("No hard disk controller installed.") : tr("No drive controller installed."));
    if (!list->topLevelItemCount())
        missing.append(tr("No drives installed. Use Add Drive to install one."));
    missingHardware->setText(missing.join(QLatin1Char(' ')));
    missingHardware->setVisible(!missing.isEmpty());
    if (!keep && list->topLevelItemCount())
        keep = list->topLevelItem(0);
    list->verticalScrollBar()->setValue(scroll);
    building = false;
    list->setUpdatesEnabled(true);

    if (keep != nullptr) {
        list->setCurrentItem(keep);
        select(keep->data(0, Qt::UserRole).toInt(), keep->data(0, Qt::UserRole + 1).toInt());
    } else {
        settings->setCurrentWidget(empty);
        settings->setMaximumHeight(empty->sizeHint().height());
        buttonRemove->setEnabled(false);
        selectionTitle->setText(tr("No drives installed"));
        connectionNote->hide();
    }
}

/* A drive chosen: chosen on its page too, so the settings beneath are its. */
void
SettingsDrives::select(int kind, int row)
{
    buttonRemove->setEnabled(true);
    if (auto *item = list->currentItem()) {
        selectionTitle->setText(tr("%1 settings").arg(item->text(0)));
        const bool missing = item->data(0, Qt::UserRole + 2).toBool();
        connectionNote->setText(item->text(1) == tr("(Disconnected)") ? tr("This drive is kept in the configuration but is not connected to a controller port.") : missing ? tr("No controller is connected to this channel. Select an available connection below.")
                                       : item->toolTip(2));
        connectionNote->setVisible(!connectionNote->text().isEmpty());
    }

    if (kind == KIND_CASSETTE) {
        settings->setCurrentWidget(empty);
        return;
    }

    if (auto *view = viewOf(kind))
        view->setCurrentIndex(view->model()->index(row, 0));
    if (auto *controls = controlsOf(kind); (controls != nullptr) && (settings->indexOf(controls) >= 0))
        settings->setCurrentWidget(controls);
    else
        settings->setCurrentWidget(empty);
    /* As tall as the drive's settings, not stretched to fill. */
    settings->setMaximumHeight(settings->currentWidget()->sizeHint().height());
}

/* Whether a drive of the list can be put in: a floppy drive only where a
   floppy controller has a connector for it. */
bool
SettingsDrives::connectable(int kind, int row) const
{
    return (kind != KIND_FLOPPY) || (Cards::floppyControllerOf(Cards::floppyControllers(), row) >= 0);
}

/* A floppy, CD-ROM, MO, removable disk or tape drive put in: the first of
   that kind not in use, set as its page sets one. */
void
SettingsDrives::addFixed(int kind, int setterIndex)
{
    auto *view     = viewOf(kind);
    auto *controls = controlsOf(kind);
    auto *setter   = (controls != nullptr) ? controls->findChild<QComboBox *>(kinds()[kind].setter) : nullptr;
    if ((view == nullptr) || (setter == nullptr))
        return;

    auto *model = view->model();
    int   row   = -1;
    for (int r = 0; (r < model->rowCount()) && (row < 0); r++) {
        if ((model->index(r, 0).data(Qt::UserRole).toInt() == 0) && connectable(kind, r))
            row = r;
    }
    if (row < 0) {
        QMessageBox::information(this, tr("Add Drive"),
                                 (kind == KIND_FLOPPY) ? tr("Every connector of the floppy controller has a drive on it.")
                                                       : tr("Every %1 there can be is in use.").arg(tr(kinds()[kind].title.toUtf8().constData()).toLower()));
        return;
    }

    view->setCurrentIndex(model->index(row, 0));
    setter->setCurrentIndex(setterIndex);
    emit setter->activated(setterIndex);

    refresh();
    for (int i = 0; i < list->topLevelItemCount(); i++) {
        auto *item = list->topLevelItem(i);
        if ((item->data(0, Qt::UserRole).toInt() == kind) && (item->data(0, Qt::UserRole + 1).toInt() == row))
            list->setCurrentItem(item);
    }
}

void
SettingsDrives::remove(int kind, int row)
{
    if (kind == KIND_CASSETTE) {
        if (auto *box = cassette())
            box->setChecked(false);
        return;
    }

    auto *view = viewOf(kind);
    if (view == nullptr)
        return;
    view->setCurrentIndex(view->model()->index(row, 0));

    if (kind == KIND_HARDDISK) {
        if (auto *button = harddisks->findChild<QPushButton *>("pushButtonRemove"))
            button->click();
        return;
    }

    auto *controls = controlsOf(kind);
    auto *setter   = (controls != nullptr) ? controls->findChild<QComboBox *>(kinds()[kind].setter) : nullptr;
    if (setter == nullptr)
        return;
    const int none = (kind == KIND_FLOPPY) ? 0 : qMax(setter->findData(0), 0);
    setter->setCurrentIndex(none);
    emit setter->activated(none);
}

/* Add Drive: the kinds, and under each what it can be, in the list Add
   card uses. */
void
SettingsDrives::showAddList()
{
    QList<Cards::PickItem> items;
    const QString          hardDisk = tr("Hard disk");

    auto click = [this](const char *name) {
        if (auto *button = harddisks->findChild<QPushButton *>(name))
            button->click();
        refresh();
        if (auto *view = viewOf(KIND_HARDDISK)) {
            const int row = view->currentIndex().row();
            for (int i = 0; i < list->topLevelItemCount(); ++i) {
                auto *item = list->topLevelItem(i);
                if (item->data(0, Qt::UserRole).toInt() == KIND_HARDDISK &&
                    item->data(0, Qt::UserRole + 1).toInt() == row)
                    list->setCurrentItem(item);
            }
        }
    };
    items.append({ { hardDisk }, tr("New hard disk..."), [click]() { click("pushButtonNew"); } });
    items.append({ { hardDisk }, tr("Existing hard disk image..."), [click]() { click("pushButtonExisting"); } });

    for (int k = KIND_FLOPPY; k < kinds().size(); k++) {
        auto *view     = viewOf(k);
        auto *controls = controlsOf(k);
        auto *setter   = (controls != nullptr) ? controls->findChild<QComboBox *>(kinds()[k].setter) : nullptr;
        if ((view == nullptr) || (setter == nullptr))
            continue;

        bool free = false;
        for (int r = 0; r < view->model()->rowCount(); r++)
            free |= (view->model()->index(r, 0).data(Qt::UserRole).toInt() == 0) && connectable(k, r);
        if (!free)
            continue;

        const QString title = tr(kinds()[k].title.toUtf8().constData());
        for (int i = 0; i < setter->count(); i++) {
            if ((k == KIND_FLOPPY) ? (i == 0) : (setter->itemData(i).toInt() == 0))
                continue;
            items.append({ { title }, setter->itemText(i), [this, k, i]() { addFixed(k, i); } });
        }
    }

    if (auto *box = cassette(); (box != nullptr) && box->isEnabled() && !box->isChecked())
        items.append({ {}, tr("Cassette"), [box]() { box->setChecked(true); } });

    Cards::pickFrom(items, buttonAdd, true);
}

void
SettingsDrives::showImages()
{
    QDialog dialog(this);
    auto   *layout  = new QVBoxLayout(&dialog);
    auto   *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);

    dialog.setWindowTitle(tr("Disk Images"));
    layout->addWidget(new SettingsDriveImages(harddisks, &dialog));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.resize(760, 480);
    dialog.exec();
}

/* Nothing is kept here to give back: the settings beneath the list are
   borrowed for as long as Settings is open. */
void
SettingsDrives::releaseAll()
{
}
