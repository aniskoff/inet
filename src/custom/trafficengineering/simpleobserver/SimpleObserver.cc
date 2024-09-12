#include <fstream>
#include <cstdlib>

#include "custom/trafficengineering/simpleobserver/SimpleObserver.h"
#include "custom/queueing/source/CSVUtil/CSVIterator.h"
#include "custom/queueing/source/CSVUtil/CSVIterHelpers.h"

#include "inet/networklayer/configurator/base/NetworkConfiguratorBase.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/Topology.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/networklayer/configurator/ipv4/Ipv4NetworkConfigurator.h"

Define_Module(SimpleObserver);

using namespace inet;


namespace utils
{
    enum CSVPredPos
    {
        CLAIRE = 0,
        GARCH,
        ARIMA,
        GROUND_TRUTH,
        SEG,
        T_START,
        T_END
    };


    std::string addRootPrefix(std::string s)
    {
        return "<root>." + s;
    }

    std::string checkedTrafficEngAlgPar(std::string s)
    {
        static const std::unordered_set<std::string> POSSIBLE_ALG_NAMES =
                             {"none", "random", "claire", "garch", "arima", "ground_truth"};
        if (POSSIBLE_ALG_NAMES.find(s) != POSSIBLE_ALG_NAMES.cend())
        {
            return s;
        }

        std::string msg = "Unknown traffic engineering algorithm: " + s;
        throw cRuntimeError("%s", msg.data());
    }

    CSVPredPos algNameToEnum(std::string algName)
    {
        if (algName == "claire")
            return CSVPredPos::CLAIRE;

        else if (algName == "garch")
            return CSVPredPos::GARCH;

        else if (algName == "arima")
            return CSVPredPos::ARIMA;

        else if (algName == "ground_truth")
            return CSVPredPos::GROUND_TRUTH;

        else
        {
            std::string msg = "Unknown traffic engineering algorithm: " + algName;
            throw cRuntimeError("%s", msg.data());
        }
        
    }
}


void SimpleObserver::initialize(int stage)
{
    Ipv4NetworkConfigurator::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
    {
        csvSep = par("csvSep").intValue();

        srcDestToFstLstHopsFp = std::string(par("srcDestToFstLstHopsFp"));
        hostToMidHopCandidatesFp = std::string(par("hostToMidHopCandidatesFp"));
        flowIdToSourceFp = std::string(par("flowIdToSourceFp"));
        flowIdToPredFilesFp = std::string(par("flowIdToPredFilesFp"));

        nFlows = par("nFlows").intValue();
        randomRoutingFreq = par("randomRoutingFreq").doubleValue();
        predictionInterval = par("predictionInterval").doubleValue();
        timePredStart = par("timePredStart").doubleValue();
        timePredEnd = par("timePredEnd").doubleValue();

        trafficEngAlgName = utils::checkedTrafficEngAlgPar(std::string(par("trafficEngAlgName")));
        routingBitmasks = generateRoutingBitmasks(nFlows);
        

        readSrcDestToFstLstHopsConf();
        readHostToMidHopCandidatesConf();
        readFlowIdToSource();

        fillRoutingBitmaskIdxToFlowId();
        fillFlowIdToPredIter(getFlowIdToPredFilesMapping());
        

    }

    if (stage == INITSTAGE_LAST) 
    {
        if (trafficEngAlgName == "none")
            return;
            
        EV << "Topology before prepareRoutingTables(): " << endl;
        mPrintTopology();
        prepareRoutingTables();

        EV << "Topology after prepareRoutingTables(): " << endl;
        mPrintTopology();

        linksArr = collectLinksArray();
        std::cout << "Size of linksArr = " << linksArr.size() << "linksArr[0] = (" << linksArr[0].first << ", " << linksArr[0].second << ") \n"; 
        
        fastForwardPredCSVIters();
        mNextEventTimer = new ClockEvent("ObserverTimer");
        changeRoutsAndScheduleNextEvent();
        incrPredCSVIters();
        // scheduleObserverTimer(randomRoutingFreq, mNextEventTimer);
        EV << "mNextEventTimer Message scheduled";
        
    }
}

std::vector<std::vector<bool>> generateRoutingBitmasks(int nFlows)
{
    std::vector<std::vector<bool>> bitmasks((1 << nFlows), std::vector<bool>(nFlows, true));

    for (int col = nFlows - 1; col >= 0; --col)
    {
        int step = 1 << (nFlows - col);
        for (int row = 0; row < (1 << nFlows); row += step)
        {
            for (int k = 0; k < (step >> 1); ++k)
                bitmasks[row + k][col] = false;
        }
    }
    return bitmasks;
}


void SimpleObserver::changeRoutsAndScheduleNextEvent()
{
    if (trafficEngAlgName == "random")
    {
        changeRoutsRandomly();
        scheduleObserverTimer(randomRoutingFreq, mNextEventTimer);

    }
    else
    {
        changeRoutsPredictedBased();
        scheduleObserverTimer(predictionInterval, mNextEventTimer);
    }
}


void SimpleObserver::changeRoutsPredictedBased() 
{
    std::unordered_map<int, double> pred = getCurrentPrediction();
    changeRoutsPredictedBasedBruteForce(pred);
    // std::cout << "Routes Changed!" << std::endl;
}

void SimpleObserver::changeRoutsPredictedBasedBruteForce(std::unordered_map<int, double> prediction)
{
    std::vector<bool> optimalBitmask = routingBitmasks[0]; // all zero bitmask, none routes are changed
    double optimalMaxLoad = estimateMaxLinkLoadCurrState(prediction);
    
    for (int i = 1; i < routingBitmasks.size(); ++i)
    {
        applyRoutingBitmask(routingBitmasks[i]);
        double currMaxLoad = estimateMaxLinkLoadCurrState(prediction);
        if (currMaxLoad < optimalMaxLoad)
        {
            optimalMaxLoad = currMaxLoad;
            optimalBitmask = routingBitmasks[i];
        }
        applyRoutingBitmask(routingBitmasks[i]); // backtrack to initial state
    }
    // std::cout << "optimalMaxLoad is " << optimalMaxLoad << std::endl;
    applyRoutingBitmask(optimalBitmask);

}

double SimpleObserver::estimateMaxLinkLoadCurrState(std::unordered_map<int, double> prediction)
{
    std::unordered_map<pair_ss, double, pair_hash> linkToLoad;
    for (pair_ss link: linksArr)
        linkToLoad[link] = 0.;

    for (int flowId: getFlowIdsArr())
    {
        for (pair_ss link: getLinksOnPathByFlowId(flowId))
            linkToLoad[link] += prediction[flowId];
    }
    return std::max_element(linkToLoad.begin(),
                            linkToLoad.end(),
                            [](const auto& a, const auto& b)
                              {return a.second < b.second;}
                           )->second;
}

void SimpleObserver::changeRoutsRandomly()
{
    applyRandomRoutingBimask();
}

void SimpleObserver::applyRandomRoutingBimask() 
{
    int randBitmaskIdx = rand() % routingBitmasks.size();
    auto& bitmask = routingBitmasks[randBitmaskIdx];
    // Log
    std::cout << "generated bitmask is: " << std::endl;
    for (auto e: bitmask)
        std::cout << e << " ";
    std::cout << std::endl;
    //
    applyRoutingBitmask(bitmask);
}

void SimpleObserver::applyRoutingBitmask(std::vector<bool> bitmask)
{
    if (bitmask.size() != nFlows)
    {
        std::string msg = "Routing bitmask size must be equal to number of Flows (" + std::to_string(nFlows) + ") "
                          "but actual size of bitmask is " + std::to_string(bitmask.size());
        cRuntimeError("%s", msg.c_str());
    }
    for (int i = 0; i < bitmask.size(); ++i)
    {
        if (bitmask[i])
            changeRoutForFlowId(routingBitmaskIdxToFlowId[i]);
    }
}



void SimpleObserver::scheduleObserverTimer(clocktime_t delay, ClockEvent* msg)
{
    // std::cout << "Scheduling observer timer, delay = " << delay << std::endl;
    cSimpleModule::scheduleAfter(delay, msg);
}

void SimpleObserver::handleMessage(cMessage *msg)
{   
    if (simTime() >= timePredEnd - timePredStart)
    {
        std::string msg = "Achived timePredEnd bound. Stopping scheduling new messages.";
        std::cout << msg << std::endl;
        EV << msg << endl;
        return;
    }

    if (msg == mNextEventTimer)
    {
        auto linksOnPath = getLinksOnPathBySrc("<root>.s11l");
        EV << "There are  " << linksOnPath.size() << " Links on path" << endl;

        EV << "(" << linksOnPath[0].first << ", " << linksOnPath[0].second << ")" << "(" << linksOnPath[1].first << ", " << linksOnPath[1].second << ")" << "(" << linksOnPath[2].first << ", " << linksOnPath[2].second << ")" << "(" << linksOnPath[3].first << ", " << linksOnPath[3].second << ")" << endl;
        //
        changeRoutsAndScheduleNextEvent();
        incrPredCSVIters();
        //
        mPrintTopology();
    }
    else
        throw cRuntimeError("Unknown message");
}

void SimpleObserver::changeMetric(Ipv4Route* rout)
{
    EV << "Entering changeMetric with Ipv4Route* rout = " << rout << endl;
    rout->getMetric() == ROUTE_METRIC::MAIN_ROUTE ? 
                         rout->setMetric(ROUTE_METRIC::ALTERNATE_ROUTE) : 
                         rout->setMetric(ROUTE_METRIC::MAIN_ROUTE);
    EV << "Changed Ipv4Route* rout is = " << rout << endl;
}


void SimpleObserver::prepareRoutingTableForDynamicHop(const std::string& hopNodePath,
                                                      const std::string& srcNodePath,
                                                      const std::string& destNodePath)
{
    Ipv4NetworkConfigurator::Node* hopNode = (Ipv4NetworkConfigurator::Node*)
                                                    topology.getNodeFor(getModuleByPath(hopNodePath.c_str()));

    IIpv4RoutingTable* rtable = (IIpv4RoutingTable*)hopNode->routingTable;
    
    addRouteForDynamicHop(rtable, hostToMidHopCandidates[srcNodePath].first, destNodePath, true);
    addRouteForDynamicHop(rtable,  hostToMidHopCandidates[srcNodePath].second, destNodePath, false);

    // srcToDynHopCandidates 
}

void SimpleObserver::addRouteForDynamicHop(IIpv4RoutingTable* hopRtable, std::string nextHopPath, std::string destNodePath, bool mainRoute)
{
    L3AddressResolver addrResolver;

    NetworkInterface* route_iface = hopRtable->findBestMatchingRoute(
            addrResolver.addressOf(getModuleByPath(nextHopPath.c_str()))
        )->getInterface();

    Ipv4Address route_netmask = Ipv4Address::ALLONES_ADDRESS;
    Ipv4Address route_dest = addrResolver.addressOf(getModuleByPath(destNodePath.c_str())).toIpv4();

    int route_metric = mainRoute ? route_metric = ROUTE_METRIC::MAIN_ROUTE : ROUTE_METRIC::ALTERNATE_ROUTE;

    Ipv4Route* route = new Ipv4Route();
    route->setInterface(route_iface);
    route->setNetmask(route_netmask);
    route->setDestination(route_dest);
    route->setMetric(route_metric);

    hopRtable->addRoute(route);
}

void SimpleObserver::prepareRoutingTables()
{
    for (auto const& [srcDest, fstLstHops] : srcDestToFstLstHops)
    {
        prepareRoutingTableForDynamicHop(fstLstHops.first, srcDest.first, srcDest.second);
        prepareRoutingTableForDynamicHop(fstLstHops.second, srcDest.second, srcDest.first);
    }
}

void SimpleObserver::changeRoutForFlowId(const int flowId)
{
    changeRoutForSource(flowIdToSourcePath[flowId]);
}


void SimpleObserver::changeRoutForSource(const std::string& sourceNodePath)
{
    std::string destNodePath = srcToDest[sourceNodePath];
    pair_ss fstLstHops = srcDestToFstLstHops[pair_ss(sourceNodePath, destNodePath)];
    
    omnetpp::cModule * srcNodeMod = getModuleByPath(sourceNodePath.c_str());

    omnetpp::cModule * destNodeMod = getModuleByPath(destNodePath.c_str());


    Ipv4NetworkConfigurator::Node* fstHopNode = (Ipv4NetworkConfigurator::Node*)
                                                    topology.getNodeFor(getModuleByPath(fstLstHops.first.c_str()));
    

    Ipv4NetworkConfigurator::Node* lstHopNode = (Ipv4NetworkConfigurator::Node*)
                                                    topology.getNodeFor(getModuleByPath(fstLstHops.second.c_str()));


    L3AddressResolver addrRes;
    Ipv4Address srcAddr = addrRes.addressOf(srcNodeMod).toIpv4();
    Ipv4Address destAddr = addrRes.addressOf(destNodeMod).toIpv4();

    internalChangeRout(fstHopNode, lstHopNode, srcAddr, destAddr);
}

void SimpleObserver::internalChangeRout(Ipv4NetworkConfigurator::Node* fstHop, Ipv4NetworkConfigurator::Node* lstHop,
                                const Ipv4Address& src, const Ipv4Address& dest)
{
   
    internalChangeRoutForNode(fstHop, dest);
    internalChangeRoutForNode(lstHop, src);
}


void SimpleObserver::internalChangeRoutForNode(Ipv4NetworkConfigurator::Node* node, const Ipv4Address& addr)
{
    EV << "Entering changeRoutForNode, Ipv4NetworkConfigurator::Node* node=" << node << "; Ipv4Address=" << addr << endl;
    IIpv4RoutingTable* rtable = (IIpv4RoutingTable*)node->routingTable;
    
    std::vector<Ipv4Route*> routesToChange;

    for (int routIdx = 0; routIdx < rtable->getNumRoutes(); ++routIdx)
    {
        Ipv4Route* rout = rtable->getRoute(routIdx);
        if (rout->getDestination() == addr && (rout->getMetric() == ROUTE_METRIC::MAIN_ROUTE ||
                                               rout->getMetric() == ROUTE_METRIC::ALTERNATE_ROUTE))
        {
            routesToChange.push_back(rout);
        }
    }

    if (routesToChange.size() != 2) // We have only two possible routes: Main and Alternative
    {
        std::string msg = std::to_string(routesToChange.size()) + "routes were changed while changeRout" \
                                                          "procedure, but 2 expected";
        cRuntimeError("%s", msg.c_str());
    }

    for (Ipv4Route* rout: routesToChange)
        changeMetric(rout);

}

std::vector<pair_ss> SimpleObserver::collectLinksArray()
{
    std::unordered_set<pair_ss, pair_hash> tmp_res;

    for (int nodeIdx = 0; nodeIdx < topology.getNumNodes(); ++nodeIdx) 
    {
        NetworkConfiguratorBase::Node* node = (NetworkConfiguratorBase::Node *)topology.getNode(nodeIdx);
        for (int outLinkIdx = 0; outLinkIdx < node->getNumOutLinks(); ++outLinkIdx)
        {
            Topology::LinkOut* link = node->getLinkOut(outLinkIdx);
            Topology::Node* srcNode = link->getLocalNode();
            Topology::Node* destNode = link->getRemoteNode();

            std::string linkFromNodePath = srcNode->getModule()->getName();
            std::string linkToNodePath = destNode->getModule()->getName();

            // link is stored in alphabetical order of node names to force uniqness
            linkFromNodePath < linkToNodePath ?
                          tmp_res.emplace(utils::addRootPrefix(linkFromNodePath), utils::addRootPrefix(linkToNodePath)) :
                          tmp_res.emplace(utils::addRootPrefix(linkToNodePath), utils::addRootPrefix(linkFromNodePath)); 
        }
    }
    return std::vector<pair_ss>(tmp_res.begin(), tmp_res.end());
}


std::vector<pair_ss> SimpleObserver::getLinksOnPathBySrc(const std::string& srcNodePath)
{
    std::vector<pair_ss> res;
    std::string destNodePath = srcToDest[srcNodePath];
    
    L3AddressResolver addrRes;

    Ipv4Address destAddr = addrRes.addressOf(getModuleByPath(destNodePath.c_str())).toIpv4();

    for (
        auto* pathNode = (Ipv4NetworkConfigurator::Node*) topology.getNodeFor(getModuleByPath(srcNodePath.c_str()));
        addrRes.addressOf(pathNode->module).toIpv4() != destAddr;
    )
    {

        auto* nextNode = (Ipv4NetworkConfigurator::Node*)findLinkOut(
            pathNode,
            pathNode->routingTable->findBestMatchingRoute(destAddr)->getInterface()->getNodeOutputGateId()
        )->getRemoteNode();

        std::string linkFromNodePath = pathNode->module->getName();
        std::string linkToNodePath = nextNode->module->getName();

        linkFromNodePath < linkToNodePath ?
                           res.emplace_back(utils::addRootPrefix(linkFromNodePath), utils::addRootPrefix(linkToNodePath)) :
                           res.emplace_back(utils::addRootPrefix(linkToNodePath), utils::addRootPrefix(linkFromNodePath)); 
        
        pathNode = nextNode;
    }
    return res;
}

std::vector<pair_ss> SimpleObserver::getLinksOnPathByFlowId(const int flowId)
{
    return getLinksOnPathBySrc(flowIdToSourcePath[flowId]);
}



void SimpleObserver::readSrcDestToFstLstHopsConf()
{
    std::ifstream csvInStream(srcDestToFstLstHopsFp);

    // std::cout << "csvSep is " << csvSep << std::endl;

    for (CSVIterator csvIter(csvInStream, csvSep); csvIter != CSVIterator(); ++csvIter)
    {
        // std::cout << ((*csvIter)[0]) << " " << ((*csvIter)[1]) << " " << ((*csvIter)[2]) << " " << ((*csvIter)[3]) << std::endl;
        std::string srcNodePath = utils::addRootPrefix(((*csvIter)[0]));
        std::string destNodePath = utils::addRootPrefix(((*csvIter)[1]));
        std::string fstHopPath = utils::addRootPrefix(((*csvIter)[2]));
        std::string lstHopPath = utils::addRootPrefix(((*csvIter)[3]));

        srcToDest[srcNodePath] = destNodePath; // fill [src -> dest] table


        pair_ss srcDest(srcNodePath, destNodePath);
        pair_ss fstLstHops(fstHopPath, lstHopPath);

        srcDestToFstLstHops[srcDest] = fstLstHops;
    }
}

void SimpleObserver::readHostToMidHopCandidatesConf()
{
    std::ifstream csvInStream(hostToMidHopCandidatesFp);


    for (CSVIterator csvIter = CSVIterator(csvInStream, csvSep); csvIter != CSVIterator(); ++csvIter)
    {
        std::string hostPath =  utils::addRootPrefix(((*csvIter)[0]));

        if ((*csvIter).size() != 3)
        {
            std::string msg = hostPath + " has " + 
                    std::to_string((*csvIter).size()) + " possible next hops," + \
                    " while 2 where expected.";

            cRuntimeError("%s", msg.c_str());   
        }
        pair_ss nextHopCandidates(utils::addRootPrefix(((*csvIter)[1])),
                                  utils::addRootPrefix(((*csvIter)[2])));
        hostToMidHopCandidates[hostPath] = nextHopCandidates;
    }
}

void SimpleObserver::readFlowIdToSource()
{
    std::ifstream csvInStream(flowIdToSourceFp);

    int rows_cnt = 0;
    for (CSVIterator csvIter = CSVIterator(csvInStream, csvSep); csvIter != CSVIterator(); ++csvIter)
    {
        if ((*csvIter).size() != 2)
        {
            std::string msg = "FlowIdToSource mapping must contain only two columns, " 
                              "but " + flowIdToSourceFp + "has " + std::to_string((*csvIter).size()) + " columns.";

            cRuntimeError("%s", msg.c_str());   
        }
        
        int flowId =  atoi(((*csvIter)[0]).data());
        std::string sourcePath = utils::addRootPrefix(((*csvIter)[1]));
        flowIdToSourcePath[flowId] = sourcePath;
        sourcePathToFlowId[sourcePath] = flowId;

        ++rows_cnt;
    }

    if (rows_cnt != nFlows)
    {
        std::string msg = flowIdToSourceFp + "has " + std::to_string(rows_cnt) + " rows, "
                          "but number of flows must be  ==  " + std::to_string(nFlows);

        cRuntimeError("%s", msg.c_str());  
    }
}


std::unordered_map<int, std::string> SimpleObserver::getFlowIdToPredFilesMapping()
{
    std::ifstream csvInStream(flowIdToPredFilesFp);
    std::unordered_map<int, std::string> res;

    int rows_cnt = 0;
    for (CSVIterator csvIter = CSVIterator(csvInStream, csvSep); csvIter != CSVIterator(); ++csvIter)
    {
        if ((*csvIter).size() != 2)
        {
            std::string msg = "FlowIdToPredFiles mapping must contain only two columns, " 
                              "but " + flowIdToPredFilesFp + "has " + std::to_string((*csvIter).size()) + " columns.";

            cRuntimeError("%s", msg.c_str());   
        }
        
        int flowId =  atoi(((*csvIter)[0]).data());
        std::string predFp = ((*csvIter)[1]);
        res[flowId] = predFp;

        ++rows_cnt;
    }

    if (rows_cnt != nFlows)
    {
        std::string msg = flowIdToSourceFp + "has " + std::to_string(rows_cnt) + " rows, "
                          "but number of flows must be  ==  " + std::to_string(nFlows);

        cRuntimeError("%s", msg.c_str());  
    }

    return res;
}

void SimpleObserver::fillFlowIdToPredIter(std::unordered_map<int, std::string> flowIdToPredFiles)
{
    for (auto const& [flowId, predFp] : flowIdToPredFiles)
    {   flowIdToCSVIfstream[flowId] = std::ifstream(predFp);
        // `++` -- skipping header of CSV file 
        flowIdToPredIter[flowId] = ++CSVIterator(flowIdToCSVIfstream[flowId], csvSep);
    }
}


void SimpleObserver::fillRoutingBitmaskIdxToFlowId()
{
    int bimtaskIdx = 0;
    for (auto const& [flowId, _] : flowIdToSourcePath)
    {
        routingBitmaskIdxToFlowId[bimtaskIdx] = flowId;
        ++bimtaskIdx;
    }
}


std::unordered_map<int, double> SimpleObserver::getCurrentPrediction()
{
    std::unordered_map<int, double> res;
    for (auto const& [flowId, csvIter] : flowIdToPredIter)
    {
        res[flowId] = atof((*csvIter)[utils::algNameToEnum(trafficEngAlgName)].data());
    }
    return res;
}

std::vector<int> SimpleObserver::getFlowIdsArr()
{
    std::vector<int> flowIds;
    flowIds.reserve(nFlows);

    for (auto const& [flowId, _] : flowIdToSourcePath)
        flowIds.push_back(flowId);

    return flowIds;
}

void SimpleObserver::fastForwardPredCSVIters()
{
    for (auto& [_, csvIter] : flowIdToPredIter)
        fastForwardCSVIterUntilTime(csvIter, timePredStart, utils::CSVPredPos::T_START);
}
void SimpleObserver::incrPredCSVIters()
{
    for (auto& [_, csvIter] : flowIdToPredIter)
        ++csvIter;
}

int SimpleObserver::numInitStages() const {return NUM_INIT_STAGES;}



void SimpleObserver::mPrintTopology()
{
    for (int i = 0; i < topology.getNumNodes(); i++) {
        NetworkConfiguratorBase::Node* node = 
                        (NetworkConfiguratorBase::Node *)topology.getNode(i);
        
        if (node) {
            EV << "Node i=" << i << " is " << node->getModule()->getFullPath() << endl;
            EV << " It has " << node->getNumOutLinks() << " conns to other nodes\n";
            EV << " and " << node->getNumInLinks() << " conns from other nodes\n";


            EV << "Routing table is: " << endl;
            check_and_cast<IIpv4RoutingTable *>(node->routingTable)->printRoutingTable();


            EV << " Connections to other modules are:\n";
            for (int j = 0; j < node->getNumOutLinks(); j++) {
                Topology::Node *neighbour = node->getLinkOut(j)->getRemoteNode();
                cGate *gate = node->getLinkOut(j)->getLocalGate();
                EV << " " << neighbour->getModule()->getFullPath()
                << " through gate " << gate->getFullName() << endl;
            }
        }
        else 
        {
            EV << "Failed to cast!";
        }

    }
}

