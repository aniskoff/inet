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
    timePacketsStart = par("timePacketsStart").doubleValue();
    timePacketsEnd =  par("timePacketsEnd").doubleValue();
    if (strcmp(packetRepresentation, "bytes") != 0)
      throw cRuntimeError("Only packetRepresentation=bytes is supported.");

    csvInStream = std::ifstream(csvFilePath);
    if (!csvInStream)
      throw cRuntimeError("Input CSV stream object is not in valid state. Check the CSV file path.");
    csvIter = CSVIterator(csvInStream, csvSep);
    productionTimer = new ClockEvent("ProductionTimer");
    productionTimer->setContextPointer(new int(0)); // packetSize variable

    fastForwardCSVIterToStartTime();
  }

  else if (stage == INITSTAGE_QUEUEING) 
  {
    if (!productionTimer->isScheduled() && (consumer == nullptr || consumer->canPushSomePacket(outputGate->getPathEndGate()))) 
    {
      std::cout << "initT = " << atof(((*csvIter)[CSVPos::timePos]).data()) * 60. << " packetSize = " << ((*csvIter)[CSVPos::sizePos]).data() << std::endl;
      //
      double initT = std::atof(((*csvIter)[CSVPos::timePos]).data()) * 60.; // * 60. -> Converting minutes to seconds! 
      int packetSize = std::atoi(((*csvIter)[CSVPos::sizePos]).data()); 
      scheduleProductionTimer(shiftedStartRelative(initT), packetSize);
      ++csvIter;
      
      
    }
  }
}


void CSVPacketSource::handleMessage(cMessage* msg)
{
  if (simTime() > timePacketsEnd - timePacketsStart)
  {
    std::string msg = "Achived timePacketsEnd bound. Stopping scheduling new messages.";
    std::cout << msg << std::endl;
    EV << msg << endl;
    return;
  }

  if (msg == productionTimer)
  {
  
    if (consumer == nullptr || consumer->canPushSomePacket(outputGate->getPathEndGate()))
    {
      if (csvIter != CSVIterator()) // Iterator not empty
      {
        scheduleTimerAndProducePacket();
        ++csvIter;
      }
      else
      {
        std::string msg = "Reached the end of CSV packet source file: " + std::string(csvFilePath); 
        throw cTerminationException("%s", msg.data());
      }
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
  double deltaTime = shiftedStartRelative(std::atof(((*csvIter)[CSVPos::timePos]).data()) * 60. - simTime().dbl()); // * 60. -> Converting minutes to seconds!  
  int nextPacketSize = std::atoi(((*csvIter)[CSVPos::sizePos]).data()); 
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

void CSVPacketSource::fastForwardCSVIterToStartTime()
{
  while ((atof(((*csvIter)[CSVPos::timePos]).data()) * 60 < timePacketsStart) && csvIter != CSVIterator())
    ++csvIter;    
}


double CSVPacketSource::shiftedStartRelative(double t)
{
  return t - timePacketsStart;
}

}
}