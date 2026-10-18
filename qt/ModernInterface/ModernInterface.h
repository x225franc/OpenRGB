/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QPointer>
#include <QIcon>
#include <QColor>
class QApplication;
class QMainWindow;
class QTabWidget;
class QWidget;
class QListWidget;
class QLineEdit;
class QLabel;
class QEvent;
class QPushButton;
#include "ModernNavigation.h"
class ModernInterface : public QObject
{
    Q_OBJECT
public:
    static void applyTheme(QApplication* app);
    static QIcon icon(const QString& name, const QColor& color = QColor("#dedee7"));
    ModernInterface(QMainWindow* window, QTabWidget* mainTabs, QTabWidget* devices, QTabWidget* settings,
        QTabWidget* information, QWidget* commands);
    void refresh();

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
    QMainWindow* window;
    QTabWidget* mainTabs;
    QTabWidget* devices;
    ModernNavigation* primary;
    ModernNavigation* deviceNavigation;
    QLabel* title;
    QLabel* description;
    QLabel* deviceCount;
    QWidget* sidebar;
    QLabel* brand;
    QLabel* sidebarTagline;
    QWidget* statusCard;
    QPushButton* appearanceButton;
    bool compact = false;
    void updateResponsiveLayout();
    QString material;
    QString accent;
    void updateHeader();
    void appearance();
    void applyBackdrop(QWidget* target = nullptr);
    void saveAppearance();
    void adaptPluginPage(QWidget* page);
};
