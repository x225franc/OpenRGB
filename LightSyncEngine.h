#pragma once
#include <QSet>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QTcpServer>
#include <QElapsedTimer>
#include <QFile>
#include <memory>
#include <mutex>
#include <vector>
#include "OpenRGBPluginInterface.h"

struct LightSyncSettings
{
    bool enabled = true, offOnSuspend = false, onOnResume = true, interpolate = true, laptop = true;
    int darkBelow = 1, fadeMs = 100, goveeFps = 25, yeelightFps = 10;
};
struct LightSyncDevice
{
    QString id, kind, model, name, ip;
    QHostAddress local;
    int port = 55443, count = 1;
    QJsonObject discoveryData;
    RGBControllerInterface* controller = nullptr;
    RGBController_Setup setup{};
    std::mutex mutex;
    std::vector<RGBColor> latest;
    int brightness = 100;
    quint64 revision = 0;
    bool enabled = true;
    QString status = "Discovered", reportedStatus;
    // Worker-owned transport state, never accessed by effect callbacks.
    QTcpSocket* control = nullptr;
    QTcpSocket* music = nullptr;
    QTcpServer* listener = nullptr;
    bool streaming = false, submitted = false, dark = false, externallyOff = false;
    QByteArray controlRead;
    int phase = 0, failures = 0;
    qint64 lastSend = 0, nextAttempt = 0, phaseAt = 0, fadeAt = -1, lastSeen = 0;
    int previousBright = 1, fadeBright = 1;
    RGBColor previousColor = 0;
    std::vector<double> smooth;
    std::vector<RGBColor> sent;
    quint64 sentRevision = 0;

};
class LampArrayBackend;
class LightSyncEngine : public QObject
{
    Q_OBJECT
public:
    explicit LightSyncEngine(OpenRGBPluginAPIInterface*, QString, QJsonArray cached = {});
    ~LightSyncEngine();
    static QByteArray GoveePacket(const std::vector<RGBColor>&);
    static QByteArray RazerPacket(QByteArray);
    static QJsonObject ParseYeelight(const QByteArray&);
public slots:
    void start();
    void scan();
    void configure(LightSyncSettings);
    void setDeviceEnabled(QString, bool);
    void changeAddress(QString, QString);
    void forgetDevice(QString);
    void searchAgain();
    void restart();
    void suspend(bool);
    void recoverKeyboard(QString reason);
    void shutdown();
signals:
    void inventory(QJsonArray);
    void message(QString);
    void initialized();
    void shutdownComplete();
private:
    friend struct LightSyncEngineTestAccess;
    void refreshDiscovery();
    void tick();
    void receive(QUdpSocket*, QHostAddress, bool);
    void found(QJsonObject);
    void createController(const std::shared_ptr<LightSyncDevice>&);
    static void Update(void*);
    static void UpdateZone(void*, int);
    static void UpdateLED(void*, int);
    void release(LightSyncDevice&);
    void govee(LightSyncDevice&, const std::vector<RGBColor>&, int, qint64);
    void yeelight(LightSyncDevice&, RGBColor, int, qint64);
    void command(LightSyncDevice&, QString, QJsonArray);
    void power(LightSyncDevice&, bool);
    void publish();
    void log(QString);
    OpenRGBPluginAPIInterface* api;
    QString directory;
    LightSyncSettings settings;
    QJsonArray cachedDevices;
    qint64 lastTick = 0, lastResume = -1, lastKeyboardRecovery = -1;
    QTimer* timer = nullptr;
    QTimer* heartbeat = nullptr;
    std::vector<QUdpSocket*> discovery;
    std::vector<std::shared_ptr<LightSyncDevice>> devices;
    std::vector<std::shared_ptr<LightSyncDevice>> retired;
    QSet<QString> forgotten;
    QString networkSignature;
    QUdpSocket* sender = nullptr;
    QElapsedTimer clock;
    QFile logfile;
    bool stopped = false, sleeping = false;
    int commandId = 1;
    LampArrayBackend* lamps = nullptr;
};
Q_DECLARE_METATYPE(LightSyncSettings)

