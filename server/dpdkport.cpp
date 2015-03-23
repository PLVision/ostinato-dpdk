#include "dpdkport.h"

#include <QByteArray>
#include <QHash>
#include <QTime>

#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include <linux/rtnetlink.h>

#include "../common/streambase.h"
#include "../common/abstractprotocol.h"

#include "dpdk_api.h"

#define SEC_TO_NSEC 1e9
#define MSEC_TO_NSEC 1e3

#define CAPTURE_BUFFER_SIZE 1000000000

/**
 * @brief           DPDKPort constructor
 * 
 * @param id        int, Ostinato port number
 * @param devId     uint8_t, DPDK device ID
 * @param device    const char*, Name of the device
 */
DPDKPort::DPDKPort(int id, uint8_t devId, const char* device) :
    AbstractPort(id, device),
    portId(devId),
    monitor(devId)
{
    monitor.setStatsReference(&stats_);
    
    if(!tmpFile.open())
        qWarning("Unable to open temp file for RX");
    
    isPromiscModeOn = true;
    clearPromiscOnExit = false;

    data_.set_is_exclusive_control(hasExclusiveControl());

    if(!dpdk_init_port(portId))
    {
        return;
    }

    captureBuffer = new char[CAPTURE_BUFFER_SIZE];

    dpdk_reset_dev_stats(portId);

    if(isPromiscModeOn && !dpdk_set_dev_promisc(portId, true))
        return;

    maxStatsValue_ = 0xffffffff;
    
    rxState = XTS_SUSPEND;
}

/**
 * @brief           DPDKPort destructor
 */
DPDKPort::~DPDKPort()
{
    qDebug("In %s", __FUNCTION__);

    stopTransmit();
    
    if(monitor.isRunning())
    {
        monitor.stop();
        monitor.wait();
    }
    
    if(clearPromiscOnExit)
        dpdk_set_dev_promisc(portId, false);
    
    dpdk_stop_dev(portId);
    
    tmpFile.close();

    if (captureBuffer)
    {
        delete[] captureBuffer;
    }
}

/**
 * @brief           Initialize DPDKPort and start the device
 */
void DPDKPort::init()
{
    // Start device
    if(0 != dpdk_start_dev(portId))
        return;

    if(!monitor.isRunning())
        monitor.start();

    monitor.waitForSetupFinished();

    if(!isPromiscModeOn)
        addNote("Non Promiscuous mode");
}

/**
 * @brief           Retrieve DPDK device link state and translate it to Ostinato client
 * 
 * @return          OstProto::LinkState, device current link state
 */
OstProto::LinkState DPDKPort::linkState()
{
    int linkStatus = 0;

    linkState_ = OstProto::LinkStateUnknown;

    if(dpdk_get_dev_link_status(portId, &linkStatus))
        linkState_ = linkStatus ? OstProto::LinkStateUp : OstProto::LinkStateDown;

    return linkState_;
}

/**
 * @brief           Get device exclusive control state
 * 
 * @return          true if device is exclusive controlled and false otherwice
 */
bool DPDKPort::hasExclusiveControl()
{
    // TODO
    qDebug("Get control...");

    return false;
}

/**
 * @brief           Set device exclusive control state
 * 
 * @return          true if success and false otherwice
 */
bool DPDKPort::setExclusiveControl(bool /*exclusive*/)
{
    // TODO
    qDebug("Set control...");
    
    return false;
}

/**
 * @brief           Get capture state
 * 
 * @return          true if enabled and false otherwice
 */
bool DPDKPort::isCaptureOn()
{
    return (rxState == XTS_RUN);
}

/**
 * @brief           Start data capturing
 */
void DPDKPort::startCapture()
{
    if(rxState == XTS_RUN)
    {
        qWarning("Receiver already started");
        return;
    }
    
    rxState = XTS_SUSPEND;
    
    pHandle = pcap_open_dead(DLT_EN10MB, 65535);
    pDumper = pcap_dump_open(pHandle, tmpFile.fileName().toStdString().c_str());

    if(captureBuffer)
    {
        dpdk_start_rx(portId, captureBuffer, CAPTURE_BUFFER_SIZE);
    }

    rxState = XTS_RUN;
}

/**
 * @brief           Stop data capturing
 */
void DPDKPort::stopCapture()
{
    if(rxState != XTS_RUN)
    {
        qWarning("Receiver already stopped");
        return;
    }

    quint32 captureDataSize;

    dpdk_stop_rx(portId, &captureDataSize);

    uint32_t offset = 0;
    struct pcap_pkthdr *pHdr = NULL;
    u_char *data = NULL;

    while (offset < captureDataSize)
    {
        pHdr = (struct pcap_pkthdr *)(captureBuffer + offset);
        offset += sizeof(struct pcap_pkthdr);

        data = (u_char *)(captureBuffer + offset);
        offset += pHdr->len;

        pcap_dump((u_char*)pDumper, pHdr, data);
    }
    
    pcap_dump_close(pDumper);
    pcap_close(pHandle);

    rxState = XTS_DONE;
}

/**
 * @brief           Get captured data
 * 
 * @return          QIODevice*, pointer to QIODevice to send captured data to
 */
QIODevice* DPDKPort::captureData()
{
    return &tmpFile;
}

/**
 * @brief           Get transmit state
 * 
 * @return          true if enabled and false otherwice
 */
bool DPDKPort::isTransmitOn()
{
    return (txState == XTS_RUN);
}

/**
 * @brief           Start data transmitting
 */
void DPDKPort::startTransmit()
{
    if(txState == XTS_RUN)
    {
        qCritical("Transmit is already running on port %u", portId);
        return;
    }

    qDebug("Start transmitting on port %u", portId);

    dpdk_start_tx(portId);
    txState = XTS_RUN;
}

/**
 * @brief           Stop data transmitting
 */
void DPDKPort::stopTransmit()
{
    if(txState != XTS_RUN)
    {
        qCritical("Transmit is not running on port %u", portId);
        return;
    }

    qDebug("Stop transmitting on port %u", portId);

    dpdk_stop_tx(portId);
    txState = XTS_DONE;
}

bool DPDKPort::appendToPacketList(long sec, long nsec, const uchar* packet, int length)
{
}

void DPDKPort::clearPacketList()
{
}

void DPDKPort::setPacketListLoopMode(bool loop, quint64 secDelay, quint64 nsecDelay)
{
}

void DPDKPort::loopNextPacketSet(qint64 size, qint64 repeats, long repeatDelaySec, long repeatDelayNsec)
{
}

/**
 * @brief           DPDKPort statistics monitor constructor
 */
DPDKPort::StatsMonitor::StatsMonitor(uint8_t devId)
    : QThread()
    , portId(devId)
    , stats(NULL)
    , state(XTS_SUSPEND)
{
    statsMonStop = false;
    statsMonSetupDone = false;
}

/**
 * @brief           DPDKPort statistics monitor destructor
 */
DPDKPort::StatsMonitor::~StatsMonitor()
{
}

/**
 * @brief           DPDKPort statistics monitor thread main routine
 */
void DPDKPort::StatsMonitor::run()
{
    state = XTS_SUSPEND;
    // Nothing to update
    if(!stats)
        return;
    
    dpdk_stats_t devStat;
    
    struct timeval tvStart, tvEnd;
    gettimeofday(&tvStart, NULL);
    quint64       rxP = 0, rxB = 0, txP = 0, txB = 0;
    
    state = XTS_RUN;
    while(!statsMonStop)
    {
        dpdk_get_dev_stats(portId, &devStat);
        
        stats->rxPkts = devStat.ipackets;
        stats->rxBytes = devStat.ibytes;
        stats->rxDrops = devStat.imissed;
        stats->rxErrors = devStat.ierrors;
        stats->rxFrameErrors = devStat.ibadlen + devStat.ibadcrc;
        stats->rxFifoErrors = devStat.rx_nombuf;

        stats->txPkts = devStat.opackets;
        stats->txBytes = devStat.obytes;

        gettimeofday(&tvEnd, NULL);
        quint64 secs = (tvEnd.tv_usec - tvStart.tv_usec) + (tvEnd.tv_sec - tvStart.tv_sec) * 1e6;
        
        stats->rxPps = 1e6 * (devStat.ipackets - rxP) / secs;
        stats->rxBps = 1e6 * (devStat.ibytes - rxB) / secs;

        stats->txPps = 1e6 * (devStat.opackets - txP) / secs;
        stats->txBps = 1e6 * (devStat.obytes - txB) / secs;

        //Save data
        rxP = devStat.ipackets;
        rxB = devStat.ibytes;
        txP = devStat.opackets;
        txB = devStat.obytes;
        
        tvStart = tvEnd;
        if(!statsMonStop)
            QThread::msleep(1000);
    }
    
    state = XTS_DONE;
}

/**
 * @brief           Stop port statistics monitor
 */
void DPDKPort::StatsMonitor::stop()
{
    statsMonStop = true;
}

/**
 * @brief           Pause port statistic monitor start before it will be fully configured
 * 
 * @param msecs     int, sleep time in milliseconds
 * 
 * @return          return true if setup is not done and false otherwice
 */
bool DPDKPort::StatsMonitor::waitForSetupFinished(int msecs)
{
    QTime t;

    t.start();

    while(!statsMonSetupDone)
    {
        if(t.elapsed() > msecs)
            return false;

        QThread::msleep(10);
    }

    return true;
}

void DPDKPort::updatePacketList()
{
    qDebug("In %s", __FUNCTION__);

    int kMaxPktSize = 16384;
    uchar pktBuf[kMaxPktSize];
    int len = 0;

    dpdk_tx_mode_t mode;

    if (data_.transmit_mode() == OstProto::kSequentialTransmit)
    {
        qDebug("kSequentialTransmit");
        mode = DPDK_SEQ_MODE;
    }

    if (data_.transmit_mode() == OstProto::kInterleavedTransmit)
    {
        qDebug("kInterleavedTransmit");
        mode = DPDK_INTER_MODE;
    }

    // First sort the streams by ordinalValue
    qSort(streamList_.begin(), streamList_.end(), StreamBase::StreamLessThan);

    dpdk_clear_packets(portId);

    dpdk_set_mix_mode(portId, mode);

    for (int i = 0; i < streamList_.size(); i++)
    {
        if (!streamList_[i]->isEnabled())
        {
            continue;
        }

        int frameVariableCount = streamList_[i]->frameVariableCount();
        qDebug("frameVariableCount %u", frameVariableCount);

        int frameCount = streamList_[i]->frameCount();
        qDebug("frameCount %u", frameCount);

        if (streamList_[i]->sendUnit() == OstProto::StreamControl::e_su_bursts)
        {
            quint32 burstSize = streamList_[i]->burstSize();
            qDebug("burstSize %u", burstSize);

            double burstRate = streamList_[i]->burstRate();
            qDebug("burstRate %g", burstRate);

            quint32 numBursts = streamList_[i]->numBursts();
            qDebug("numBursts %u", numBursts);

            double nDelay = SEC_TO_NSEC / burstRate;
            qDebug("nDelay %g", nDelay);

            quint64 delay64 = quint64(nDelay);
            qDebug("delay %llu", delay64);

            quint32 streamId = 0;
            if (!dpdk_add_burst_stream(portId, delay64, burstSize, numBursts, &streamId))
            {
                continue;
            }

            for (int frameNum = 0; frameNum < frameVariableCount; frameNum++)
            {
                len = streamList_[i]->frameValue(pktBuf, sizeof(pktBuf), frameNum);
                qDebug("len %u", len);

                 dpdk_add_packet(portId, streamId, pktBuf, len);
            }
        }

        if (streamList_[i]->sendUnit() == OstProto::StreamControl::e_su_packets)
        {
            quint32 numPackets = streamList_[i]->numPackets();
            qDebug("numPackets %u", numPackets);

            double packetRate = streamList_[i]->packetRate();
            qDebug("packetRate %g", packetRate);

            double nDelay = SEC_TO_NSEC / packetRate;
            qDebug("nDelay %g", nDelay);

            quint64 delay64 = quint64(nDelay);
            qDebug("delay %llu", delay64);

            quint32 streamId = 0;
            if (!dpdk_add_packet_stream(portId, delay64, numPackets, &streamId))
            {
                continue;
            }

            for (int frameNum = 0; frameNum < frameVariableCount; frameNum++)
            {
                len = streamList_[i]->frameValue(pktBuf, sizeof(pktBuf), frameNum);
                qDebug("len %u", len);

                dpdk_add_packet(portId, streamId, pktBuf, len);
            }
        }

        if (streamList_[i]->nextWhat() == OstProto::StreamControl::e_nw_stop)
        {
            qDebug("e_nw_stop");
        }

        if (streamList_[i]->nextWhat() == OstProto::StreamControl::e_nw_goto_id)
        {
            qDebug("e_nw_goto_id");
            dpdk_set_loop_mode(portId, true);
        }

        if (streamList_[i]->nextWhat() == OstProto::StreamControl::e_nw_goto_next)
        {
            qDebug("e_nw_goto_next");
        }
    }
}
