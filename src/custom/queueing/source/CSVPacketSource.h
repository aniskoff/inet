#pragma once

#include "inet/common/clock/ClockUserModuleMixin.h"
#include "inet/queueing/base/ActivePacketSourceBase.h"
#include "custom/queueing/source/CSVUtil/CSVIterator.h"
#include <string>

using std::string;

namespace inet{
namespace queueing{

class INET_API CSVPacketSource : public ClockUserModuleMixin<ActivePacketSourceBase>
{
  public:
    virtual void handleCanPushPacketChanged(cGate *gate) override;
    virtual void handlePushPacketProcessed(Packet *packet, cGate *gate, bool successful) override;
    virtual ~CSVPacketSource();
  
  protected:
    ClockEvent *productionTimer = nullptr;


  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *message) override;

    virtual void scheduleProductionTimer(double deltaTime, int packetSize);
    void producePacket(int packetSize);
    void scheduleTimerAndProducePacket();
      
  private:
    const char* csvFilePath = nullptr;
    char csvSep = '\t';
    std::ifstream csvInStream;
    CSVIterator csvIter;

};

}
}