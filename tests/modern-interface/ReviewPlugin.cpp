/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ReviewPlugin.h"
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QTabWidget>
#include <QTabBar>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QAbstractButton>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QCheckBox>
#include <QScrollBar>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QScreen>
#include <QWindow>
OpenRGBPluginInfo ReviewPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info { };
    info.Name = "OpenRGB UI Review";
    info.Version = "1.0";
    info.Description = "Development-only interface validation driver";
    info.Label = "UI Review";
    info.Location = OPENRGB_PLUGIN_LOCATION_INFORMATION;
    return info;
}
void ReviewPlugin::Load(OpenRGBPluginAPIInterface*)
{
    panel = new QWidget;
    auto layout = new QVBoxLayout(panel);
    layout->addWidget(new QLabel("Development validation driver. Not part of the release.", panel));
    directory = qEnvironmentVariable("OPENRGB_UI_REVIEW_DIR");
    if(directory.isEmpty())
        return;
    QDir().mkpath(directory + "/screenshots");
    connect(&timer, &QTimer::timeout, this, &ReviewPlugin::poll);
    timer.start(200);
}
void ReviewPlugin::poll()
{
    QFile input(directory + "/command.json");
    if(!input.open(QIODevice::ReadOnly))
        return;
    auto command = QJsonDocument::fromJson(input.readAll()).object();
    const QString id = command["id"].toString();
    if(id.isEmpty() || id == lastId)
        return;
    lastId = id;
    auto result = execute(command);
    result["id"] = id;
    result["at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QFile output(directory + "/response.json");
    if(output.open(QIODevice::WriteOnly))
        output.write(QJsonDocument(result).toJson());
}
QJsonObject ReviewPlugin::execute(const QJsonObject& c)
{
    QMainWindow* window = nullptr;
    for(auto top : qApp->topLevelWidgets())
        if(top->objectName() == "OpenRGBDialog")
            window = qobject_cast<QMainWindow*>(top);
    if(!window)
        return { { "ok", false }, { "error", "main window missing" } };
    const QString action = c["action"].toString();
    if(action == "navigate")
    {
        auto tabs = window->findChild<QTabWidget*>("MainTabBar");
        bool found = false;
        for(int i = 0; i < tabs->count(); ++i)
            if(tabs->tabText(i) == c["page"].toString())
            {
                tabs->setCurrentIndex(i);
                found = true;
                break;
            }
        if(!found)
            return { { "ok", false }, { "error", "page missing" } };
        const QString sub = c["subpage"].toString();
        if(!sub.isEmpty())
            for(auto inner : tabs->currentWidget()->findChildren<QTabWidget*>())
                for(int i = 0; i < inner->count(); ++i)
                {
                    auto button = inner->tabBar()->tabButton(i, QTabBar::LeftSide);
                    if(inner->tabText(i) == sub || (button && button->toolTip() == sub))
                    {
                        inner->setCurrentIndex(i);
                        found = true;
                    }
                }
    }
    else if(action == "capture")
    {
        QString name = c["name"].toString();
        name.remove('/');
        name.remove('\\');
        name.remove("..");
        if(name.isEmpty())
            name = "window";
        const bool desktop = c["desktop"].toBool();
        QWidget* surface = window;
        if(!c["object"].toString().isEmpty())
        {
            auto found = window->findChild<QWidget*>(c["object"].toString());
            if(found)
                surface = found;
        }
        bool saved = desktop
            ? window->screen()->grabWindow(0).save(directory + "/screenshots/" + name + ".png")
            : surface->grab().save(directory + "/screenshots/" + name + ".png");
        return { { "ok", saved }, { "width", window->width() }, { "height", window->height() },
            { "devicePixelRatio", window->devicePixelRatioF() } };
    }
    else if(action == "search")
    {
        if(auto line = window->findChild<QLineEdit*>("DeviceSearch"))
            line->setText(c["text"].toString());
    }
    else if(action == "appearance")
    {
        auto button = window->findChild<QAbstractButton*>("AppearanceButton");
        if(!button)
            return { { "ok", false } };
        QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
    }
    else if(action == "appearance-set")
    {
        QWidget* dialog = nullptr;
        for(auto top : qApp->topLevelWidgets())
            if(top->objectName() == "AppearanceDialog")
                dialog = top;
        if(!dialog)
            return { { "ok", false }, { "error", "appearance not open" } };
        auto combos = dialog->findChildren<QComboBox*>();
        if(combos.size() != 2)
            return { { "ok", false } };
        combos[0]->setCurrentIndex(combos[0]->findData(c["material"].toString()));
        combos[1]->setCurrentIndex(combos[1]->findData(c["accent"].toString()));
        auto buttons = dialog->findChild<QDialogButtonBox*>();
        QMetaObject::invokeMethod(buttons->button(QDialogButtonBox::Save), "click", Qt::QueuedConnection);
    }
    else if(action == "scroll")
    {
        auto area = window->findChild<QScrollArea*>(c["object"].toString());
        if(!area)
            return { { "ok", false } };
        area->verticalScrollBar()->setValue(c["value"].toInt());
    }
    else if(action == "set-number")
    {
        auto number = window->findChild<QSpinBox*>(c["object"].toString());
        if(!number)
            return { { "ok", false } };
        number->setValue(c["value"].toInt());
    }
    else if(action == "resize")
        window->resize(c["width"].toInt(), c["height"].toInt());
    else if(action == "show")
        window->showNormal();
    else if(action == "hide")
        window->hide();
    else if(action == "quit")
        QTimer::singleShot(0, qApp, &QApplication::quit);
    else if(action == "click")
    {
        if(auto button = window->findChild<QAbstractButton*>(c["object"].toString()))
            QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
        else
            return { { "ok", false }, { "error", "button missing" } };
    }
    else if(action == "state")
    {
        QJsonArray widgets;
        for(auto w : window->findChildren<QWidget*>())
            if(w->isVisibleTo(window) && !w->objectName().isEmpty())
            {
                auto bounds = w->geometry();
                QJsonObject item { { "class", w->metaObject()->className() }, { "name", w->objectName() },
                    { "x", w->mapTo(window, QPoint()).x() }, { "y", w->mapTo(window, QPoint()).y() },
                    { "w", bounds.width() }, { "h", bounds.height() }, { "enabled", w->isEnabled() } };
                if(auto label = qobject_cast<QLabel*>(w))
                    item["text"] = label->text();
                if(auto line = qobject_cast<QLineEdit*>(w))
                {
                    item["text"] = line->text();
                    item["font"] = line->font().family();
                    item["contentWidth"] = line->contentsRect().width();
                    item["contentHeight"] = line->contentsRect().height();
                }
                if(auto number = qobject_cast<QSpinBox*>(w))
                {
                    item["text"] = number->text();
                    item["value"] = number->value();
                }
                if(auto button = qobject_cast<QAbstractButton*>(w))
                {
                    item["text"] = button->text();
                    item["font"] = button->font().family();
                    item["checked"] = button->isChecked();
                }
                widgets.append(item);
            }
        return { { "ok", true }, { "widgets", widgets }, { "width", window->width() },
            { "height", window->height() }, { "visible", window->isVisible() },
            { "backdrop", window->property("backdrop").toBool() },
            { "alphaBuffer", window->windowHandle() ? window->windowHandle()->format().alphaBufferSize() : -1 } };
    }
    return { { "ok", true } };
}
