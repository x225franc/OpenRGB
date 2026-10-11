/*---------------------------------------------------------*\
| VisualMapSettingsManager.cpp                              |
|                                                           |
|   Settings management for Visual Map Plugin               |
|                                                           |
|   This file is part of the OpenRGB Visual Map Plugin      |
|   project                                                 |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <fstream>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QString>
#include "OpenRGBVisualMapPlugin.h"
#include "VisualMapSettingsManager.h"

bool VisualMapSettingsManager::SaveMap(std::string filename, json j)
{
    if(!CreateSettingsDirectory())
    {
        return false;
    }

    if(!CreateMapsDirectory())
    {
        return false;
    }

    return write_file(MapsFolder() / filename, j);
}

json VisualMapSettingsManager::LoadMap(std::string filename)
{
    json j;

    if(!CreateSettingsDirectory())
    {
        return j;
    }

    if(!CreateMapsDirectory())
    {
        return j;
    }

    return load_json_file(MapsFolder() / filename);
}

std::vector<std::string> VisualMapSettingsManager::GetMapNames()
{
    return list_files(MapsFolder());
}

bool VisualMapSettingsManager::SaveGradient(std::string filename, json j)
{
    if(!CreateSettingsDirectory())
    {
        return false;
    }

    if(!CreateGradientsDirectory())
    {
        return false;
    }

    return write_file(GradientsFolder() / filename, j);
}

json VisualMapSettingsManager::LoadGradient(std::string filename)
{
    json j;

    if(!CreateSettingsDirectory())
    {
        return j;
    }

    if(!CreateMapsDirectory())
    {
        return j;
    }

    return load_json_file(GradientsFolder() / filename);
}

std::vector<std::string> VisualMapSettingsManager::GetGradientsNames()
{
    return list_files(GradientsFolder());
}

bool VisualMapSettingsManager::CreateSettingsDirectory()
{
    return create_dir(SettingsFolder());
}

bool VisualMapSettingsManager::CreateMapsDirectory()
{
    return create_dir(MapsFolder());
}

bool VisualMapSettingsManager::CreateGradientsDirectory()
{
    return create_dir(GradientsFolder());
}

filesystem::path VisualMapSettingsManager::SettingsFolder()
{
    return OpenRGBVisualMapPlugin::api->GetConfigurationDirectory() / "plugins" / "settings";
}

filesystem::path VisualMapSettingsManager::MapsFolder()
{
    return SettingsFolder() / "virtual-controllers";
}

filesystem::path VisualMapSettingsManager::GradientsFolder()
{
    return SettingsFolder() / "gradients";
}

bool VisualMapSettingsManager::write_file(filesystem::path file_name, json j)
{
    // Commit by replacing the original only after a complete successful write.
    QSaveFile file(QString::fromStdWString(file_name.wstring()));
    if(!file.open(QIODevice::WriteOnly))
    {
        LOG_ERROR("[OpenRGBVisualMapPlugin] Cannot open settings file: %s", file.errorString().toStdString().c_str());
        return false;
    }
    try
    {
        const std::string data = j.dump(4);
        if(file.write(data.data(), static_cast<qint64>(data.size())) != static_cast<qint64>(data.size()))
            return false;
        if(!file.commit())
        {
            LOG_ERROR("[OpenRGBVisualMapPlugin] Cannot commit settings file: %s", file.errorString().toStdString().c_str());
            return false;
        }
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGBVisualMapPlugin] Cannot write file: %s", e.what());
        return false;
    }
}

json VisualMapSettingsManager::load_json_file(filesystem::path file_name)
{
    json j;

    std::ifstream file(file_name);

    if(file)
    {
        try
        {
            file >> j;
            file.close();
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("[OpenRGBVisualMapPlugin] Cannot read file: %s\n", e.what());
        }
    }

    return j;
}

std::vector<std::string> VisualMapSettingsManager::list_files(filesystem::path path)
{
    std::vector<std::string> filenames;

    QDir dir(QString::fromStdString(path.string()));

    if(dir.exists())
    {
        for (const QString & entry : dir.entryList(QDir::Files))
        {
            filenames.push_back(entry.toStdString());
        }
    }

    /*-----------------------------------------------------*\
    | Alphabetical sort                                     |
    \*-----------------------------------------------------*/
    std::sort(filenames.begin(), filenames.end());

    return filenames;
}

bool VisualMapSettingsManager::create_dir(filesystem::path directory)
{
    QDir dir(QString::fromStdString(directory.string()));

    if(dir.exists())
    {
        return true;
    }

    return QDir().mkpath(dir.path());
}
