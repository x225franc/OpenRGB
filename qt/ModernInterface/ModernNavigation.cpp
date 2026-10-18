/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ModernNavigation.h"
#include "ModernInterface.h"
#include <QTabWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QTimer>
#include <QEvent>
#include <algorithm>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QPainter>
namespace
{
class NavigationDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(
        QPainter* painter, const QStyleOptionViewItem& original, const QModelIndex& index) const override
    {
        if(!original.widget || !original.widget->property("compact").toBool())
        {
            QStyledItemDelegate::paint(painter, original, index);
            return;
        }
        QStyleOptionViewItem option(original);
        initStyleOption(&option, index);
        const QIcon image = option.icon;
        option.text.clear();
        option.icon = QIcon();
        option.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
        original.widget->style()->drawControl(QStyle::CE_ItemViewItem, &option, painter, original.widget);
        const QRect iconRect(option.rect.center().x() - 10, option.rect.center().y() - 10, 20, 20);
        image.paint(painter, iconRect, Qt::AlignCenter,
            option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled);
    }
};
QString pageLabel(QTabWidget* tabs, int index)
{
    QString label = tabs->tabText(index).trimmed();
    if(label.isEmpty())
        if(QWidget* button = tabs->tabBar()->tabButton(index, QTabBar::LeftSide))
            label = button->toolTip().trimmed();
    if(label.isEmpty())
        label = tabs->widget(index)->windowTitle();
    return label;
}
int rank(const QString& label)
{
    const QString lower = label.toLower();
    if(lower == "devices")
        return 0;
    if(lower.contains("effects"))
        return 1;
    if(lower.contains("lightsync"))
        return 2;
    if(lower.contains("visual map"))
        return 3;
    if(lower.contains("sdk server"))
        return 4;
    if(lower.contains("sdk client"))
        return 5;
    if(lower.contains("settings"))
        return 90;
    if(lower.contains("information"))
        return 100;
    return 10;
}
}
ModernNavigation::ModernNavigation(QTabWidget* target, QWidget* parent, bool isPrimary)
    : QObject(parent)
    , tabs(target)
    , panel(new QWidget(parent))
    , primary(isPrimary)
{
    panel->setObjectName(primary ? "PrimaryNavigation" : "SecondaryNavigation");
    panel->setFixedWidth(primary ? 212 : 240);
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    if(!primary && target->objectName() == "DevicesTabBar")
    {
        search = new QLineEdit(panel);
        search->setObjectName("DeviceSearch");
        search->setPlaceholderText(tr("Find a device"));
        search->setClearButtonEnabled(true);
        search->setAccessibleName(tr("Search devices"));
        connect(search, &QLineEdit::textChanged, this, &ModernNavigation::filter);
        layout->addWidget(search);
    }
    list = new QListWidget(panel);
    list->setObjectName(primary ? "PrimaryList" : "SecondaryList");
    list->setFrameShape(QFrame::NoFrame);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setIconSize(QSize(20, 20));
    list->setWordWrap(!primary);
    list->setSpacing(primary ? 3 : 5);
    if(primary)
        list->setItemDelegate(new NavigationDelegate(list));
    list->setAccessibleName(primary ? tr("Main navigation") : tr("Page navigation"));
    layout->addWidget(list, 1);
    tabs->tabBar()->hide();
    tabs->tabBar()->installEventFilter(this);
    tabs->installEventFilter(this);
    target->findChild<QStackedWidget*>()->installEventFilter(this);
    connect(list, &QListWidget::currentRowChanged, this,
        [this](int row)
        {
            if(tabs && row >= 0 && row < pages.size() && pages[row])
                tabs->setCurrentWidget(pages[row]);
        });
    connect(tabs, &QTabWidget::currentChanged, this, [this] { refresh(); });
    connect(tabs->tabBar(), &QTabBar::tabMoved, this, [this] { scheduleRefresh(); });
    refresh();
}
void ModernNavigation::scheduleRefresh()
{
    if(queued)
        return;
    queued = true;
    QTimer::singleShot(0, this,
        [this]
        {
            queued = false;
            refresh();
        });
}
bool ModernNavigation::eventFilter(QObject*, QEvent* event)
{
    switch(event->type())
    {
    case QEvent::ChildAdded:
    case QEvent::ChildRemoved:
    case QEvent::LayoutRequest:
    case QEvent::LanguageChange:
    case QEvent::ToolTipChange:
    case QEvent::Show:
        scheduleRefresh();
        break;
    default:
        break;
    }
    return false;
}
void ModernNavigation::refresh()
{
    if(!tabs)
        return;
    tabs->tabBar()->hide();
    struct Entry
    {
        QPointer<QWidget> page;
        QString label;
        bool enabled;
        bool visible;
    };
    QVector<Entry> entries;
    for(int i = 0; i < tabs->count(); ++i)
    {
        QString label = pageLabel(tabs, i);
        entries.push_back({ tabs->widget(i), label, tabs->isTabEnabled(i), tabs->isTabVisible(i) });
        if(QWidget* button = tabs->tabBar()->tabButton(i, QTabBar::LeftSide))
            button->installEventFilter(this);
    }
    if(primary)
        std::stable_sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return rank(a.label) < rank(b.label); });
    bool same = entries.size() == pages.size();
    for(int i = 0; same && i < entries.size(); ++i)
    {
        auto item = list->item(i);
        same = pages[i] == entries[i].page && item->text() == entries[i].label
            && bool(item->flags() & Qt::ItemIsEnabled) == entries[i].enabled
            && item->data(Qt::UserRole).toBool() == entries[i].visible;
    }
    QSignalBlocker blocker(list);
    if(!same)
    {
        list->clear();
        pages.clear();
        for(const auto& entry : entries)
        {
            auto item = new QListWidgetItem(ModernInterface::icon(entry.label), entry.label, list);
            item->setToolTip(entry.label);
            item->setSizeHint(QSize(primary ? (compact ? 52 : 200) : 220, primary ? 44 : 68));
            if(!entry.enabled)
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setData(Qt::UserRole, entry.visible);
            pages.push_back(entry.page);
        }
    }
    int selected = -1;
    for(int i = 0; i < pages.size(); ++i)
    {
        auto item = list->item(i);
        item->setHidden(!entries[i].visible || !item->text().contains(query, Qt::CaseInsensitive));
        if(pages[i] == tabs->currentWidget())
            selected = i;
    }
    list->setCurrentRow(selected);
    emit changed();
}
void ModernNavigation::filter(const QString& text)
{
    query = text.trimmed();
    refresh();
}
QString ModernNavigation::currentLabel() const
{
    return tabs && tabs->currentIndex() >= 0 ? pageLabel(tabs, tabs->currentIndex()) : QString();
}
int ModernNavigation::visibleCount() const
{
    int count = 0;
    for(int i = 0; i < list->count(); ++i)
        if(list->item(i)->data(Qt::UserRole).toBool())
            ++count;
    return count;
}

void ModernNavigation::setCompact(bool value)
{
    if(!primary || compact == value)
        return;
    compact = value;
    panel->setFixedWidth(compact ? 52 : 212);
    list->setProperty("compact", compact);
    for(int i = 0; i < list->count(); ++i)
        list->item(i)->setSizeHint(QSize(compact ? 52 : 200, 44));
    list->style()->unpolish(list);
    list->style()->polish(list);
    list->viewport()->update();
}
