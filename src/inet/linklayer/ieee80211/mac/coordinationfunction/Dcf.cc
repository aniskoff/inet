//
// Copyright (C) 2016 OpenSim Ltd.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/ieee8022/Ieee8022LlcHeader_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/Dcf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/DcfFs.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/rateselection/RateSelection.h"
#include "inet/linklayer/ieee80211/mac/recipient/RecipientAckProcedure.h"

namespace inet {
namespace ieee80211 {

Define_Module(Dcf);

void Dcf::initialize(int stage)
{
    ModeSetListener::initialize(stage);
    if (stage == INITSTAGE_LINK_LAYER_2) {
        startRxTimer = new cMessage("startRxTimeout");
        mac = check_and_cast<Ieee80211Mac *>(getContainingNicModule(this)->getSubmodule("mac"));
        dataAndMgmtRateControl = dynamic_cast<IRateControl *>(getSubmodule(("rateControl")));
        tx = check_and_cast<ITx *>(getModuleByPath(par("txModule")));
        rx = check_and_cast<IRx *>(getModuleByPath(par("rxModule")));
        dcfChannelAccess = check_and_cast<IChannelAccess *>(getSubmodule("channelAccess"));
        originatorDataService = check_and_cast<IOriginatorMacDataService *>(getSubmodule(("originatorMacDataService")));
        recipientDataService = check_and_cast<IRecipientMacDataService*>(getSubmodule("recipientMacDataService"));
        recoveryProcedure = check_and_cast<NonQoSRecoveryProcedure *>(getSubmodule("recoveryProcedure"));
        rateSelection = check_and_cast<IRateSelection*>(getSubmodule("rateSelection"));
        pendingQueue = new PendingQueue(par("maxQueueSize"), nullptr, par("prioritizeMulticast") ? PendingQueue::Priority::PRIORITIZE_MULTICAST_OVER_DATA : PendingQueue::Priority::PRIORITIZE_MGMT_OVER_DATA);
        rtsProcedure = new RtsProcedure();
        rtsPolicy = check_and_cast<IRtsPolicy *>(getSubmodule("rtsPolicy"));
        recipientAckProcedure = new RecipientAckProcedure();
        recipientAckPolicy = check_and_cast<IRecipientAckPolicy *>(getSubmodule("recipientAckPolicy"));
        originatorAckPolicy = check_and_cast<IOriginatorAckPolicy *>(getSubmodule("originatorAckPolicy"));
        frameSequenceHandler = new FrameSequenceHandler();
        ackHandler = new AckHandler();
        ctsProcedure = new CtsProcedure();
        ctsPolicy = check_and_cast<ICtsPolicy *>(getSubmodule("ctsPolicy"));
        stationRetryCounters = new StationRetryCounters();
        inProgressFrames = new InProgressFrames(pendingQueue, originatorDataService, ackHandler);
        originatorProtectionMechanism = check_and_cast<OriginatorProtectionMechanism*>(getSubmodule("originatorProtectionMechanism"));
    }
}

void Dcf::handleMessage(cMessage* msg)
{
    if (msg == startRxTimer) {
        if (!isReceptionInProgress()) {
            frameSequenceHandler->handleStartRxTimeout();
            updateDisplayString();
        }
    }
    else
        throw cRuntimeError("Unknown msg type");
}

void Dcf::updateDisplayString()
{
    if (frameSequenceHandler->isSequenceRunning()) {
        auto history = frameSequenceHandler->getFrameSequence()->getHistory();
        getDisplayString().setTagArg("t", 0, ("Fs: " + history).c_str());
    }
    else
        getDisplayString().removeTag("t");
}

void Dcf::channelGranted(IChannelAccess *channelAccess)
{
    ASSERT(dcfChannelAccess == channelAccess);
    if (!frameSequenceHandler->isSequenceRunning()) {
        frameSequenceHandler->startFrameSequence(new DcfFs(), buildContext(), this);
        updateDisplayString();
    }
}

void Dcf::processUpperFrame(Packet *packet, const Ptr<Ieee80211DataOrMgmtHeader>& header)
{
    Enter_Method("processUpperFrame(%s)", packet->getName());
    EV_INFO << "Processing upper frame: " << packet->getName() << endl;
    if (pendingQueue->insert(packet)) {
        EV_INFO << "Frame " << packet->getName() << " has been inserted into the PendingQueue." << endl;
        EV_DETAIL << "Requesting channel" << endl;
        dcfChannelAccess->requestChannel(this);
    }
    else {
        EV_INFO << "Frame " << packet->getName() << " has been dropped because the PendingQueue is full." << endl;
        emit(NF_PACKET_DROP, packet);
        delete packet;
    }
}

void Dcf::transmitControlResponseFrame(Packet *responsePacket, const Ptr<Ieee80211MacHeader>& responseHeader, Packet *receivedPacket, const Ptr<Ieee80211MacHeader>& receivedHeader)
{
    responsePacket->insertTrailer(std::make_shared<Ieee80211MacTrailer>());
    const IIeee80211Mode *responseMode = nullptr;
    if (auto rtsFrame = std::dynamic_pointer_cast<Ieee80211RtsFrame>(receivedHeader))
        responseMode = rateSelection->computeResponseCtsFrameMode(receivedPacket, rtsFrame);
    else if (auto dataOrMgmtHeader = std::dynamic_pointer_cast<Ieee80211DataOrMgmtHeader>(receivedHeader))
        responseMode = rateSelection->computeResponseAckFrameMode(receivedPacket, dataOrMgmtHeader);
    else
        throw cRuntimeError("Unknown received frame type");
    RateSelection::setFrameMode(responsePacket, responseHeader, responseMode);
    tx->transmitFrame(responsePacket, responseHeader, modeSet->getSifsTime(), this);
    delete responsePacket;
}

void Dcf::processMgmtFrame(Packet *packet, const Ptr<Ieee80211MgmtHeader>& mgmtHeader)
{
    throw cRuntimeError("Unknown management frame");
}

void Dcf::recipientProcessTransmittedControlResponseFrame(const Ptr<Ieee80211MacHeader>& header)
{
    if (auto ctsFrame = std::dynamic_pointer_cast<Ieee80211CtsFrame>(header))
        ctsProcedure->processTransmittedCts(ctsFrame);
    else if (auto ackFrame = std::dynamic_pointer_cast<Ieee80211AckFrame>(header))
        recipientAckProcedure->processTransmittedAck(ackFrame);
    else
        throw cRuntimeError("Unknown control response frame");
}

void Dcf::scheduleStartRxTimer(simtime_t timeout)
{
    Enter_Method_Silent();
    scheduleAt(simTime() + timeout, startRxTimer);
}

void Dcf::processLowerFrame(Packet *packet, const Ptr<Ieee80211MacHeader>& header)
{
    Enter_Method_Silent();
    if (frameSequenceHandler->isSequenceRunning()) {
        // TODO: always call processResponses
        if ((!isForUs(header) && !startRxTimer->isScheduled()) || isForUs(header)) {
            frameSequenceHandler->processResponse(packet);
            updateDisplayString();
        }
        else {
            EV_INFO << "This frame is not for us" << std::endl;
            delete packet;
        }
        cancelEvent(startRxTimer);
    }
    else if (isForUs(header))
        recipientProcessReceivedFrame(packet, header);
    else {
        EV_INFO << "This frame is not for us" << std::endl;
        delete packet;
    }
}

void Dcf::transmitFrame(Packet *packet, simtime_t ifs)
{
    const auto& frame = packet->peekHeader<Ieee80211MacHeader>();
    RateSelection::setFrameMode(packet, frame, rateSelection->computeMode(packet, frame));
    auto pendingPacket = inProgressFrames->getPendingFrameFor(packet);
    auto duration = originatorProtectionMechanism->computeDurationField(packet, frame, pendingPacket, pendingPacket == nullptr ? nullptr : pendingPacket->peekHeader<Ieee80211DataOrMgmtHeader>());
    const auto& header = packet->removeHeader<Ieee80211MacHeader>();
    header->setDuration(duration);
    packet->insertHeader(header);
    tx->transmitFrame(packet, packet->peekHeader<Ieee80211MacHeader>(), ifs, this);
}

/*
 * TODO:  If a PHY-RXSTART.indication primitive does not occur during the ACKTimeout interval,
 * the STA concludes that the transmission of the MPDU has failed, and this STA shall invoke its
 * backoff procedure **upon expiration of the ACKTimeout interval**.
 */

void Dcf::frameSequenceFinished()
{
    dcfChannelAccess->releaseChannel(this);
    if (hasFrameToTransmit())
        dcfChannelAccess->requestChannel(this);
    mac->sendDownPendingRadioConfigMsg(); // TODO: review
}

bool Dcf::isReceptionInProgress()
{
    return rx->isReceptionInProgress();
}

void Dcf::recipientProcessReceivedFrame(Packet *packet, const Ptr<Ieee80211MacHeader>& header)
{
    if (auto dataOrMgmtHeader = std::dynamic_pointer_cast<Ieee80211DataOrMgmtHeader>(header))
        recipientAckProcedure->processReceivedFrame(packet, dataOrMgmtHeader, recipientAckPolicy, this);
    if (auto dataHeader = std::dynamic_pointer_cast<Ieee80211DataHeader>(header))
        sendUp(recipientDataService->dataFrameReceived(packet, dataHeader));
    else if (auto mgmtHeader = std::dynamic_pointer_cast<Ieee80211MgmtHeader>(header))
        sendUp(recipientDataService->managementFrameReceived(packet, mgmtHeader));
    else { // TODO: else if (auto ctrlFrame = dynamic_cast<Ieee80211ControlFrame*>(frame))
        sendUp(recipientDataService->controlFrameReceived(packet, header));
        recipientProcessControlFrame(packet, header);
    }
}

void Dcf::sendUp(const std::vector<Packet*>& completeFrames)
{
    for (auto frame : completeFrames)
        mac->sendUpFrame(frame);
}

void Dcf::recipientProcessControlFrame(Packet *packet, const Ptr<Ieee80211MacHeader>& header)
{
    if (auto rtsFrame = std::dynamic_pointer_cast<Ieee80211RtsFrame>(header))
        ctsProcedure->processReceivedRts(packet, rtsFrame, ctsPolicy, this);
    else
        throw cRuntimeError("Unknown control frame");
}

FrameSequenceContext* Dcf::buildContext()
{
    auto nonQoSContext = new NonQoSContext(originatorAckPolicy);
    return new FrameSequenceContext(mac->getAddress(), modeSet, inProgressFrames, rtsProcedure, rtsPolicy, nonQoSContext, nullptr);
}

void Dcf::transmissionComplete(Packet *packet, const Ptr<Ieee80211MacHeader>& header)
{
    if (frameSequenceHandler->isSequenceRunning()) {
        frameSequenceHandler->transmissionComplete();
        updateDisplayString();
    }
    else
        recipientProcessTransmittedControlResponseFrame(header);
    delete packet;
}

bool Dcf::hasFrameToTransmit()
{
    return !pendingQueue->isEmpty() || inProgressFrames->hasInProgressFrames();
}

void Dcf::originatorProcessRtsProtectionFailed(Packet *packet)
{
    EV_INFO << "RTS frame transmission failed\n";
    auto protectedHeader = packet->peekHeader<Ieee80211DataOrMgmtHeader>();
    recoveryProcedure->rtsFrameTransmissionFailed(protectedHeader, stationRetryCounters);
    if (recoveryProcedure->isRtsFrameRetryLimitReached(packet, protectedHeader)) {
        recoveryProcedure->retryLimitReached(packet, protectedHeader);
        inProgressFrames->dropFrame(packet);
        emit(NF_PACKET_DROP, packet);
        emit(NF_LINK_BREAK, packet);
        delete packet;
    }
}

void Dcf::originatorProcessTransmittedFrame(Packet *packet)
{
    auto transmittedHeader = packet->peekHeader<Ieee80211MacHeader>();
    if (auto dataOrMgmtHeader = std::dynamic_pointer_cast<Ieee80211DataOrMgmtHeader>(transmittedHeader)) {
        if (originatorAckPolicy->isAckNeeded(dataOrMgmtHeader)) {
            ackHandler->processTransmittedDataOrMgmtFrame(dataOrMgmtHeader);
        }
        else if (dataOrMgmtHeader->getReceiverAddress().isMulticast()) {
            recoveryProcedure->multicastFrameTransmitted(stationRetryCounters);
            inProgressFrames->dropFrame(packet);
        }
    }
    else if (auto rtsFrame = std::dynamic_pointer_cast<Ieee80211RtsFrame>(transmittedHeader))
        rtsProcedure->processTransmittedRts(rtsFrame);
}

void Dcf::originatorProcessReceivedFrame(Packet *packet, Packet *lastTransmittedPacket)
{
    auto frame = packet->peekHeader<Ieee80211MacHeader>();
    auto lastTransmittedFrame = lastTransmittedPacket->peekHeader<Ieee80211MacHeader>();
    if (frame->getType() == ST_ACK) {
        auto lastTransmittedDataOrMgmtHeader = std::dynamic_pointer_cast<Ieee80211DataOrMgmtHeader>(lastTransmittedFrame);
        if (dataAndMgmtRateControl) {
            int retryCount;
            if (lastTransmittedFrame->getRetry())
                retryCount = recoveryProcedure->getRetryCount(packet, lastTransmittedDataOrMgmtHeader);
            else
                retryCount = 0;
            dataAndMgmtRateControl->frameTransmitted(packet, retryCount, true, false);
        }
        recoveryProcedure->ackFrameReceived(packet, lastTransmittedDataOrMgmtHeader, stationRetryCounters);
        ackHandler->processReceivedAck(std::dynamic_pointer_cast<Ieee80211AckFrame>(frame), lastTransmittedDataOrMgmtHeader);
        inProgressFrames->dropFrame(lastTransmittedPacket);
    }
    else if (frame->getType() == ST_RTS)
        ; // void
    else if (frame->getType() == ST_CTS)
        recoveryProcedure->ctsFrameReceived(stationRetryCounters);
    else
        throw cRuntimeError("Unknown frame type");
    delete packet;
}

void Dcf::originatorProcessFailedFrame(Packet *packet)
{
    EV_INFO << "Data/Mgmt frame transmission failed\n";
    const auto& failedFrame = packet->peekHeader<Ieee80211DataOrMgmtHeader>();
    ASSERT(failedFrame->getType() != ST_DATA_WITH_QOS);
    ASSERT(ackHandler->getAckStatus(failedFrame) == AckHandler::Status::WAITING_FOR_ACK);
    recoveryProcedure->dataOrMgmtFrameTransmissionFailed(packet, failedFrame, stationRetryCounters);
    bool retryLimitReached = recoveryProcedure->isRetryLimitReached(packet, failedFrame);
    if (dataAndMgmtRateControl) {
        int retryCount = recoveryProcedure->getRetryCount(packet, failedFrame);
        dataAndMgmtRateControl->frameTransmitted(packet, retryCount, false, retryLimitReached);
    }
    ackHandler->processFailedFrame(failedFrame);
    if (retryLimitReached) {
        recoveryProcedure->retryLimitReached(packet, failedFrame);
        inProgressFrames->dropFrame(packet);
        // KLUDGE: removed headers and trailers to allow higher layer protocols to process the packet
        packet->popHeader<Ieee80211DataOrMgmtHeader>();
        const auto& nextHeader = packet->peekHeader();
        if (std::dynamic_pointer_cast<Ieee8022LlcHeader>(nextHeader))
            packet->popHeader<Ieee8022LlcHeader>();
        packet->popTrailer<Ieee80211MacTrailer>();
        emit(NF_PACKET_DROP, packet);
        emit(NF_LINK_BREAK, packet);
        delete packet;
    }
    else {
        auto h = packet->removeHeader<Ieee80211DataOrMgmtHeader>();
        h->setRetry(true);
        packet->insertHeader(h);
    }
}

bool Dcf::isForUs(const Ptr<Ieee80211MacHeader>& header) const
{
    return header->getReceiverAddress() == mac->getAddress() || (header->getReceiverAddress().isMulticast() && !isSentByUs(header));
}

bool Dcf::isSentByUs(const Ptr<Ieee80211MacHeader>& header) const
{
    // FIXME:
    // Check the roles of the Addr3 field when aggregation is applied
    // Table 8-19—Address field contents
    if (auto dataOrMgmtHeader = std::dynamic_pointer_cast<Ieee80211DataOrMgmtHeader>(header))
        return dataOrMgmtHeader->getAddress3() == mac->getAddress();
    else
        return false;
}

void Dcf::corruptedFrameReceived()
{
    if (frameSequenceHandler->isSequenceRunning()) {
        if (!startRxTimer->isScheduled()) {
            frameSequenceHandler->handleStartRxTimeout();
            updateDisplayString();
        }
    }
}

Dcf::~Dcf()
{
    cancelAndDelete(startRxTimer);
    delete pendingQueue;
    delete inProgressFrames;
    delete rtsProcedure;
    delete recipientAckProcedure;
    delete ackHandler;
    delete stationRetryCounters;
    delete frameSequenceHandler;
    delete ctsProcedure;
}

} // namespace ieee80211
} // namespace inet

