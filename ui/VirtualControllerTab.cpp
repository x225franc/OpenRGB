/*---------------------------------------------------------*\
| VirtualControllerTab.cpp                                  |
|                                                           |
|   Virtual controller tab for visual map plugin            |
|                                                           |
|   This file is part of the OpenRGB Visual Map Plugin      |
|   project                                                 |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cmath>
#include <limits>
#include <QInputDialog>
#include <QMenu>
#include <QScopedValueRollback>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QWidgetAction>
#include <QVBoxLayout>
#include "OpenRGBVisualMapPlugin.h"
#include "VirtualControllerTab.h"
#include "VisualMapJsonDefinitions.h"
#include "VisualMapSettingsManager.h"
#include "WidgetEditor.h"
#include "ZoneManager.h"

VirtualControllerTab::VirtualControllerTab(QWidget *parent):
    QWidget(parent),
    ui(new Ui::VirtualControllerTab),
    virtual_controller(new VirtualController())
{
    ui->setupUi(this);

    /*-----------------------------------------------------*\
    | Default settings for main grid                        |
    \*-----------------------------------------------------*/
    settings                = new GridSettings();
    settings->w             = 64;
    settings->h             = 64;
    settings->show_bounds   = true;
    settings->show_grid     = true;
    settings->grid_size     = 1;
    settings->snap_to_grid  = false;

    active_state["ctrl_zones"] = json::array();

    /*-----------------------------------------------------*\
    | Init the grid                                         |
    \*-----------------------------------------------------*/
    ui->grid->Init();
    ui->gridOptions->Init(settings);
    ui->grid->ApplySettings(settings);

    virtual_controller->UpdateSize(settings->w, settings->h);

    /*-----------------------------------------------------*\
    | Init the grid                                         |
    \*-----------------------------------------------------*/
    InitZoneList();

    /*-----------------------------------------------------*\
    | Init other ui parts                                   |
    \*-----------------------------------------------------*/
    ui->itemFrame->hide();
    ui->backgroundApplier->SetSize(settings->w, settings->h);
    ui->backgroundFrame->hide();

    /*-----------------------------------------------------*\
    | Listen for virtual controller updates, redraw         |
    \*-----------------------------------------------------*/
    connect(this, &VirtualControllerTab::VirtualControllerPostUpdateSignal, this, &VirtualControllerTab::VirtualControllerPostUpdateSlot);

    virtual_controller->SetPostUpdateCallBack([&](const QImage& image)
    {
        emit VirtualControllerPostUpdateSignal(image);
    });

    /*-----------------------------------------------------*\
    | Init other ui parts                                   |
    \*-----------------------------------------------------*/
    UpdateVirtualControllerDetails();

    /*-----------------------------------------------------*\
    | Init Menu                                             |
    \*-----------------------------------------------------*/
    CreateMainMenu();
    autosave_timer = new QTimer(this);
    autosave_timer->setSingleShot(true);
    connect(autosave_timer, &QTimer::timeout, this, [this]()
    {
        if(!saved_filename.empty() && !VisualMapSettingsManager::SaveMap(saved_filename, active_state))
            LOG_ERROR("[OpenRGBVisualMapPlugin] Autosave failed for %s; live changes remain in memory", saved_filename.c_str());
    });
}

VirtualControllerTab::~VirtualControllerTab()
{
    delete virtual_controller;

    ZoneManager::Get()->FreeControllerZones(controller_zones);

    delete ui;
}

void VirtualControllerTab::CreateMainMenu()
{
    QMenu* main_menu = new QMenu(ui->main_menu);
    ui->main_menu->setMenu(main_menu);

    main_menu->setMaximumWidth(ui->main_menu->maximumWidth());

    register_controller = new QAction("Register controller", this);
    register_controller->setCheckable(true);
    connect(register_controller, &QAction::triggered, this, &VirtualControllerTab::RegisterAction);
    main_menu->addAction(register_controller);

    add_background = new QAction("Add background", this);
    add_background->setCheckable(true);
    connect(add_background, &QAction::triggered, this, &VirtualControllerTab::AddBackgroundAction);
    main_menu->addAction(add_background);

    QAction* save_vmap = new QAction("Save", this);
    connect(save_vmap, &QAction::triggered, this, &VirtualControllerTab::SaveVmapAction);
    main_menu->addAction(save_vmap);

    QAction* load_vmap = new QAction("Load", this);
    connect(load_vmap, &QAction::triggered, this, &VirtualControllerTab::LoadVmapAction);
    main_menu->addAction(load_vmap);

    QAction* clear = new QAction("Clear", this);
    connect(clear, &QAction::triggered, this, &VirtualControllerTab::ClearVmapAction);
    main_menu->addAction(clear);
}

void VirtualControllerTab::RenameController(std::string value)
{
    virtual_controller->SetName(value);
    emit ControllerRenamed(value);
}

std::string VirtualControllerTab::GetControllerName()
{
    return virtual_controller->GetName();
}

void VirtualControllerTab::resizeEvent(QResizeEvent*)
{
    ui->grid->update();
}

void VirtualControllerTab::UpdateVirtualControllerDetails()
{
    virtual_controller->UpdateVirtualZone();
    ui->virtual_controller_details_label->setText(QString::fromStdString("Total leds: " + std::to_string(virtual_controller->GetTotalLeds())));
}

void VirtualControllerTab::InitZoneList()
{
    ZoneManager::Get()->FreeControllerZones(controller_zones);

    controller_zones = ZoneManager::Get()->CopyControllerZones();

    ui->device_list->Init(controller_zones);
}

void VirtualControllerTab::LoadFile(std::string filename)
{
    json j = VisualMapSettingsManager::LoadMap(filename);

    RenameController(filename);

    LoadJson(j);
    saved_filename = filename;
}

void VirtualControllerTab::LoadJson(json j)
{
    QScopedValueRollback<bool> restore_guard(restoring, true);
    virtual_controller->Clear();

    active_state["ctrl_zones"] = json::array();

    if(j.contains("ctrl_zones"))
    {
        json ctrl_zones = j["ctrl_zones"];
        std::vector<ControllerZone*> all_zones = controller_zones;

        bool has_failures = false;

        for (json::iterator it = ctrl_zones.begin(); it != ctrl_zones.end(); ++it)
        {
            json entry = it.value();

            if(entry.contains("controller") && entry.contains("settings") && entry.contains("zone_idx"))
            {
                json controller = entry["controller"];
                json settings = entry["settings"];

                std::vector<ControllerZone*> candidates;

                for(ControllerZone* ctrl_zone : all_zones)
                {
                    if(controller.contains("name") && controller.contains("location") && controller.contains("vendor") && controller.contains("serial"))
                    {
                        /*---------------------------------*\
                        | Don't compare location for HID    |
                        | devices, because it constantly    |
                        | changes                           |
                        \*---------------------------------*/
                        bool hid_location = std::string(controller["location"]).find("HID: ") == 0;

                        if(
                            ctrl_zone->controller->GetName() == controller["name"] &&
                            ctrl_zone->controller->GetVendor() == controller["vendor"] &&
                            ctrl_zone->controller->GetSerial() == controller["serial"] &&
                            (ctrl_zone->controller->GetLocation() == controller["location"] || hid_location) &&
                            ctrl_zone->zone_idx == entry["zone_idx"])
                        {
                            if(entry.contains("custom_zone_name"))
                            {
                                ctrl_zone->custom_zone_name = entry["custom_zone_name"];
                            }

                            ctrl_zone->settings = settings;

                            candidates.push_back(ctrl_zone);
                        }
                    }
                }

                ControllerZone* zone = nullptr;
                if (candidates.size() > 1)
                {
                    /*-------------------------------------*\
                    | If we found more than onne zone       |
                    | matching the saved one then fall back |
                    | to comparing by location even for HID |
                    | devices.                              |
                    | This is a workaround to prevent us    |
                    | from loading the wrong device when    |
                    | multiple HID devices have the same    |
                    | name, vendor, serial, and zone_idx.   |
                    | It won't work 100% of the time, but   |
                    | it will work in some situations       |
                    \*-------------------------------------*/
                    for (ControllerZone* z : candidates)
                    {
                        if (controller.contains("location") && (z->controller->GetLocation() == controller["location"]))
                        {
                            zone = z;
                            break;
                        }
                    }
                }
                else if (candidates.size() == 1)
                {
                    zone = candidates[0];
                }

                /*-----------------------------------------*\
                | An entry with no identity can never bind  |
                \*-----------------------------------------*/
                if(!zone
                && controller.contains("name")
                && controller.contains("vendor")
                && controller.contains("serial")
                && controller.contains("location"))
                {
                    AddActiveEntry(entry);
                }

                try
                {
                    if (zone)
                    {
                        AddActiveZone(zone);
                        virtual_controller->Add(zone);
                        ui->device_list->UpdateControllerState(zone);
                        /*---------------------------------*\
                        | Remove this zone from the list of |
                        | all zones so that we don't modify |
                        | its custom name or settings if    |
                        | there's another collision.        |
                        \*---------------------------------*/
                        all_zones.erase(std::find(all_zones.begin(), all_zones.end(), zone));
                    }
                }
                catch (const std::exception&)
                {
                    has_failures = true;
                }
            }
        }

        QPoint button_pos = ui->main_menu->cursor().pos();

        if(has_failures)
        {
            QMessageBox msgBox;
            msgBox.setText("Some of the components could not be loaded, the format is probably out of date.");
            msgBox.setWindowTitle("Sorry");
            msgBox.move(button_pos.x(), button_pos.y());
            msgBox.exec();
        }

        if(j.contains("grid_settings"))
        {
            j.at("grid_settings").get_to(settings);
            active_state["grid_settings"] = j["grid_settings"];
        }

        ui->gridOptions->SetSettings(settings);

        ui->grid->ResetItems(virtual_controller->GetZones());

        virtual_controller->UpdateSize(settings->w, settings->h);

        UpdateVirtualControllerDetails();
    }
}

void VirtualControllerTab::ApplyAutoRegister()
{
    if(settings->auto_register)
    {
        register_controller->setChecked(true);
        RegisterAction();
    }
}

void VirtualControllerTab::on_backgroundApplier_BackgroundUpdated(const QImage& image)
{
    virtual_controller->ApplyImage(image);
}

void VirtualControllerTab::VirtualControllerPostUpdateSlot(const QImage& image)
{
    ui->grid->UpdatePreview(image);
}

void VirtualControllerTab::Hide()
{
    virtual_controller->Register(false, false);
}

void VirtualControllerTab::Recreate()
{
    /*-----------------------------------------------------*\
    | Drop the zones before InitZoneList frees them         |
    \*-----------------------------------------------------*/
    CaptureActiveState();
    virtual_controller->Clear();

    Clear();
    InitZoneList();
    ReassignZones();

    if(register_controller->isChecked())
    {
        virtual_controller->Register(true, settings->hide_members);
    }
}

void VirtualControllerTab::CaptureActiveState()
{
    // Cached controller identities are still usable after a device disconnects.
    // Merge live edits, retaining entries that cannot currently be rebound.
    for(ControllerZone* zone : virtual_controller->GetZones()) AddActiveZone(zone);
    active_state["grid_settings"] = settings;
    if(autosave_timer && !restoring && !saved_filename.empty()) autosave_timer->start(300);
}

void VirtualControllerTab::ReassignZones()
{
    LoadJson(active_state);
}

void VirtualControllerTab::PauseForDetection()
{
    CaptureActiveState();
    virtual_controller->Clear();
}

void VirtualControllerTab::AddActiveZone(ControllerZone* added_zone)
{
    AddActiveEntry(json(added_zone));
}

void VirtualControllerTab::AddActiveEntry(json added_zone_json)
{
    bool found = false;

    for(std::size_t saved_zone_idx = 0; saved_zone_idx < active_state["ctrl_zones"].size(); saved_zone_idx++)
    {
        json saved_zone_json = active_state["ctrl_zones"][saved_zone_idx];

        /*-------------------------------------------------*\
        | Don't compare location for HID devices, because   |
        | it constantly changes                             |
        \*-------------------------------------------------*/
        bool hid_location = std::string(saved_zone_json["controller"]["location"]).find("HID: ") == 0;

        if((added_zone_json["controller"]["name"] == saved_zone_json["controller"]["name"]) &&
            (added_zone_json["controller"]["vendor"] == saved_zone_json["controller"]["vendor"]) &&
            (added_zone_json["controller"]["serial"] == saved_zone_json["controller"]["serial"]) &&
            ((added_zone_json["controller"]["location"] == saved_zone_json["controller"]["location"]) || hid_location) &&
            (added_zone_json["zone_idx"] == saved_zone_json["zone_idx"]))
        {
            active_state["ctrl_zones"][saved_zone_idx] = added_zone_json;
            found = true;
            break;
        }
    }

    if(!found)
    {
        active_state["ctrl_zones"].push_back(added_zone_json);
    }
}

void VirtualControllerTab::RemoveActiveZone(ControllerZone* removed_zone)
{
    bool        found               = false;
    json        removed_zone_json   = removed_zone;
    std::size_t saved_zone_idx      = 0;

    for(/*saved_zone_idx*/; saved_zone_idx < active_state["ctrl_zones"].size(); saved_zone_idx++)
    {
        json saved_zone_json = active_state["ctrl_zones"][saved_zone_idx];

        /*-------------------------------------------------*\
        | Don't compare location for HID devices, because   |
        | it constantly changes                             |
        \*-------------------------------------------------*/
        bool hid_location = std::string(saved_zone_json["controller"]["location"]).find("HID: ") == 0;

        if((removed_zone_json["controller"]["name"] == saved_zone_json["controller"]["name"]) &&
            (removed_zone_json["controller"]["vendor"] == saved_zone_json["controller"]["vendor"]) &&
            (removed_zone_json["controller"]["serial"] == saved_zone_json["controller"]["serial"]) &&
            ((removed_zone_json["controller"]["location"] == saved_zone_json["controller"]["location"]) || hid_location) &&
            (removed_zone_json["zone_idx"] == saved_zone_json["zone_idx"]))
        {
            found = true;
            break;
        }
    }

    if(found)
    {
        active_state["ctrl_zones"].erase(active_state["ctrl_zones"].begin() + saved_zone_idx);
    }
}

void VirtualControllerTab::Clear()
{
    ui->device_list->Clear();
    ui->grid->Clear();

    UpdateItemOptions({});

    selected_ctrl_zone = nullptr;
}

void VirtualControllerTab::UpdateItemOptions(std::vector<ControllerZone*> selected_controller_zones)
{
    if(selected_controller_zones.size() == 1)
    {
        ui->itemOptions->SetControllerZone(selected_controller_zones[0]);
        ui->itemFrame->show();
    }
    else
    {
        ui->itemOptions->SetControllerZone(nullptr);
        ui->itemFrame->hide();
    }
}

/*---------------------------------------------------------*\
| UI element signals                                        |
\*---------------------------------------------------------*/
void VirtualControllerTab::on_device_list_DeviceAdded(ControllerZone* controller_zone)
{
    AddActiveZone(controller_zone);
    virtual_controller->Add(controller_zone);
    UpdateVirtualControllerDetails();
    CaptureActiveState();
    ui->grid->ResetItems(virtual_controller->GetZones());
}


void VirtualControllerTab::on_device_list_DeviceRemoved(ControllerZone* controller_zone)
{
    RemoveActiveZone(controller_zone);
    virtual_controller->Remove(controller_zone);
    UpdateVirtualControllerDetails();
    CaptureActiveState();
    ui->grid->ResetItems(virtual_controller->GetZones());
}

void VirtualControllerTab::on_device_list_SelectionChanged(std::vector<ControllerZone*> selected_controller_zones)
{
    ui->grid->SetSelection(selected_controller_zones);
    UpdateItemOptions(selected_controller_zones);
}

void VirtualControllerTab::on_grid_SelectionChanged(std::vector<ControllerZone*> selected_controller_zones)
{
    ui->device_list->SetSelection(selected_controller_zones);
    UpdateItemOptions(selected_controller_zones);
}

void VirtualControllerTab::on_itemOptions_ItemOptionsChanged()
{
    virtual_controller->UpdateVirtualZone();
    ui->grid->UpdateItems();
    CaptureActiveState();
}

void VirtualControllerTab::on_grid_Changed()
{
    virtual_controller->UpdateVirtualZone();
    ui->itemOptions->Update();
    CaptureActiveState();
}

void VirtualControllerTab::on_itemOptions_ShapeEditRequest(ControllerZone* controller_zone)
{
    if(controller_zone)
    {
        int result = WidgetEditor::Show(controller_zone, controller_zones);

        if(result)
        {
            on_itemOptions_ItemOptionsChanged();
        }
    }
}

void VirtualControllerTab::on_gridOptions_SettingsChanged()
{
    ui->grid->ApplySettings(settings);
    ui->backgroundApplier->SetSize(settings->w, settings->h);
    virtual_controller->UpdateSize(settings->w, settings->h);
    CaptureActiveState();
}

void VirtualControllerTab::on_gridOptions_AutoResizeRequest()
{
    /*-----------------------------------------------------*\
    | Do nothing if the controller is empty                 |
    \*-----------------------------------------------------*/
    if(virtual_controller->IsEmpty())
    {
        return;
    }

    /*-----------------------------------------------------*\
    | Calculate bounds                                      |
    \*-----------------------------------------------------*/
    qreal min_x = std::numeric_limits<qreal>::max();
    qreal min_y = std::numeric_limits<qreal>::max();
    qreal max_x = std::numeric_limits<qreal>::lowest();
    qreal max_y = std::numeric_limits<qreal>::lowest();

    for (ControllerZone* controller_zone: virtual_controller->GetZones())
    {
        min_x = std::min(min_x, controller_zone->settings.x);
        min_y = std::min(min_y, controller_zone->settings.y);

        max_x = std::max(max_x, controller_zone->settings.x + controller_zone->width());
        max_y = std::max(max_y, controller_zone->settings.y + controller_zone->height());
    }

    const qreal origin_x = std::floor(min_x);
    const qreal origin_y = std::floor(min_y);

    /*-----------------------------------------------------*\
    | Shift all controllers                                 |
    \*-----------------------------------------------------*/
    for (ControllerZone* controller_zone: virtual_controller->GetZones())
    {
        controller_zone->settings.x -= origin_x;
        controller_zone->settings.y -= origin_y;
    }

    /*-----------------------------------------------------*\
    | Resize the map                                        |
    \*-----------------------------------------------------*/
    settings->w = std::ceil(max_x) - origin_x;
    settings->h = std::ceil(max_y) - origin_y;

    /*-----------------------------------------------------*\
    | Update GUI elements                                   |
    \*-----------------------------------------------------*/
    ui->grid->ApplySettings(settings);
    ui->grid->UpdateItems();
    ui->gridOptions->SetSettings(settings);
}

/*---------------------------------------------------------*\
| Main menu actions                                         |
\*---------------------------------------------------------*/
void VirtualControllerTab::RegisterAction()
{
    virtual_controller->Register(register_controller->isChecked(), settings->hide_members);
}

void VirtualControllerTab::AddBackgroundAction()
{
    ui->backgroundFrame->setVisible(add_background->isChecked());
}

void VirtualControllerTab::ClearVmapAction()
{
    for(ControllerZone* ctrl_zone: virtual_controller->GetZones())
    {
        ctrl_zone->settings = ControllerZoneSettings::defaults();
    }

    virtual_controller->Clear();
    active_state["ctrl_zones"] = json::array();
    CaptureActiveState();

    ui->grid->ResetItems(virtual_controller->GetZones());

    ui->itemOptions->Update();
}

void VirtualControllerTab::SaveVmapAction()
{
    QString filename = QInputDialog::getText(
                           nullptr, "Save virtual controller", "Choose a filename",
                           QLineEdit::Normal, QString::fromUtf8(GetControllerName().c_str())).trimmed();

    if(!filename.isEmpty())
    {
        RenameController(filename.toStdString());

        json j;

        CaptureActiveState();
        j = active_state;
        if(!VisualMapSettingsManager::SaveMap(filename.toStdString(), j))
        {
            QMessageBox::warning(this, tr("Save visual map"),
                tr("The map could not be saved. Check that the configuration folder is writable. Your changes are still in memory."));
        }
        else
        {
            saved_filename = filename.toStdString();
        }
    }
}

void VirtualControllerTab::LoadVmapAction()
{
    QStringList file_list;

    std::vector<std::string> filenames = VisualMapSettingsManager::GetMapNames();

    for(const std::string& filename : filenames)
    {
        file_list << QString::fromUtf8(filename.c_str());
    }

    QInputDialog *inp = new QInputDialog(this);

    inp->setOptions(QInputDialog::UseListViewForComboBoxItems);
    inp->setComboBoxItems(file_list);
    inp->setWindowTitle("Choose file");

    if(!inp->exec())
    {
        return;
    }

    QString filename = inp->textValue();

    LoadFile(filename.toStdString());
    ApplyAutoRegister();
}
