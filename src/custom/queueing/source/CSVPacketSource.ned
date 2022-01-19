package custom.queueing.source;

import inet.queueing.base.PacketSourceBase;
import inet.queueing.contract.IActivePacketSource;

//
// This module is an active packet source which pushes packets into the connected
// packet consumer.
//
simple CSVPacketSource extends PacketSourceBase like IActivePacketSource
{
    parameters:
        string clockModule = default(""); // relative path of a module that implements IClock; optional
        // volatile double productionInterval @unit(s); // elapsed time between subsequent packets pushed to the connected packet consumer
        int csvSep = default(9); // char(9) == '\t'
        string csvFilePath;

        @class(CSVPacketSource);
        @display("i=block/source");
        @signal[packetCreated](type=inet::Packet);
        @statistic[packetCreated](title="packets created"; record=count,sum(packetBytes),vector(packetBytes); interpolationmode=none);
    gates:
        output out @labels(push);
}
