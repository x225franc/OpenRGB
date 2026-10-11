#include <QApplication>
#include <QPluginLoader>
#include <iostream>
#include "OpenRGBPluginInterface.h"
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if(argc != 3) return 2;
    for(int i=1; i<argc; i++)
    {
        QPluginLoader loader(QString::fromLocal8Bit(argv[i]));
        auto* plugin = qobject_cast<OpenRGBPluginInterface*>(loader.instance());
        if(!plugin) { std::cerr << "FAIL " << loader.errorString().toStdString() << '\n'; return 1; }
        if(plugin->GetPluginAPIVersion() != OPENRGB_PLUGIN_API_VERSION) return 1;
        std::cout << "PASS " << argv[i] << ": Qt " << qVersion()
                  << ", API " << plugin->GetPluginAPIVersion() << '\n';
        loader.unload();
    }
    return 0;
}
