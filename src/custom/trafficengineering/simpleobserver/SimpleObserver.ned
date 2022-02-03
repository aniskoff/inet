package custom.trafficengineering.simpleobserver;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;

simple SimpleObserver extends Ipv4NetworkConfigurator
{
    parameters:
        int csvSep = default(44); // char(44) == ','
        string dynHopsToNextHopsFp;
        string srcDestToFstLstHopsFp;

        @class(SimpleObserver);
}