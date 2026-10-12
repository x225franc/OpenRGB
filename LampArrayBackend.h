#pragma once
#include <QString>
#include <QPointF>
#include <vector>
#include <functional>
#include <memory>
#include "RGBControllerInterface.h"
class LampArrayBackend
{
public:
    struct Device { QString id, name; std::vector<QPointF> positions; };
    LampArrayBackend();
    ~LampArrayBackend();
    std::vector<Device> discover();
    QString send(QString, const std::vector<RGBColor>&, int);
    void release();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
