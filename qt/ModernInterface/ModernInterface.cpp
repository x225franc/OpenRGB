/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ModernInterface.h"
#include "ModernPageLayout.h"
#include "ModernPluginLayout.h"
#include "ModernWindowsAccent.h"
#include "ResourceManager.h"
#include "SettingsManager.h"
#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QDialog>
#include <QMenu>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QTimer>
#include <QScreen>
#include <QScrollArea>
#include <QGroupBox>
#include <QStyleFactory>
#include <QStyle>
#include <QEvent>
#include <QShortcut>
#include <QAbstractItemView>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif
namespace
{
QString accentColor()
{
    auto settings = ResourceManager::get()->GetSettingsManager()->GetSettings("ModernInterface");
    const std::string value
        = settings.is_object() && settings.contains("accent") && settings["accent"].is_string()
        ? settings["accent"].get<std::string>()
        : "windows";
    if(value == "windows")
        return modernWindowsAccent().name();
    if(value == "blue")
        return "#8bb9ff";
    if(value == "mint")
        return "#8edcba";
    if(value == "violet")
        return "#bba5ff";
    return "#ed96cc";
}
QColor accentForeground(const QColor& color)
{
    auto linear = [](double channel)
    {
        channel /= 255.0;
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const double luminance
        = 0.2126 * linear(color.red()) + 0.7152 * linear(color.green()) + 0.0722 * linear(color.blue());
    return QColor((luminance + 0.05) / 0.05 >= 1.05 / (luminance + 0.05) ? "#181820" : "#ffffff");
}
void wrapNavigation(QTabWidget* tabs, ModernNavigation* navigation)
{
    auto parentLayout = tabs->parentWidget()->layout();
    auto wrapper = new QWidget(tabs->parentWidget());
    wrapper->setObjectName("NavigationWorkspace");
    delete parentLayout->replaceWidget(tabs, wrapper);
    auto row = new QHBoxLayout(wrapper);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);
    row->addWidget(navigation->widget());
    row->addWidget(tabs, 1);
}
}
void ModernInterface::applyTheme(QApplication* app)
{
    // Appearance belongs to this UI, never to a remote SDK server.
    json schema;
    schema["material"] = { { "type", "string" }, { "title", "Window material" }, { "default", "acrylic" },
        { "enum", { "acrylic", "mica", "opaque" } }, { "order", 0 } };
    schema["accent"] = { { "type", "string" }, { "title", "Accent colour" }, { "default", "windows" },
        { "enum", { "windows", "rose", "blue", "mint", "violet" } }, { "order", 1 } };
    schema["navigation"] = { { "type", "string" }, { "title", "Navigation" }, { "default", "auto" },
        { "enum", { "auto", "expanded", "collapsed" } }, { "order", 2 } };
    // Register the complete appearance schema, including the persistent sidebar choice.
    ResourceManager::get()->GetSettingsManager()->RegisterSettingsSchemaComplete(
        "ModernInterface", "Appearance", schema, 1, true, false);
    app->setWindowIcon(QIcon(":OpenRGBMonochrome.png"));
    app->setStyle(QStyleFactory::create("Fusion"));
    QFont font("Segoe UI Variable Text", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    app->setFont(font);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#202026"));
    palette.setColor(QPalette::WindowText, QColor("#f1f1f6"));
    palette.setColor(QPalette::Base, QColor("#24242c"));
    palette.setColor(QPalette::AlternateBase, QColor("#2b2b33"));
    palette.setColor(QPalette::Text, QColor("#f1f1f6"));
    palette.setColor(QPalette::Button, QColor("#34343e"));
    palette.setColor(QPalette::ButtonText, QColor("#f1f1f6"));
    palette.setColor(QPalette::Highlight, QColor(accentColor()));
    palette.setColor(QPalette::HighlightedText, accentForeground(QColor(accentColor())));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    palette.setColor(QPalette::Accent, QColor(accentColor()));
#endif
    palette.setColor(QPalette::Link, QColor(accentColor()));
    palette.setColor(QPalette::ToolTipBase, QColor("#35353e"));
    palette.setColor(QPalette::ToolTipText, QColor("#f1f1f6"));
    for(auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        palette.setColor(QPalette::Disabled, role, QColor("#82828f"));
    app->setPalette(palette);
    QFile stylesheet(":/modern/modern.qss");
    if(stylesheet.open(QIODevice::ReadOnly))
    {
        QString style = QString::fromUtf8(stylesheet.readAll());
        style.replace("@ACCENT@", accentColor());
        const QColor foreground = accentForeground(QColor(accentColor()));
        style.replace("@ACCENT_TEXT@", foreground.name());
        const bool white = foreground == QColor("#ffffff");
        style.replace("@CHECK_IMAGE@", white ? ":/modern/check-white.png" : ":/modern/check.png");
        style.replace("@TOGGLE_IMAGE@", white ? ":/modern/toggle-on-white.png" : ":/modern/toggle-on.png");
        const QColor base("#2a2a32"), accent(accentColor());
        const QColor surface((base.red() * 85 + accent.red() * 15) / 100,
            (base.green() * 85 + accent.green() * 15) / 100, (base.blue() * 85 + accent.blue() * 15) / 100);
        style.replace("@ACCENT_SURFACE@", surface.name());
        app->setStyleSheet(style);
    }
}
ModernInterface::ModernInterface(QMainWindow* mainWindow, QTabWidget* main, QTabWidget* deviceTabs,
    QTabWidget* settingTabs, QTabWidget* informationTabs, QWidget* commands)
    : QObject(mainWindow)
    , window(mainWindow)
    , mainTabs(main)
    , devices(deviceTabs)
    , commandBar(commands)
{
    window->setWindowTitle("OpenRGB");
    window->setMinimumSize(1080, 720);
    auto settings = ResourceManager::get()->GetSettingsManager()->GetSettings("ModernInterface");
    if(!settings.is_object())
        settings = json::object();
    material = QString::fromStdString(settings.value("material", std::string("acrylic")));
    accent = QString::fromStdString(settings.value("accent", std::string("windows")));
    navigationMode = QString::fromStdString(settings.value("navigation", std::string("auto")));
    appliedAccent = accentColor();
    QWidget* oldCentral = window->takeCentralWidget();
    if(auto holder = oldCentral->findChild<QHBoxLayout*>("MainButtonsLayout"))
        holder->removeWidget(commands);
    auto root = new QWidget(window);
    root->setObjectName("ModernRoot");
    window->setCentralWidget(root);
    auto row = new QHBoxLayout(root);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    sidebar = new QWidget(root);
    sidebar->setObjectName("AppSidebar");
    sidebar->setFixedWidth(248);
    auto side = new QVBoxLayout(sidebar);
    side->setContentsMargins(18, 26, 18, 18);
    side->setSpacing(18);
    auto brandRow = new QHBoxLayout;
    navigationToggle = new QToolButton(sidebar);
    navigationToggle->setObjectName("NavigationToggle");
    navigationToggle->setIcon(icon("menu"));
    navigationToggle->setIconSize(QSize(20, 20));
    navigationToggle->setFixedSize(32, 32);
    navigationToggle->setCheckable(true);
    navigationToggle->setCursor(Qt::PointingHandCursor);
    navigationToggle->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(navigationToggle, &QToolButton::clicked, this,
        [this]
        {
            navigationMode = compact ? "expanded" : "collapsed";
            saveAppearance();
            updateResponsiveLayout();
        });
    connect(navigationToggle, &QToolButton::customContextMenuRequested, this,
        [this](const QPoint& point)
        {
            QMenu menu(navigationToggle);
            auto automatic = menu.addAction(tr("Automatic width"));
            automatic->setCheckable(true);
            automatic->setChecked(navigationMode == "auto");
            if(menu.exec(navigationToggle->mapToGlobal(point)) == automatic)
            {
                navigationMode = "auto";
                saveAppearance();
                updateResponsiveLayout();
            }
        });
    brandLogo = new QLabel(sidebar);
    brandLogo->setPixmap(QIcon(":OpenRGBMonochrome.png").pixmap(28, 28));
    brandLogo->setFixedSize(32, 32);
    brand = new QLabel("OpenRGB", sidebar);
    brand->setObjectName("BrandName");
    brandRow->addWidget(navigationToggle);
    brandRow->addWidget(brandLogo);
    brandRow->addWidget(brand);
    brandRow->addStretch();
    side->addLayout(brandRow);
    sidebarTagline = new QLabel(tr("YOUR LIGHTING, TOGETHER"), sidebar);
    sidebarTagline->setObjectName("SidebarEyebrow");
    side->addWidget(sidebarTagline);
    primary = new ModernNavigation(mainTabs, sidebar, true);
    side->addWidget(primary->widget(), 1);
    appearanceButton = new QPushButton(ModernInterface::icon("appearance"), tr("Appearance"), sidebar);
    appearanceButton->setObjectName("AppearanceButton");
    appearanceButton->setCursor(Qt::PointingHandCursor);
    connect(appearanceButton, &QPushButton::clicked, this, &ModernInterface::appearance);
    side->addWidget(appearanceButton);
    statusCard = new QFrame(sidebar);
    statusCard->setObjectName("SidebarStatus");
    auto statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(14, 12, 14, 12);
    auto statusTitle = new QLabel(tr("LIGHTING STUDIO"), statusCard);
    statusTitle->setObjectName("SidebarEyebrow");
    deviceCount = new QLabel(statusCard);
    deviceCount->setObjectName("DeviceCount");
    statusLayout->addWidget(statusTitle);
    statusLayout->addWidget(deviceCount);
    side->addWidget(statusCard);
    row->addWidget(sidebar);
    auto workspace = new QWidget(root);
    workspace->setObjectName("AppWorkspace");
    auto column = new QVBoxLayout(workspace);
    column->setContentsMargins(26, 26, 26, 20);
    column->setSpacing(20);
    auto heading = new QWidget(workspace);
    heading->setObjectName("PageHeading");
    auto headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    headingLayout->setSpacing(6);
    title = new QLabel(heading);
    title->setObjectName("PageTitle");
    description = new QLabel(heading);
    description->setObjectName("PageDescription");
    description->setWordWrap(true);
    headingLayout->addWidget(title);
    headingLayout->addWidget(description);
    column->addWidget(heading);
    oldCentral->layout()->setContentsMargins(0, 0, 0, 0);
    oldCentral->layout()->setSpacing(12);
    auto workspaceScroll = new QScrollArea(workspace);
    workspaceScroll->setObjectName("WorkspaceScroll");
    workspaceScroll->setFrameShape(QFrame::NoFrame);
    workspaceScroll->setWidgetResizable(true);
    // Hidden tab pages can report a larger minimum than the visible page.
    // Do not let that stretch every page; retain a bounded overflow fallback.
    oldCentral->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    oldCentral->setMinimumWidth(1060);
    workspaceScroll->setWidget(oldCentral);
    column->addWidget(workspaceScroll, 1);
    column->addWidget(commands);
    row->addWidget(workspace, 1);
    commands->setObjectName("ProfileCommandBar");
    ModernPluginLayout::MakeCommandBar(commands);
    updateCommandBar();
    deviceNavigation = new ModernNavigation(devices, root);
    wrapNavigation(devices, deviceNavigation);
    wrapNavigation(settingTabs, new ModernNavigation(settingTabs, root));
    wrapNavigation(informationTabs, new ModernNavigation(informationTabs, root));
    connect(mainTabs, &QTabWidget::currentChanged, this, [this] { updateHeader(); });
    connect(primary, &ModernNavigation::changed, this, [this] { updateHeader(); });
    connect(deviceNavigation, &ModernNavigation::changed, this, [this] { updateHeader(); });
    window->installEventFilter(this);
    qApp->installEventFilter(this);
    qApp->installNativeEventFilter(this);
    auto shortcut = new QShortcut(QKeySequence("Ctrl+1"), window);
    QPointer<QWidget> devicesPage = mainTabs->widget(0);
    connect(shortcut, &QShortcut::activated, this,
        [this, devicesPage]
        {
            if(devicesPage)
                mainTabs->setCurrentWidget(devicesPage);
        });
    updateHeader();
    updateResponsiveLayout();
    QTimer::singleShot(0, this, [this] { applyBackdrop(); });
}
ModernInterface::~ModernInterface()
{
    qApp->removeNativeEventFilter(this);
    accentSubscription.reset();
}
bool ModernInterface::nativeEventFilter(const QByteArray&, void* message, qintptr*)
{
#ifdef Q_OS_WIN
    const auto native = static_cast<MSG*>(message);
    if(accent == "windows" && !accentRefreshQueued
        && (native->message == ModernAccentChangedMessage || native->message == WM_DWMCOLORIZATIONCOLORCHANGED
            || native->message == WM_SETTINGCHANGE || native->message == WM_THEMECHANGED))
    {
        accentRefreshQueued = true;
        QTimer::singleShot(100, this,
            [this]
            {
                accentRefreshQueued = false;
                const QString color = accentColor();
                if(color != appliedAccent)
                {
                    appliedAccent = color;
                    applyTheme(qApp);
                }
            });
    }
#else
    Q_UNUSED(message);
#endif
    return false;
}
void ModernInterface::refresh()
{
    const auto settings = ResourceManager::get()->GetSettingsManager()->GetSettings("ModernInterface");
    if(settings.is_object())
    {
        const auto requestedMaterial
            = QString::fromStdString(settings.value("material", std::string("acrylic")));
        const auto requestedAccent = QString::fromStdString(settings.value("accent", std::string("windows")));
        navigationMode = QString::fromStdString(settings.value("navigation", std::string("auto")));
        updateResponsiveLayout();
        if(requestedMaterial != material || requestedAccent != accent)
        {
            material = requestedMaterial;
            accent = requestedAccent;
            appliedAccent = accentColor();
            applyTheme(qApp);
            applyBackdrop();
        }
    }
    primary->refresh();
    deviceNavigation->refresh();
    updateHeader();
}
void ModernInterface::updateHeader()
{
    const QString label = primary->currentLabel();
    title->setText(label);
    QString subtitle;
    const QString lower = label.toLower();
    if(lower.contains("devices"))
        subtitle = tr("Choose a device and make it yours.");
    else if(lower.contains("effects"))
        subtitle = tr("Bring your lighting to life, across your whole setup.");
    else if(lower.contains("lightsync"))
        subtitle = tr("LAN lighting and your laptop keyboard, in one place.");
    else if(lower.contains("visual"))
        subtitle = tr("Arrange your lights the way they sit around you.");
    else if(lower.contains("settings"))
        subtitle = tr("Set up OpenRGB to work your way.");
    else if(lower.contains("sdk"))
        subtitle = tr("Connect your lighting to other applications and computers.");
    else
        subtitle = tr("Your devices. Your colours. One workspace.");
    description->setText(subtitle);
    const int count = deviceNavigation->visibleCount();
    deviceCount->setText(count == 1 ? tr("1 device available") : tr("%1 devices available").arg(count));
}
void ModernInterface::adaptPluginPage(QWidget* page)
{
    if(!page || page->property("modernScrollPage").toBool() || !page->layout())
        return;
    if(auto preview = page->findChild<QToolButton*>("preview"))
    {
        preview->setIcon(icon("monitor"));
        preview->setToolButtonStyle(Qt::ToolButtonIconOnly);
        preview->setAccessibleName(tr("Preview effect"));
    }
    if(auto information = page->findChild<QToolButton*>("toggle_info"))
    {
        information->setIcon(icon("information"));
        information->setToolButtonStyle(Qt::ToolButtonIconOnly);
        information->setAccessibleName(tr("Effect information"));
    }
    ModernPageLayout::MakeScrollable(page);
}
bool ModernInterface::eventFilter(QObject* target, QEvent* event)
{
    if(target == window && event->type() == QEvent::LanguageChange)
        QTimer::singleShot(0, this, [this] { updateCommandBar(); });
    if(target == window && event->type() == QEvent::Resize)
        updateResponsiveLayout();
    if(event->type() == QEvent::Polish || event->type() == QEvent::Show
        || event->type() == QEvent::DynamicPropertyChange)
    {
        auto widget = qobject_cast<QWidget*>(target);
        if(widget && widget->property("modernPlugin").toString() == "LightSync"
            && !widget->property("modernLightSyncLayout").toBool())
        {
            QPointer<QWidget> guarded = widget;
            QTimer::singleShot(0, this,
                [guarded]
                {
                    if(guarded)
                        ModernPluginLayout::MakeLightSyncSections(guarded);
                });
        }
        if(widget && QByteArray(widget->metaObject()->className()) == "OpenRGBEffectPage"
            && !widget->property("modernScrollPage").toBool())
        {
            QPointer<QWidget> guarded = widget;
            QTimer::singleShot(0, this,
                [this, guarded]
                {
                    if(guarded)
                        adaptPluginPage(guarded);
                });
        }
    }
    if(event->type() == QEvent::Show)
    {
        auto dialog = qobject_cast<QDialog*>(target);
        if(dialog && dialog->isWindow() && !(dialog->windowFlags() & Qt::FramelessWindowHint)
            && QByteArray(dialog->metaObject()->className()) != "QFileDialog"
            && QByteArray(dialog->metaObject()->className()) != "QColorDialog"
            && QByteArray(dialog->metaObject()->className()) != "QFontDialog")
        {
            QPointer<QWidget> guarded = dialog;
            QTimer::singleShot(0, this,
                [this, guarded]
                {
                    if(guarded)
                        applyBackdrop(guarded);
                });
        }
    }
    if(target == window
        && (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange
            || event->type() == QEvent::ThemeChange))
        QTimer::singleShot(0, this, [this] { applyBackdrop(); });
    return false;
}
void ModernInterface::applyBackdrop(QWidget* target)
{
    QWidget* surface = target ? target : window;
#ifdef Q_OS_WIN
    HWND handle = reinterpret_cast<HWND>(surface->winId());
    if(surface == window && !accentSubscription)
    {
        try
        {
            accentSubscription = std::make_unique<ModernWindowsAccentSubscription>(surface->winId());
        }
        catch(...)
        {
        } // Native Windows theme messages remain a fallback.
    }
    const BOOL dark = TRUE;
    const int rounded = 2;
    DwmSetWindowAttribute(handle, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(handle, 33, &rounded, sizeof(rounded));
    HIGHCONTRASTW contrast = { sizeof(HIGHCONTRASTW), 0, nullptr };
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    int type = material == "opaque" || (contrast.dwFlags & HCF_HIGHCONTRASTON) ? 1
        : material == "mica"                                                   ? 2
                                                                               : 3;
    const HRESULT result = DwmSetWindowAttribute(handle, 38, &type, sizeof(type));
    const bool supported = SUCCEEDED(result) && type != 1;
    const bool changed = surface->property("backdrop").toBool() != supported;
    surface->setProperty("backdrop", supported);
    if(changed)
    {
        surface->style()->unpolish(surface);
        surface->style()->polish(surface);
    }
    QPalette palette = surface->palette();
    palette.setColor(QPalette::Window, supported ? QColor(0, 0, 0, 0) : QColor("#202026"));
    surface->setPalette(palette);
    surface->setAttribute(Qt::WA_NoSystemBackground, false);
    surface->setAttribute(Qt::WA_StyledBackground, true);
    const MARGINS margins = supported ? MARGINS { -1, -1, -1, -1 } : MARGINS { 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(handle, &margins);
#else
    surface->setProperty("backdrop", false);
#endif
    surface->update();
}
void ModernInterface::updateCommandBar()
{
    if(auto label = commandBar->findChild<QLabel*>("ActiveProfileLabel"))
        label->setText(tr("Profile"));
    if(auto preview = commandBar->findChild<QPushButton*>("ButtonToggleDeviceView"))
        preview->setText(tr("LED preview"));
    if(auto scan = commandBar->findChild<QPushButton*>("ButtonRescan"))
        scan->setText(tr("Search again"));
    if(auto remove = commandBar->findChild<QPushButton*>("ButtonDeleteProfile"))
        remove->setText(tr("Delete"));
    if(auto save = commandBar->findChild<QToolButton*>("ButtonSaveProfile"))
    {
        save->setProperty("primary", true);
        save->style()->unpolish(save);
        save->style()->polish(save);
    }
    commandBar->style()->unpolish(commandBar);
    commandBar->style()->polish(commandBar);
}
void ModernInterface::updateResponsiveLayout()
{
    const bool requested
        = navigationMode == "collapsed" || (navigationMode != "expanded" && window->width() < 1440);
    navigationToggle->setChecked(!requested);
    navigationToggle->setToolTip(requested ? tr("Expand navigation") : tr("Collapse navigation"));
    navigationToggle->setAccessibleName(navigationToggle->toolTip());
    if(requested == compact)
        return;
    compact = requested;
    sidebar->setFixedWidth(compact ? 88 : 248);
    primary->setCompact(compact);
    brand->setVisible(!compact);
    brandLogo->setVisible(!compact);
    sidebarTagline->setVisible(!compact);
    statusCard->setVisible(!compact);
    appearanceButton->setText(compact ? QString() : tr("Appearance"));
    appearanceButton->setToolTip(tr("Appearance"));
    appearanceButton->setAccessibleName(tr("Appearance"));
}
void ModernInterface::saveAppearance()
{
    auto manager = ResourceManager::get()->GetSettingsManager();
    auto settings = manager->GetSettings("ModernInterface");
    settings["material"] = material.toStdString();
    settings["accent"] = accent.toStdString();
    settings["navigation"] = navigationMode.toStdString();
    manager->SetSettings("ModernInterface", settings);
    manager->SaveSettings();
}
void ModernInterface::appearance()
{
    QDialog dialog(window);
    dialog.setAttribute(Qt::WA_TranslucentBackground, true);
    dialog.setObjectName("AppearanceDialog");
    dialog.setWindowTitle(tr("Appearance"));
    dialog.setMinimumWidth(430);
    auto layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);
    auto heading = new QLabel(tr("Make yourself at home"), &dialog);
    heading->setObjectName("DialogHeading");
    layout->addWidget(heading);
    auto note = new QLabel(tr("A quiet, dark workspace with a little colour."), &dialog);
    note->setProperty("muted", true);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto form = new QFormLayout;
    form->setSpacing(16);
    QComboBox surface, colour;
    surface.addItem(tr("Acrylic"), "acrylic");
    surface.addItem(tr("Mica"), "mica");
    surface.addItem(tr("Opaque"), "opaque");
    surface.setCurrentIndex(qMax(0, surface.findData(material)));
    colour.addItem(tr("Windows (automatic)"), "windows");
    colour.addItem(tr("Rose"), "rose");
    colour.addItem(tr("Blue"), "blue");
    colour.addItem(tr("Mint"), "mint");
    colour.addItem(tr("Violet"), "violet");
    colour.setCurrentIndex(qMax(0, colour.findData(accent)));
    form->addRow(tr("Window material"), &surface);
    form->addRow(tr("Accent colour"), &colour);
    layout->addLayout(form);
    auto hint = new QLabel(
        tr("Windows controls transparency. Acrylic and Mica fall back to a solid surface when unavailable."),
        &dialog);
    hint->setWordWrap(true);
    hint->setProperty("muted", true);
    layout->addWidget(hint);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if(dialog.exec() == QDialog::Accepted)
    {
        material = surface.currentData().toString();
        accent = colour.currentData().toString();
        saveAppearance();
        appliedAccent = accentColor();
        applyTheme(qApp);
        applyBackdrop();
    }
}
