/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <memory>
class ModernWindowsAccentSubscription;
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
class QToolButton;
#include "ModernNavigation.h"
class ModernInterface : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    static void applyTheme(QApplication* app);
    static QIcon icon(const QString& name, const QColor& color = QColor("#dedee7"));
    ModernInterface(QMainWindow* window, QTabWidget* mainTabs, QTabWidget* devices, QTabWidget* settings,
        QTabWidget* information, QWidget* commands);
    ~ModernInterface() override;
    void refresh();

protected:
    bool eventFilter(QObject*, QEvent*) override;
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;

private:
    QMainWindow* window;
    QTabWidget* mainTabs;
    QTabWidget* devices;
    ModernNavigation* primary;
    ModernNavigation* deviceNavigation;
    QLabel* title;
    QLabel* description;
    QLabel* deviceCount;
    QWidget* commandBar;
    void updateCommandBar();
    QWidget* sidebar;
    QLabel* brand;
    QToolButton* navigationToggle;
    QString navigationMode = "auto";
    QLabel* sidebarTagline;
    QWidget* statusCard;
    QPushButton* settingsButton;
    bool compact = false;
    void updateResponsiveLayout();
    QString material;
    QString accent;
    QString appliedAccent;
    bool accentRefreshQueued = false;
    std::unique_ptr<ModernWindowsAccentSubscription> accentSubscription;
    void updateHeader();
    void applyBackdrop(QWidget* target = nullptr);
    void saveAppearance();
    void adaptPluginPage(QWidget* page);
};
