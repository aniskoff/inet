#pragma once


#include <string>
#include <unordered_map>

#include <omnetpp.h>

#include "inet/common/clock/ClockUserModuleMixin.h"
#include "inet/networklayer/configurator/base/NetworkConfiguratorBase.h"
#include "inet/networklayer/configurator/ipv4/Ipv4NetworkConfigurator.h"

#include "custom/queueing/source/CSVUtil/CSVIterator.h"


using namespace inet;

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ h2;  
    }
};

using pair_ss = std::pair<std::string, std::string>;


std::vector<std::vector<bool>> generateRoutingBitmasks(int nFlows);

class SimpleObserver : public Ipv4NetworkConfigurator
{
    public:
        enum ROUTE_METRIC
        {
            MAIN_ROUTE = -2,
            ALTERNATE_ROUTE = -1
        };


    protected:
        virtual void initialize(int stage) override;
        virtual void handleMessage(cMessage *msg) override;
        virtual int numInitStages() const override;

    private:
        void scheduleObserverTimer(clocktime_t delay, ClockEvent* msg);
        // Logging-like functions
        void mPrintTopology();

        // Route changing functions
        void changeRoutForSource(const std::string& srcNodePath);
        void changeRoutForFlowId(const int flowId);
        void internalChangeRout(Ipv4NetworkConfigurator::Node* fstHop, Ipv4NetworkConfigurator::Node* lstHop,
                        const Ipv4Address& src, const Ipv4Address& dest);
        
        void internalChangeRoutForNode(Ipv4NetworkConfigurator::Node* node, const Ipv4Address& addr);
        void changeMetric(Ipv4Route* rout);

        // Routing tables initial setup funcitons
        void addRouteForDynamicHop(IIpv4RoutingTable* hopRtable, std::string nextHopPath, std::string destNodePath, bool mainRoute);
        void prepareRoutingTableForDynamicHop(const std::string& hopNodePath, const std::string& srcNodePath, const std::string& destNodePath);
        void prepareRoutingTables();

        // Traffic engineering functions
        std::vector<pair_ss> collectLinksArray();
        std::vector<pair_ss> getLinksOnPathBySrc(const std::string& srcNodePath);
        std::vector<pair_ss> getLinksOnPathByFlowId(const int flowId);
        void fillRoutingBitmaskIdxToFlowId();
        void applyRoutingBitmask(std::vector<bool> bitmask);
        
        // Random traffic engineering
        void changeRoutsRandomly(); // simply calls applyRandomRoutingBimask()
        void applyRandomRoutingBimask();
     

        // Predicted based traffic engineering
        void changeRoutsPredictedBased();
        void changeRoutsPredictedBasedBruteForce(std::unordered_map<int, double> prediction);
        double estimateMaxLinkLoadCurrState(std::unordered_map<int, double> prediction);
        std::unordered_map<int, double> getCurrentPrediction();

        // Common traffic eng function
        void changeRoutsAndScheduleNextEvent();

        // Configs reading functions
        void readSrcDestToFstLstHopsConf();
        void readHostToMidHopCandidatesConf();
        void readFlowIdToSource();
        std::unordered_map<int, std::string> getFlowIdToPredFilesMapping();
        void fillFlowIdToPredIter(std::unordered_map<int, std::string> flowIdToPredFiles);

        // Other helper functions

        std::vector<int> getFlowIdsArr();
        void fastForwardPredCSVIters();
        void incrPredCSVIters();



        std::unordered_map<std::string, std::string> srcToDest;
        std::unordered_map<pair_ss, pair_ss, pair_hash> srcDestToFstLstHops;
        // Hop is "dynamic" if there can be some traffic enginerring in it i.e. we can manage it's next hops.

        std::unordered_map<int, std::string> flowIdToSourcePath;
        std::unordered_map<std::string, int> sourcePathToFlowId;
        std::unordered_map<std::string, pair_ss> hostToMidHopCandidates;
        std::vector<pair_ss> linksArr;
        
        std::unordered_map<int, int> routingBitmaskIdxToFlowId;
        
        std::unordered_map<int, CSVIterator> flowIdToPredIter;

        // Internal mapping for flowIdToPredIter 
        std::unordered_map<int, std::ifstream> flowIdToCSVIfstream;

        
        std::string hostToMidHopCandidatesFp;
        std::string srcDestToFstLstHopsFp;
        std::string flowIdToSourceFp;
        std::string flowIdToPredFilesFp;
        char csvSep = ',';  

        int nFlows;
        double randomRoutingFreq; // If algorithm=Random => change routes randomly every `randomRoutingFreq` seconds;
        std::string trafficEngAlgName;
        double predictionInterval; 
        double timePredStart;
        double timePredEnd;

        std::vector<std::vector<bool>> routingBitmasks;

        ClockEvent* mNextEventTimer = nullptr;


};