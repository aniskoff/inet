package custom.queueing.source;

import inet.queueing.base.PacketSourceBase;
import inet.queueing.contract.IActivePacketSource;


simple CSVPacketSource extends PacketSourceBase like IActivePacketSource
{
    parameters:
        string clockModule = default(""); // relative path of a module that implements IClock; optional
        // volatile double productionInterval @unit(s); // elapsed time between subsequent packets pushed to the connected packet consumer
        string csvFilePath;
        int csvSep;

        @class(CSVPacketSource);
        @display("i=block/source");
        @signal[packetCreated](type=inet::Packet);
        @statistic[packetCreated](title="packets created"; record=count,sum(packetBytes),vector(packetBytes); interpolationmode=none);
    gates:
        output out @labels(push);
}
