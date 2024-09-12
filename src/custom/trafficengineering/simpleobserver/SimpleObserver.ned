package custom.trafficengineering.simpleobserver;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;

simple SimpleObserver extends Ipv4NetworkConfigurator
{
    parameters:
        int csvSep = default(44); // char(44) == ','
        string hostToMidHopCandidatesFp;
        string srcDestToFstLstHopsFp;
        string flowIdToSourceFp;
        string flowIdToPredFilesFp; // flowId -> Prediction filePath
        int nFlows;
        double randomRoutingFreq;
        double predictionInterval;
        double timePredStart; // Starting time for all predictions 
        double timePredEnd; // End time for all predictions
        string trafficEngAlgName; // Either `random`, `claire`, `garch`, `arima` or `ground_truth`

        @class(SimpleObserver);
}