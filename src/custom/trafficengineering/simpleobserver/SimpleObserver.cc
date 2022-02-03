#include <fstream>
#include <cstdlib>

#include "custom/trafficengineering/simpleobserver/SimpleObserver.h"
#include "custom/queueing/source/CSVUtil/CSVIterator.h"

#include "inet/networklayer/configurator/base/NetworkConfiguratorBase.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/Topology.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/networklayer/configurator/ipv4/Ipv4NetworkConfigurator.h"

Define_Module(SimpleObserver);

using namespace inet;

std::string addRootPrefix(std::string s)
{
    return "<root>." + s;
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

void SimpleObserver::applyRandomRoutingBimask() 
{
    int randBitmaskIdx = rand() % routingBitmasks.size();
    applyRoutingBimask(routingBitmasks[randBitmaskIdx]);
}

void SimpleObserver::applyRoutingBimask(std::vector<bool> bitmask)
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

void SimpleObserver::initialize(int stage)
{
    // const char* nedTypesStr = "inet.node.inet.StandardHost "
    //                           "inet.node.inet.Router";

    Ipv4NetworkConfigurator::initialize(stage);

    if (stage == INITSTAGE_LOCAL)
    {
        csvSep = par("csvSep");
        srcDestToFstLstHopsFp = std::string(par("srcDestToFstLstHopsFp"));
        dynHopsToNextHopsFp = std::string(par("dynHopsToNextHopsFp"));

        readSrcDestToFstLstHopsConf();
        readDynHopsToNextHopsConf();
    }

    if (stage == INITSTAGE_LAST) 
    {
        EV << "Topology before prepareRoutingTables(): " << endl;
        mPrintTopology();

        prepareRoutingTables();

        EV << "Topology after prepareRoutingTables(): " << endl;
        mPrintTopology();

        linksArr = collectLinksArray();
        std::cout << "Size of linksArr = " << linksArr.size() << "linksArr[0] = (" << linksArr[0].first << ", " << linksArr[0].second << ") \n"; 

        mObserverTimer = new ClockEvent("ObserverTimer");
        scheduleObserverTimer(0.002, mObserverTimer); // HARDCODED DELAY==2e-3
        EV << "mObserverTimer Message scheduled";
        
    }
}


void SimpleObserver::scheduleObserverTimer(clocktime_t delay, ClockEvent* msg)
{
    cSimpleModule::scheduleAfter(delay, msg);
}

void SimpleObserver::handleMessage(cMessage *msg)
{  
    EV << "Entering mObserverTimer msg handler1" << endl;
    if (msg == mObserverTimer)
    {
        EV << "Entering mObserverTimer msg handler2" << endl;
        ////

        ////
        // changeRout(fstHop, lstHop, Ipv4Address("10.0.0.1"), Ipv4Address("10.0.0.22"));

        //
        // changeRoutForSource("<root>.client1");
        // auto linksOnPath = getLinksOnPathBySrc("<root>.client1");
        // EV << "There are  " << linksOnPath.size() << " Links on path" << endl;
        // EV << "(" << linksOnPath[0].first << ", " << linksOnPath[0].second << ")" << "(" << linksOnPath[1].first << ", " << linksOnPath[1].second << ")" << "(" << linksOnPath[2].first << ", " << linksOnPath[2].second << ")" << "(" << linksOnPath[3].first << ", " << linksOnPath[3].second << ")" << endl;
        applyRandomRoutingBimask();
        scheduleObserverTimer(0.002, mObserverTimer); // HARDCODED DELAY==2e-3
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




void SimpleObserver::prepareRoutingTableForDynamicHop(const std::string& hopNodePath, const std::string& destNodePath)
{
    Ipv4NetworkConfigurator::Node* hopNode = (Ipv4NetworkConfigurator::Node*)
                                                    topology.getNodeFor(getModuleByPath(hopNodePath.c_str()));

    IIpv4RoutingTable* rtable = (IIpv4RoutingTable*)hopNode->routingTable;
    
    addRouteForDynamicHop(rtable, dynamicHopToNextHopCandidates[hopNodePath].first, destNodePath, true);
    addRouteForDynamicHop(rtable, dynamicHopToNextHopCandidates[hopNodePath].second, destNodePath, false);
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
        prepareRoutingTableForDynamicHop(fstLstHops.first, srcDest.second);
        prepareRoutingTableForDynamicHop(fstLstHops.second, srcDest.first);
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

    if (routesToChange.size() != 2) // Remove Hardcoded 2 routes
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
                          tmp_res.emplace(addRootPrefix(linkFromNodePath), addRootPrefix(linkToNodePath)) :
                          tmp_res.emplace(addRootPrefix(linkToNodePath), addRootPrefix(linkFromNodePath)); 
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
                           res.emplace_back(addRootPrefix(linkFromNodePath), addRootPrefix(linkToNodePath)) :
                           res.emplace_back(addRootPrefix(linkToNodePath), addRootPrefix(linkFromNodePath)); 
        
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
        std::string srcNodePath = addRootPrefix(((*csvIter)[0]));
        std::string destNodePath = addRootPrefix(((*csvIter)[1]));
        std::string fstHopPath = addRootPrefix(((*csvIter)[2]));
        std::string lstHopPath = addRootPrefix(((*csvIter)[3]));

        srcToDest[srcNodePath] = destNodePath; // fill [src -> dest] table


        pair_ss srcDest(srcNodePath, destNodePath);
        pair_ss fstLstHops(fstHopPath, lstHopPath);

        srcDestToFstLstHops[srcDest] = fstLstHops;
    }
}

void SimpleObserver::readDynHopsToNextHopsConf()
{
    std::ifstream csvInStream(dynHopsToNextHopsFp);


    for (CSVIterator csvIter = CSVIterator(csvInStream, csvSep); csvIter != CSVIterator(); ++csvIter)
    {
        std::string dynHopPath =  addRootPrefix(((*csvIter)[0]));

        if ((*csvIter).size() != 3)
        {
            std::string msg = dynHopPath + " has " + 
                    std::to_string((*csvIter).size()) + " possible next hops," + \
                    " while 2 where expected.";

            cRuntimeError("%s", msg.c_str());   
        }
        pair_ss nextHopCandidates(addRootPrefix(((*csvIter)[1])),
                                  addRootPrefix(((*csvIter)[2])));
        dynamicHopToNextHopCandidates[dynHopPath] = nextHopCandidates;
    }
}



int SimpleObserver::numInitStages() const {return NUM_INIT_STAGES;}

