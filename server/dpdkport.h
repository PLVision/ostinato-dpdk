#ifndef __DPDKPort_H__
#define __DPDKPort_H__

#include <QtGlobal>
#include <QTemporaryFile>
#include <QBuffer>
#include <QThread>
#include <pcap.h>

#include "dpdk_api.h"
#include "abstractport.h"

class DPDKPort : public AbstractPort
{
    uint8_t             portId;

protected:
    enum XThreadState
    {
        XTS_SUSPEND,
        XTS_RUN,
        XTS_DONE
    };

public:
    DPDKPort(int id, uint8_t devId, const char *device);
    virtual ~DPDKPort();

    void init();

    virtual OstProto::LinkState linkState();
    virtual bool        hasExclusiveControl();
    virtual bool        setExclusiveControl(bool exclusive);

protected:
    bool                isPromiscModeOn;
    bool                clearPromiscOnExit;

    // Statistics rrelated functionality
protected:
    class StatsMonitor: public QThread
    {
    private:
        uint8_t         portId;
        PortStats*      stats;
        XThreadState    state;
        
    public:
        StatsMonitor(uint8_t devId);
        ~StatsMonitor();
        void            run();
        void            stop();
        inline bool     isRunning() { return (state == XTS_RUN);}
        bool            waitForSetupFinished(int msecs = 10000);
        inline void     setStatsReference(PortStats* pStats){ stats = pStats; }
    private:
        static const int refreshFreq = 1; // in seconds
        bool            statsMonStop;
        bool            statsMonSetupDone;
    };
    StatsMonitor        monitor;

    // Tx related functionality
public:
    virtual void        startTransmit();
    virtual void        stopTransmit();
    virtual bool        isTransmitOn();

    virtual void        clearPacketList();
    virtual void        loopNextPacketSet(qint64 size, qint64 repeats, long repeatDelaySec, long repeatDelayNsec);
    virtual bool        appendToPacketList(long sec, long nsec, const uchar *packet, int length);
    virtual void        setPacketListLoopMode(bool loop, quint64 secDelay, quint64 nsecDelay);
    virtual void        updatePacketList();

    // Rx related functionality
public:
    virtual void        startCapture();
    virtual void        stopCapture();
    virtual bool        isCaptureOn();
    virtual QIODevice*  captureData();

protected:
    XThreadState        rxState;
    XThreadState        txState;
    QTemporaryFile      tmpFile;
    QBuffer             tmpBuffer;

    pcap_t              *pHandle;
    pcap_dumper_t       *pDumper;
    char                *captureBuffer;
};

#endif //__DPDKPort_H__
