/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QtTest>
#include <QApplication>
#include <QTabWidget>
#include <QTabBar>
#include <QListWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include "ModernNavigation.h"
#include "ModernPageLayout.h"
#include "ModernPluginLayout.h"
#include <QFormLayout>
#include <QSpinBox>
#include <QScrollBar>
#include <QLabel>
#include <QFile>
#include <QAction>
#include <QFontDatabase>
#include <QStyleFactory>
class ModernNavigationTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        qApp->setFont(QFont("Segoe UI", 10));
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QFile file(QFINDTESTDATA("../../qt/ModernInterface/modern.qss"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString style = QString::fromUtf8(file.readAll());
        style.replace("@ACCENT@", "#ed96cc");
        style.replace("@ACCENT_SURFACE@", "#473a49");
        qApp->setStyleSheet(style);
    }
    void primaryOrderingAndSelection()
    {
        QWidget host;
        QTabWidget tabs;
        auto devices = new QWidget;
        auto settings = new QWidget;
        auto effects = new QWidget;
        tabs.addTab(settings, "Settings");
        tabs.addTab(devices, "Devices");
        tabs.addTab(effects, "Effects");
        ModernNavigation nav(&tabs, &host, true);
        auto list = nav.widget()->findChild<QListWidget*>();
        QCOMPARE(list->item(0)->text(), QString("Devices"));
        QCOMPARE(list->item(1)->text(), QString("Effects"));
        list->setCurrentRow(1);
        QCOMPARE(tabs.currentWidget(), effects);
        tabs.setCurrentWidget(settings);
        QCOMPARE(list->currentRow(), 2);
    }
    void hotplugRemoveAndReorder()
    {
        QWidget host;
        QTabWidget tabs;
        auto first = new QWidget;
        auto second = new QWidget;
        tabs.addTab(first, "Keyboard");
        tabs.addTab(second, "Mouse");
        ModernNavigation nav(&tabs, &host);
        auto list = nav.widget()->findChild<QListWidget*>();
        tabs.removeTab(0);
        delete first;
        QTRY_COMPARE(list->count(), 1);
        list->setCurrentRow(0);
        QCOMPARE(tabs.currentWidget(), second);
        auto third = new QWidget;
        tabs.addTab(third, "Lamp");
        QTRY_COMPARE(list->count(), 2);
        tabs.tabBar()->moveTab(1, 0);
        QTRY_COMPARE(list->item(0)->text(), QString("Lamp"));
        list->setCurrentRow(0);
        QCOMPARE(tabs.currentWidget(), third);
    }
    void filteringPreservesSelection()
    {
        QWidget host;
        QTabWidget tabs;
        tabs.setObjectName("DevicesTabBar");
        auto keyboard = new QWidget;
        auto mouse = new QWidget;
        tabs.addTab(keyboard, "Keyboard");
        tabs.addTab(mouse, "Mouse");
        ModernNavigation nav(&tabs, &host);
        auto list = nav.widget()->findChild<QListWidget*>();
        auto search = nav.widget()->findChild<QLineEdit*>();
        QVERIFY(search);
        search->setText("mOu");
        QVERIFY(list->item(0)->isHidden());
        QVERIFY(!list->item(1)->isHidden());
        QCOMPARE(tabs.currentWidget(), keyboard);
        list->setCurrentRow(1);
        QCOMPARE(tabs.currentWidget(), mouse);
        search->clear();
        QVERIFY(!list->item(0)->isHidden());
        QCOMPARE(tabs.currentWidget(), mouse);
    }
    void visibilityAndDisabledState()
    {
        QWidget host;
        QTabWidget tabs;
        tabs.addTab(new QWidget, "One");
        tabs.addTab(new QWidget, "Two");
        ModernNavigation nav(&tabs, &host);
        auto list = nav.widget()->findChild<QListWidget*>();
        tabs.setTabEnabled(1, false);
        nav.refresh();
        QVERIFY(!(list->item(1)->flags() & Qt::ItemIsEnabled));
        tabs.setTabVisible(1, false);
        nav.refresh();
        QVERIFY(list->item(1)->isHidden());
        QCOMPARE(nav.visibleCount(), 1);
        tabs.setTabVisible(1, true);
        tabs.setTabEnabled(1, true);
        nav.refresh();
        QCOMPARE(nav.visibleCount(), 2);
        QVERIFY(list->item(1)->flags() & Qt::ItemIsEnabled);
    }
    void customTabLabelsAndRename()
    {
        QWidget host;
        QTabWidget tabs;
        tabs.addTab(new QWidget, "");
        auto label = new QWidget;
        label->setToolTip("Razer BlackWidow");
        tabs.tabBar()->setTabButton(0, QTabBar::LeftSide, label);
        ModernNavigation nav(&tabs, &host);
        auto list = nav.widget()->findChild<QListWidget*>();
        QCOMPARE(list->item(0)->text(), QString("Razer BlackWidow"));
        label->setToolTip("New keyboard name");
        QTRY_COMPARE(list->item(0)->text(), QString("New keyboard name"));
    }
    void emptyAndDeletedTarget()
    {
        QWidget host;
        auto tabs = new QTabWidget;
        ModernNavigation nav(tabs, &host);
        QCOMPARE(nav.visibleCount(), 0);
        QCOMPARE(nav.currentLabel(), QString());
        tabs->addTab(new QWidget, "Later");
        QCoreApplication::processEvents();
        delete tabs;
        nav.refresh();
        nav.filter("test");
        QCOMPARE(nav.currentLabel(), QString());
    }
    void noLayoutRefreshLoop()
    {
        QWidget host;
        QTabWidget tabs;
        tabs.addTab(new QWidget, "Keyboard");
        ModernNavigation nav(&tabs, &host);
        QCoreApplication::processEvents();
        QSignalSpy spy(&nav, &ModernNavigation::changed);
        QTest::qWait(100);
        const int before = spy.count();
        QTest::qWait(100);
        QCOMPARE(spy.count(), before);
    }
    void scrollSurfacePreservesPageIdentityAndOwnership()
    {
        QTabWidget tabs;
        tabs.resize(640, 360);
        auto page = new QWidget;
        auto layout = new QVBoxLayout(page);
        auto group = new QGroupBox("Performance", page);
        auto form = new QFormLayout(group);
        for(int i = 0; i < 12; ++i)
        {
            auto number = new QSpinBox(group);
            number->setValue(60);
            form->addRow(new QLabel("Capture rate", group), number);
        }
        layout->addWidget(group);
        QPointer<QWidget> guardedGroup = group;
        tabs.addTab(page, "Ambient");
        QVERIFY(ModernPageLayout::MakeScrollable(page));
        QCOMPARE(tabs.widget(0), page);
        QVERIFY(page->isAncestorOf(group));
        QVERIFY(!ModernPageLayout::MakeScrollable(page));
        QCOMPARE(page->findChildren<QScrollArea*>().size(), 1);
        tabs.show();
        QTest::qWait(10);
        auto scroll = page->findChild<QScrollArea*>();
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        for(auto label : group->findChildren<QLabel*>())
            QVERIFY(label->height() >= label->fontMetrics().height());
        delete page;
        QVERIFY(guardedGroup.isNull());
        QCOMPARE(tabs.count(), 0);
    }
    void keyboardNavigationAfterThemeRefresh()
    {
        QWidget host;
        QTabWidget tabs;
        tabs.addTab(new QWidget, "Devices");
        tabs.addTab(new QWidget, "Effects");
        ModernNavigation nav(&tabs, &host, true);
        auto list = nav.widget()->findChild<QListWidget*>();
        host.show();
        list->setFocus();
        QTest::keyClick(list, Qt::Key_Down);
        QCOMPARE(tabs.currentIndex(), 1);
        qApp->setStyleSheet(qApp->styleSheet());
        QCoreApplication::processEvents();
        QCOMPARE(tabs.currentIndex(), 1);
        QCOMPARE(list->currentRow(), 1);
    }
    void lightSyncSectionsKeepLiveControls()
    {
        QWidget root;
        auto original = new QVBoxLayout(&root);
        auto header = new QHBoxLayout;
        header->addWidget(new QLabel("LightSync"));
        original->addLayout(header);
        original->addWidget(new QLabel("Explanation"));
        original->addWidget(new QLabel("Keyboard status"));
        auto table = new QTableWidget(8, 6);
        original->addWidget(table);
        auto group = new QGroupBox("Lights");
        auto form = new QVBoxLayout(group);
        auto rate = new QSpinBox;
        rate->setObjectName("yeelight");
        rate->setValue(10);
        form->addWidget(rate);
        original->addWidget(group);
        auto maintenance = new QHBoxLayout;
        maintenance->addWidget(new QPushButton("Open log"));
        original->addLayout(maintenance);
        auto status = new QLabel("Engine running");
        original->addWidget(status);
        original->addWidget(new QLabel("Message"));
        QVERIFY(ModernPluginLayout::MakeLightSyncSections(&root));
        QVERIFY(!ModernPluginLayout::MakeLightSyncSections(&root));
        auto tabs = root.findChild<QTabWidget*>("LightSyncSections");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(root.findChild<QSpinBox*>("yeelight"), rate);
        QCOMPARE(rate->value(), 10);
        QVERIFY(tabs->widget(0)->isAncestorOf(table));
        QVERIFY(tabs->widget(1)->isAncestorOf(group));
        QVERIFY(tabs->widget(0)->isAncestorOf(status));
    }
    void compactNavigationKeepsAccessibleLabels()
    {
        QWidget host;
        QTabWidget tabs;
        auto devices = new QWidget;
        auto effects = new QWidget;
        tabs.addTab(devices, "Devices");
        tabs.addTab(effects, "Effects");
        ModernNavigation nav(&tabs, &host, true);
        auto list = nav.widget()->findChild<QListWidget*>();
        list->setCurrentRow(1);
        nav.setCompact(true);
        QCOMPARE(nav.widget()->width(), 52);
        QCOMPARE(list->item(1)->text(), QString("Effects"));
        QCOMPARE(list->item(1)->toolTip(), QString("Effects"));
        QCOMPARE(tabs.currentWidget(), effects);
        nav.setCompact(false);
        QCOMPARE(nav.widget()->width(), 212);
        QCOMPARE(list->currentRow(), 1);
    }
    void profileBarRetainsConnectedActions()
    {
        QWidget root;
        auto layout = new QGridLayout(&root);
        auto label = new QLabel("Active Profile:", &root);
        label->setObjectName("ActiveProfileLabel");
        auto profiles = new QComboBox(&root);
        profiles->setObjectName("ProfileBox");
        profiles->addItem("Ambilight");
        auto save = new QToolButton(&root);
        save->setObjectName("ButtonSaveProfile");
        auto action = new QAction("Save Profile", &root);
        save->setDefaultAction(action);
        layout->addWidget(label, 0, 0);
        layout->addWidget(profiles, 0, 1);
        layout->addWidget(save, 0, 2);
        for(const QString& name :
            { QString("ButtonDeleteProfile"), QString("ButtonToggleDeviceView"), QString("ButtonRescan") })
        {
            auto button = new QPushButton(name, &root);
            button->setObjectName(name);
            layout->addWidget(button);
        }
        QSignalSpy triggered(action, &QAction::triggered);
        QVERIFY(ModernPluginLayout::MakeCommandBar(&root));
        QVERIFY(!ModernPluginLayout::MakeCommandBar(&root));
        QCOMPARE(root.findChild<QToolButton*>("ButtonSaveProfile"), save);
        QCOMPARE(profiles->currentText(), QString("Ambilight"));
        save->click();
        QCOMPARE(triggered.count(), 1);
    }
    void translatedCorePageOrdering()
    {
        QWidget host;
        QTabWidget tabs;
        for(auto entry : { QPair<QString, QString> { "TabSettings", "Parametres" },
                { "TabInformation", "Informations" }, { "TabDevices", "Peripheriques" } })
        {
            auto page = new QWidget;
            page->setObjectName(entry.first);
            tabs.addTab(page, entry.second);
        }
        tabs.addTab(new QWidget, "Effects");
        ModernNavigation nav(&tabs, &host, true);
        auto list = nav.widget()->findChild<QListWidget*>();
        QCOMPARE(list->item(0)->text(), QString("Peripheriques"));
        QCOMPARE(list->item(1)->text(), QString("Effects"));
        QCOMPARE(list->item(2)->text(), QString("Parametres"));
        QCOMPARE(list->item(3)->text(), QString("Informations"));
    }
    void repeatedPluginReload()
    {
        QWidget host;
        QTabWidget tabs;
        tabs.addTab(new QWidget, "Devices");
        ModernNavigation nav(&tabs, &host, true);
        auto list = nav.widget()->findChild<QListWidget*>();
        for(int i = 0; i < 200; ++i)
        {
            auto page = new QWidget;
            tabs.addTab(page, "LightSync");
            nav.refresh();
            list->setCurrentRow(1);
            QCOMPARE(tabs.currentWidget(), page);
            tabs.removeTab(1);
            delete page;
            QCoreApplication::processEvents();
            nav.refresh();
            QCOMPARE(list->count(), 1);
            QCOMPARE(tabs.currentIndex(), 0);
        }
    }
};
QTEST_MAIN(ModernNavigationTests)
#include "ModernNavigationTests.moc"
