#pragma once

#include "sdk/IPhysicalPlugin.h"

#include <QMutex>
#include <QThread>

#include <atomic>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winusb.h>
#endif

class CandleLightCanFdPlugin : public IPhysicalPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IPhysicalPlugin_iid FILE "candlelight_canfd.json")
    Q_INTERFACES(IPhysicalPlugin)

public:
    explicit CandleLightCanFdPlugin(QObject* parent = nullptr);
    ~CandleLightCanFdPlugin() override;

    bool open(const QVariantMap& config) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray& data) override;

    QString name() const override;
    QString version() const override;
    QVariantMap defaultConfig() const override;

private:
    struct BitTiming {
        quint32 prop = 0;
        quint32 seg1 = 0;
        quint32 seg2 = 0;
        quint32 sjw = 0;
        quint32 brp = 0;
    };

    static int parseInt(const QVariant& value, int fallback);
    static bool parseBool(const QVariant& value, bool fallback);
    static int canFdLengthToDlc(int length);
    static int canFdDlcToLength(int dlc);

    QByteArray buildProjectCanFrame(quint32 canId, const QByteArray& payload) const;
    QByteArray buildUsbFrame(const QByteArray& projectFrame);
    void readLoop();

#if defined(Q_OS_WIN)
public:
    struct TimeLimits {
        quint32 seg1Min = 1;
        quint32 seg1Max = 256;
        quint32 seg2Min = 1;
        quint32 seg2Max = 128;
        quint32 sjwMax = 128;
        quint32 brpMin = 1;
        quint32 brpMax = 1024;
        quint32 brpInc = 1;
    };

    struct Capabilities {
        quint32 feature = 0;
        quint32 fclkCan = 80000000;
        TimeLimits nominal;
        TimeLimits data;
        bool supportsFd = false;
    };

private:
    bool openWinUsb(const QVariantMap& config);
    bool initializeDevice(const QVariantMap& config);
    bool setBitTiming(quint8 request, const BitTiming& timing);
    bool setDeviceMode(quint32 mode, quint32 flags);
    bool controlTransfer(quint8 request, quint8 direction, quint16 value, void* data, quint32 size, quint32* transferred = nullptr);
    bool readCapabilities();
    bool solveBitTiming(int bitrate, int samplePointPermille, const TimeLimits& limits, BitTiming* timing) const;
    void parseUsbPacket(const QByteArray& packet);
    QString lastWindowsError(const QString& prefix) const;

    HANDLE m_device = INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE m_usb = nullptr;
    quint8 m_epIn = 0x81;
    quint8 m_epOut = 0x02;
    quint8 m_interfaceNumber = 0;
    quint8 m_channel = 0;
    Capabilities m_caps;
#endif

    std::atomic_bool m_open = false;
    std::atomic_bool m_running = false;
    QThread* m_worker = nullptr;
    mutable QMutex m_ioMutex;
    quint8 m_echoId = 0;
    bool m_forceFd = true;
    bool m_brs = true;
    quint32 m_timeoutMs = 500;
};
