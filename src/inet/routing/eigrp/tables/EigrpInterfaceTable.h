//
// Copyright (C) 2009 - today Brno University of Technology, Czech Republic
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
/**
 * @author Jan Zavrel (honza.zavrel96@gmail.com)
 * @author Jan Bloudicek (jbloudicek@gmail.com)
 * @author Vit Rek (rek@kn.vutbr.cz)
 * @author Vladimir Vesely (ivesely@fit.vutbr.cz)
 * @copyright Brno University of Technology (www.fit.vutbr.cz) under GPLv3
 */


#ifndef __INET_EIGRPINTERFACETABLE_H_
#define __INET_EIGRPINTERFACETABLE_H_

#include "inet/common/INETDefs.h"
#include "inet/common/ModuleAccess.h"
#include "inet/routing/eigrp/EigrpTimer_m.h"
#include "inet/networklayer/common/InterfaceEntry.h"

namespace inet {
namespace eigrp {

class EigrpInterface: public cObject
{
protected:
  int interfaceId;        /**< ID of interface */
  const char *interfaceName;  /**< Name of interface */
  int networkId;          /**< ID of network in RoutingOfNetworks table */ //TODO - PROB-03 - only 1 network per interface
  std::set<int>networksIds; /**< IDs of IPv6 networks in RoutingOfNetworks table */
  int helloInt;           /**< Hello interval in seconds (<1-65535>) */
  int holdInt;            /**< Router's hold interval in seconds (<1-65535>) */
  EigrpTimer *hellot;     /**< pointer to hello timer */
  bool enabled;           /**< EIGRP is enabled on interface */
  bool splitHorizon;      /**< Split horizon rule enabled */
  bool passive;           /**< Interface is passive */
  double interfaceDatarate; /**< Datarate of InterfaceEntry interace */

  uint64_t bandwidth;       /**< Bandwidth in Kbps (<1-10 000 000>) */
  uint64_t delay;           /**< Delay in us (<1-16 777 215>) */
  int reliability;        /**< Reliability in percent (<1-255>) */
  int load;               /**< Load in percent (<1-255>) */
  int mtu;                /**< MTU of interface in B */

  // Statistics for optimization
  int relMsgs;            /**< Number of messages waiting for sending on the interface */
  int pendingMsgs;        /**< Number of reliable sent messages but not acknowledged */

  int neighborCount;      /**< Number of neighbors on interface */
  int stubCount;          /**< Number of stub neighbors on interface */

public:
  EigrpInterface(InterfaceEntry *iface, int networkId, bool enabled);
  ~EigrpInterface();

  bool operator==(const EigrpInterface& iface) const
  {
      return interfaceId == iface.getInterfaceId();
  }

  //-- Setters and getters
  void setInterfaceId(int id) { interfaceId = id; }
  int getInterfaceId() const { return interfaceId; }

  void setHelloTimerPtr(EigrpTimer *timer) { this->hellot = timer; }
  EigrpTimer *getHelloTimer() const { return this->hellot; }

  void setHelloInt(int helloInt) { this->helloInt = helloInt; }
  int getHelloInt() const { return this->helloInt; }

  void setHoldInt(int holdInt) { this->holdInt = holdInt; }
  int getHoldInt() const { return this->holdInt; }

  void setEnabling(bool enabled) { this->enabled = enabled; }
  bool isEnabled() const { return this->enabled; }

  void setNetworkId(int netId) { this->networkId = netId; }
  int getNetworkId() const { return this->networkId; }

  void decNumOfNeighbors() { this->neighborCount--; }
  void incNumOfNeighbors() { this->neighborCount++; }
  int getNumOfNeighbors() const { return this->neighborCount; }

  void decNumOfStubs() { this->stubCount--; }
  void incNumOfStubs() { this->stubCount++; }
  int getNumOfStubs() const { return this->stubCount; }

  void setBandwidth(uint64_t bw) { this->bandwidth = bw; }
  uint64_t getBandwidth() const { return bandwidth; }

  void setInterfaceDatarate(double datarate);
  double getInterfaceDatarate() const {return interfaceDatarate;}


  void setDelay(uint64_t dly) { this->delay = dly; }
  uint64_t getDelay() const { return delay; }

  void setReliability(int rel) { this->reliability = rel; }
  int getReliability() const { return reliability; }

  void setLoad(int load) { this->load = load; }
  int getLoad() const { return load; }

  void setMtu(int mtu) { this->mtu = mtu; }
  int getMtu() const { return mtu; }

  bool isMulticastAllowedOnIface(InterfaceEntry *iface);

  void setSplitHorizon(bool shEnabled) { this->splitHorizon = shEnabled; }
  bool isSplitHorizonEn() const { return this->splitHorizon; }

  void setRelMsgs(int cnt) { this->relMsgs = cnt; }
  int getRelMsgs() const { return relMsgs; }

  void setPendingMsgs(int cnt) { this->pendingMsgs = cnt; }
  int getPendingMsgs() const { return pendingMsgs; }
  void incPendingMsgs() { this->pendingMsgs++; }
  void decPendingMsgs() { this->pendingMsgs--; }

  void setPassive(bool passive) { this->passive = passive; }
  bool isPassive() const { return this->passive; }

  const char *getInterfaceName() const { return this->interfaceName; }

  /**
   * Get networkIds set .begin()
   */
  std::set<int>::iterator getNetworksIdsBegin() {return networksIds.begin();}
  /**
   * Get networksIds set .end()
   */
  std::set<int>::iterator getNetworksIdsEnd() {return networksIds.end();}
  /**
   * Insert networkID into set
   */
  std::pair<std::set<int>::iterator, bool> insertToNetworksIds(int netId) {return networksIds.insert(netId);}
  /**
   * Clears networksIds set
   */
  void clearNetworkIds() {networksIds.clear();}
};


/*
 * T��da reprezentuj�c� tabulku EIGRP-ENABLED interfac�
 */

class EigrpInterfaceTable : public cSimpleModule
{
  protected:
    typedef typename std::vector<EigrpInterface *> InterfaceVector;

    InterfaceVector eigrpInterfaces;

    virtual void initialize(int stage);
    virtual void handleMessage(cMessage *msg);
    virtual int numInitStages() const { return NUM_INIT_STAGES; }

    /**
     * Stops hello timer.
     */
    void cancelHelloTimer(EigrpInterface *iface);

  public:
    ~EigrpInterfaceTable();

    /**
     * Adds interface to table.
     */
    void addInterface(EigrpInterface *interface);
    /**
     * Removes interface from table
     */
    EigrpInterface *removeInterface(EigrpInterface *iface);

    /**
     * Gets interface from table by interface ID.
     */
    EigrpInterface *findInterfaceById(int ifaceId);
    /**
     * Returns number of interfaces in the table.
     */
    int getNumInterfaces() const { return eigrpInterfaces.size(); }
    /**
     * Returns interface by its position in the table.
     */
    EigrpInterface *getInterface(int k) const { return eigrpInterfaces[k]; }
};


} //namespace eigrp
} //namespace inet

#endif