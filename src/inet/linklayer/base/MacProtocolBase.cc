//
// Copyright (C) 2013 OpenSim Ltd.
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
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "inet/linklayer/base/MacProtocolBase.h"

#include "inet/common/IInterfaceRegistrationListener.h"
#include "inet/common/ModuleAccess.h"

namespace inet {

MacProtocolBase::MacProtocolBase()
{
}

MacProtocolBase::~MacProtocolBase()
{
    delete currentTxFrame;
}

MacAddress MacProtocolBase::parseMacAddressParameter(const char *addrstr)
{
    MacAddress address;

    if (!strcmp(addrstr, "auto"))
        // assign automatic address
        address = MacAddress::generateAutoAddress();
    else
        address.setAddress(addrstr);

    return address;
}

void MacProtocolBase::initialize(int stage)
{
    LayeredProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        currentTxFrame = nullptr;
        upperLayerInGateId = findGate("upperLayerIn");
        upperLayerOutGateId = findGate("upperLayerOut");
        lowerLayerInGateId = findGate("lowerLayerIn");
        lowerLayerOutGateId = findGate("lowerLayerOut");
        hostModule = findContainingNode(this);
        txQueue = getConnectedModule<queueing::IPacketQueue>(gate(upperLayerInGateId));
    }
    else if (stage == INITSTAGE_NETWORK_INTERFACE_CONFIGURATION)
        registerInterface();
}

void MacProtocolBase::registerInterface()
{
    ASSERT(networkInterface == nullptr);
    networkInterface = getContainingNicModule(this);
    configureNetworkInterface();
}

void MacProtocolBase::sendUp(cMessage *message)
{
    if (message->isPacket())
        emit(packetSentToUpperSignal, message);
    send(message, upperLayerOutGateId);
}

void MacProtocolBase::sendDown(cMessage *message)
{
    if (message->isPacket())
        emit(packetSentToLowerSignal, message);
    send(message, lowerLayerOutGateId);
}

bool MacProtocolBase::isUpperMessage(cMessage *message)
{
    return message->getArrivalGateId() == upperLayerInGateId;
}

bool MacProtocolBase::isLowerMessage(cMessage *message)
{
    return message->getArrivalGateId() == lowerLayerInGateId;
}

void MacProtocolBase::deleteCurrentTxFrame()
{
    delete currentTxFrame;
    currentTxFrame = nullptr;
}

void MacProtocolBase::dropCurrentTxFrame(PacketDropDetails& details)
{
    emit(packetDroppedSignal, currentTxFrame, &details);
    delete currentTxFrame;
    currentTxFrame = nullptr;
}

void MacProtocolBase::popTxQueue()
{
    if (currentTxFrame != nullptr)
        throw cRuntimeError("Model error: incomplete transmission exists");
    ASSERT(txQueue != nullptr);
    currentTxFrame = txQueue->dequeuePacket();
    currentTxFrame->setArrival(getId(), upperLayerInGateId, simTime());
    take(currentTxFrame);
}

void MacProtocolBase::flushQueue(PacketDropDetails& details)
{
    // code would look slightly nicer with a pop() function that returns nullptr if empty
    if (txQueue)
        while (!txQueue->isEmpty()) {
            auto packet = txQueue->dequeuePacket();
            emit(packetDroppedSignal, packet, &details); // FIXME this signal lumps together packets from the network and packets from higher layers! separate them
            delete packet;
        }
}

void MacProtocolBase::clearQueue()
{
    if (txQueue)
        while (!txQueue->isEmpty())
            delete txQueue->dequeuePacket();
}

void MacProtocolBase::handleMessageWhenDown(cMessage *msg)
{
    if (!msg->isSelfMessage() && msg->getArrivalGateId() == lowerLayerInGateId) {
        EV << "Interface is turned off, dropping packet\n";
        delete msg;
    }
    else
        LayeredProtocolBase::handleMessageWhenDown(msg);
}

void MacProtocolBase::handleStartOperation(LifecycleOperation *operation)
{
    networkInterface->setState(NetworkInterface::State::UP);
    networkInterface->setCarrier(true);
}

void MacProtocolBase::handleStopOperation(LifecycleOperation *operation)
{
    PacketDropDetails details;
    details.setReason(INTERFACE_DOWN);
    if (currentTxFrame)
        dropCurrentTxFrame(details);
    flushQueue(details);
    networkInterface->setCarrier(false);
    networkInterface->setState(NetworkInterface::State::DOWN);
}

void MacProtocolBase::handleCrashOperation(LifecycleOperation *operation)
{
    deleteCurrentTxFrame();
    clearQueue();
    networkInterface->setCarrier(false);
    networkInterface->setState(NetworkInterface::State::DOWN);
}

void MacProtocolBase::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));
}

queueing::IPassivePacketSource *MacProtocolBase::getProvider(cGate *gate)
{
    return (gate->getId() == upperLayerInGateId) ? txQueue.get() : nullptr;
}

/**
 * Notifies about a change in the possibility of pulling some packet from
 * the passive packet source at the given gate.
 *
 * This method is called, for example, when a new packet is inserted into
 * a queue. It allows the sink to pull a new packet from the queue.
 *
 * The gate parameter must be a valid gate of this module.
 */
void MacProtocolBase::handleCanPullPacketChanged(cGate *gate)
{
    Enter_Method("handleCanPullPacketChanged");
    tryProcessUpperPackets();
}

/**
 * Notifies about the completion of the packet processing for a packet that
 * was pulled earlier independently whether the packet is passed or streamed.
 *
 * This method is called, for example, when a previously pulled packet is
 * failed to be processed successfully. It allows the sink to retry the
 * operation.
 *
 * The gate parameter must be a valid gate of this module. The packet must
 * not be nullptr.
 */
void MacProtocolBase::handlePullPacketProcessed(Packet *packet, cGate *gate, bool successful)
{
    Enter_Method("handlePullPacketProcessed");
    tryProcessUpperPackets();
}

bool MacProtocolBase::canProcessUpperPacket() const
{
    return (!currentTxFrame    // not an active transmission
            && txQueue->canPullSomePacket(gate(upperLayerInGateId)->getPathStartGate())
            );
}

void MacProtocolBase::processUpperPacket()
{
    auto packet = txQueue->pullPacket(gate(upperLayerInGateId)->getPathStartGate());
    take(packet);
    handleUpperPacket(packet);
}

void MacProtocolBase::tryProcessUpperPackets()
{
    while (canProcessUpperPacket())
        processUpperPacket();
}

} // namespace inet

