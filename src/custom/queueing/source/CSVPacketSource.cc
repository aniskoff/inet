#include "custom/queueing/source/CSVPacketSource.h"
#include "custom/queueing/source/CSVUtil/CSVIterator.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/Simsignals.h"


enum CSVPos
{
  IdPos = 0,
  timePos,
  sizePos,
  flowIdPos
};

namespace inet {
namespace queueing {

Define_Module(CSVPacketSource);


CSVPacketSource::~CSVPacketSource() 
{
  delete reinterpret_cast<int*>(productionTimer->getContextPointer()); // freeing resources
  cancelAndDeleteClockEvent(productionTimer);
}

void CSVPacketSource::initialize(int stage)
{
  ClockUserModuleMixin::initialize(stage);
  if (stage == INITSTAGE_LOCAL) { 
    csvSep = par("csvSep");
    csvFilePath = par("csvFilePath");
    if (strcmp(packetRepresentation, "bytes") != 0)
      throw cRuntimeError("Only packetRepresentation=bytes is supported.");

    csvInStream = std::ifstream(csvFilePath);
    if (!csvInStream)
      throw cRuntimeError("Input CSV stream object is not in valid state. Check the CSV file path.");
    csvIter = CSVIterator(csvInStream, csvSep);
    productionTimer = new ClockEvent("ProductionTimer");
    productionTimer->setContextPointer(new int(0)); // packetSize variable
    
  }
  else if (stage == INITSTAGE_QUEUEING) 
  {
    if (!productionTimer->isScheduled() && (consumer == nullptr || consumer->canPushSomePacket(outputGate->getPathEndGate()))) 
    {
      
      double initT = std::atof(((*csvIter)[CSVPos::timePos]).data());  // CSVReader.getTime()
      int packetSize = std::atoi(((*csvIter)[CSVPos::sizePos]).data()); // CSVReader.getPacketSize();
      scheduleProductionTimer(initT, packetSize);
      ++csvIter;
      
      
    }
  }
}

void CSVPacketSource::handleMessage(cMessage* msg)
{
  if (msg == productionTimer)
  {
    if (consumer == nullptr || consumer->canPushSomePacket(outputGate->getPathEndGate()))
    {
      
      scheduleTimerAndProducePacket();
      
      ++csvIter;
    }
  }
  else
    throw cRuntimeError("Unknown message");
}


void CSVPacketSource::scheduleProductionTimer(double deltaTime, int packetSize)
{
  *reinterpret_cast<int*>(productionTimer->getContextPointer()) = packetSize;
  scheduleClockEventAfter(deltaTime, productionTimer);
}

void CSVPacketSource::scheduleTimerAndProducePacket()
{
  int currentPacketSize = *reinterpret_cast<int*>(productionTimer->getContextPointer());
  double deltaTime = std::atof(((*csvIter)[CSVPos::timePos]).data()) - simTime().dbl();  // CSVReader.getTime() - simTime();
  int nextPacketSize = std::atoi(((*csvIter)[CSVPos::sizePos]).data()); // CSVReader.getPacketSize();
  scheduleProductionTimer(deltaTime, nextPacketSize);
  
  producePacket(currentPacketSize);
}

void CSVPacketSource::handleCanPushPacketChanged(cGate *gate)
{
    Enter_Method("handleCanPushPacketChanged");
    if (!productionTimer->isScheduled() && (consumer == nullptr || consumer->canPushSomePacket(outputGate->getPathEndGate()))) {
        scheduleTimerAndProducePacket();
    }
}

void CSVPacketSource::producePacket(int packetSize)
{

  packetLengthParameter->setIntValue(packetSize * 8); // in bits
  
  auto packet = createPacket();
  EV_INFO << "Producing packet" << EV_FIELD(packet) << EV_ENDL;
  ///////
  EV_INFO << "Current simTime is: " << simTime() << EV_ENDL;

  
  ///////
  pushOrSendPacket(packet, outputGate, consumer);
  updateDisplayString();
}

void CSVPacketSource::handlePushPacketProcessed(Packet *packet, cGate *gate, bool successful)
{
  Enter_Method("handlePushPacketProcessed");
}


}
}
