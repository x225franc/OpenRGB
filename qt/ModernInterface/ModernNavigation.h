/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QPointer>
#include <QVector>
class QTabWidget;
class QWidget;
class QListWidget;
class QLineEdit;
class QEvent;
// Existing tabs own plugin pages; navigation uses guarded page identities.
class ModernNavigation : public QObject
{
    Q_OBJECT
public:
    ModernNavigation(QTabWidget* tabs, QWidget* parent, bool primary = false);
    QWidget* widget() const
    {
        return panel;
    }
    void refresh();
    void filter(const QString& text);
    void setCompact(bool value);
    QString currentLabel() const;
    int visibleCount() const;
signals:
    void changed();

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
    QPointer<QTabWidget> tabs;
    QWidget* panel;
    QListWidget* list;
    QLineEdit* search = nullptr;
    QVector<QPointer<QWidget>> pages;
    bool primary;
    bool queued = false;
    bool compact = false;
    QString query;
    void scheduleRefresh();
};
