/****************************************************************************
  FileName     [ v3NtkTemDecomp.cpp ]
  PackageName  [ v3/src/trans ]
  Synopsis     [ Temporal Decomposition to V3 Network. ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef V3_NTK_TEMDECOMP_C
#define V3_NTK_TEMDECOMP_C

#include "v3Msg.h"
#include "v3NtkUtil.h"
#include "v3StrUtil.h"
#include "v3NtkTemDecomp.h"

/* -------------------------------------------------- *\
 * Class V3NtkTemDecomp Implementations
\* -------------------------------------------------- */
// Constructor and Destructor
V3NtkTemDecomp::V3NtkTemDecomp(V3NtkHandler* const p, const uint32_t& cycle, const bool& init)
   : V3NtkHandler(p, createV3Ntk(dynamic_cast<V3BvNtk*>(p->getNtk()))), _cycle(cycle) {
   assert (_handler); assert (_ntk); assert (cycle);
   assert (!_handler->getNtk()->getModuleSize());  // Module Instance will be Changed !!
   // Perform Network Transformation
   _c2pMap.clear(); _p2cMap.clear(); performNtkTransformation(init);
   // Reserve Mapping Tables if Needed
   if (!V3NtkHandler::P2CMapON()) _p2cMap.clear();
   if (!V3NtkHandler::C2PMapON()) _c2pMap.clear();
}

V3NtkTemDecomp::~V3NtkTemDecomp() {
   _c2pMap.clear(); _p2cMap.clear();
}

// I/O Ancestry Functions
const string
V3NtkTemDecomp::getInputName(const uint32_t& index) const {
   // Current Network
   assert (_ntk); if (index >= _ntk->getInputSize()) return "";
   V3NetStrHash::const_iterator it = _netHash.find(_ntk->getInput(index).id);
   if (it != _netHash.end()) { assert (!_ntk->getInput(index).cp); return it->second; }
   // Parent Networks
   string name = "";
   if (_handler) {
      const V3Ntk* const ntk = _handler->getNtk(); assert (ntk);
      const uint32_t latchIndex = _ntk->getInputSize() - ntk->getLatchSize();
      if (index >= latchIndex) {
         name = _handler->getNetName(_handler->getNtk()->getLatch(index - latchIndex));
         if (name.size()) name += (V3AuxExpansionName + "0");
      }
      else {
         name = _handler->getInputName(index % ntk->getInputSize());
         if (name.size()) name += (V3AuxExpansionName + v3Int2Str(index / ntk->getInputSize()));
      }
   }
   return name.size() ? name : getNetNameOrFormedWithId(_ntk->getInput(index));
}

const string
V3NtkTemDecomp::getOutputName(const uint32_t& index) const {
   // Current Network
   assert (_ntk); if (index >= _ntk->getOutputSize()) return "";
   V3IdxStrHash::const_iterator it = _outIndexHash.find(index);
   if (it != _outIndexHash.end()) return it->second;
   // Parent Networks
   string name = "";
   if (_handler) {
      const V3Ntk* const ntk = _handler->getNtk(); assert (ntk);
      name = _handler->getOutputName(index % ntk->getOutputSize());
      if (name.size()) name += (V3AuxExpansionName + v3Int2Str(index / ntk->getOutputSize()));
   }
   return name;
}

const string
V3NtkTemDecomp::getInoutName(const uint32_t& index) const {
   // Current Network
   assert (_ntk); if (index >= _ntk->getInoutSize()) return "";
   V3NetStrHash::const_iterator it = _netHash.find(_ntk->getInout(index).id);
   if (it != _netHash.end()) { assert (!_ntk->getInout(index).cp); return it->second; }
   // Parent Networks
   string name = "";
   if (_handler) {
      const V3Ntk* const ntk = _handler->getNtk(); assert (ntk);
      name = _handler->getInoutName(index % ntk->getInoutSize());
      if (name.size()) name += (V3AuxExpansionName + v3Int2Str(index / ntk->getInoutSize()));
   }
   return name.size() ? name : getNetNameOrFormedWithId(_ntk->getInout(index));
}

// Net Ancestry Functions
void
V3NtkTemDecomp::getNetName(V3NetId& id, string& name) const {
   if (V3NetUD == id) return;
   // Current Network
   if (!id.cp) {
      V3NetStrHash::const_iterator it = _netHash.find(id.id);
      if (it != _netHash.end()) { name = it->second; return; }
   }
   // Parent Networks
   if (_handler) {
      const V3NetId netId = id; id = getParentNetId(id); _handler->getNetName(id, name); if (!name.size()) return;
      for (uint32_t i = 0; i < _cycle; ++i)
         if (netId.id == _p2cMap[i][id.id].id) { name += (V3AuxExpansionName + v3Int2Str(i)); break; }
   }
}

const V3NetId
V3NtkTemDecomp::getNetFromName(const string& s) const {
   // Try the full name first
   V3NetId id = V3NtkHandler::getNetFromName(s); if (V3NetUD != id) return id;
   // Try dropping the "@frame" suffix
   const size_t pos = s.find_last_of(V3AuxExpansionName); if (string::npos == pos) return V3NetUD;
   const string name = s.substr(0, 1 + pos - V3AuxExpansionName.size()); if (!name.size()) return V3NetUD; int index;
   if (!v3Str2Int(s.substr(pos + 1), index) || index < 0 || index >= (int)_cycle) return V3NetUD;
   // Parent Networks
   if (!_handler) return V3NetUD; id = _handler->getNetFromName(name);
   return (V3NetUD == id) ? id : getCurrentNetId(id, index);
}

const V3NetId
V3NtkTemDecomp::getParentNetId(const V3NetId& id) const {
   if (V3NetUD == id || _c2pMap.size() <= id.id || V3NetUD == _c2pMap[id.id]) return V3NetUD;
   return isV3NetInverted(id) ? getV3InvertNet(_c2pMap[getV3NetIndex(id)]) : _c2pMap[getV3NetIndex(id)];
}

const V3NetId
V3NtkTemDecomp::getCurrentNetId(const V3NetId& id, const uint32_t& index) const {
   if (V3NetUD == id || !_p2cMap.size() || _p2cMap[0].size() <= id.id) return V3NetUD; assert (index < _cycle);
   return isV3NetInverted(id) ? getV3InvertNet(_p2cMap[index][getV3NetIndex(id)]) : _p2cMap[index][getV3NetIndex(id)];
}

// Transformation Functions
void
V3NtkTemDecomp::performNtkTransformation(const bool& init) {
   V3Ntk* const ntk = _handler->getNtk(); assert (ntk);

   const uint32_t parentNets = ntk->getNetSize(); assert (parentNets);
   const bool isBvNtk = dynamic_cast<V3BvNtk*>(ntk);
   assert(!isBvNtk);

   _p2cMap = V3NetTable(_cycle, V3NetVec(parentNets, V3NetUD));

   for (uint32_t cycle = 0; cycle < _cycle; ++cycle) _p2cMap[cycle][0] = V3NetId::makeNetId(0);

   // Compute DFS Order for Transformation
   V3NetVec orderMap; orderMap.clear(); dfsNtkForGeneralOrder(ntk, orderMap);
   assert (orderMap.size() <= parentNets); assert (!orderMap[0].id);

   V3NetId id; V3InputVec inputs; inputs.reserve(3); inputs.clear();
   uint32_t i_for_latch = ntk->getInputSize();
   // Expand Network
   for (uint32_t cycle = 0; cycle < _cycle; ++cycle) {
      V3NetVec& p2cMap = _p2cMap[cycle]; assert (parentNets == p2cMap.size());
      // Construct PI / PIO in Consistent Order
      uint32_t i = 1;
      for (uint32_t j = i + ntk->getInputSize(); i < j; ++i) {
         assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
         p2cMap[orderMap[i].id] = _ntk->createNet(ntk->getNetWidth(orderMap[i]));
         assert (V3_PI == ntk->getGateType(orderMap[i]));
         _ntk->createInput(p2cMap[orderMap[i].id]);
      }
      // Construct Latches in Consistent Order
      for (uint32_t j = i + ntk->getLatchSize(); i < j; ++i) {
         assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
         assert (V3_FF == ntk->getGateType(orderMap[i]));

         if (cycle == _cycle - 1) {  // Connect FF with the Last Frame (x)
                                     // if last frame, set as zero (o)
            const V3NetId id1 = ntk->getInputNetId(orderMap[i], 1); assert (id1.id < ntk->getNetSize());
            if (!id1.id) p2cMap[orderMap[i].id] = id1;
            else if (id1 == orderMap[i]) p2cMap[orderMap[i].id] = _ntk->createNet(ntk->getNetWidth(orderMap[i]));
            else p2cMap[orderMap[i].id] = V3NetId::makeNetId(p2cMap[id1.id].id, p2cMap[id1.id].cp ^ id1.cp);
            //cout << "FF0 " << orderMap[i].id << endl;
            //cout << "FF0 " << p2cMap[orderMap[i].id].id << endl;
         }
         else if(cycle){  // Connect with cycle - 1 frame

            id = ntk->getInputNetId(orderMap[i], 0); assert (V3NetUD != _p2cMap[cycle - 1][id.id]);
            p2cMap[orderMap[i].id] = V3NetId::makeNetId(_p2cMap[cycle - 1][id.id].id,
                                                        _p2cMap[cycle - 1][id.id].cp ^ id.cp);
            //cout << "FF " << orderMap[i].id << endl;
            //cout << "FF " << p2cMap[orderMap[i].id].id << endl;

         }
      }
      // Construct Nets and Connections
      for (; i < orderMap.size(); ++i) {
         assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
         const V3GateType& type = ntk->getGateType(orderMap[i]); assert (V3_XD > type);
         p2cMap[orderMap[i].id] = _ntk->createNet(ntk->getNetWidth(orderMap[i]));
         //cout << "AIG " << orderMap[i].id << endl;
         //cout << "AIG " << p2cMap[orderMap[i].id].id << endl;
         if (AIG_NODE == type) {
            id = ntk->getInputNetId(orderMap[i], 0); assert (V3NetUD != p2cMap[id.id]);
            inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
            id = ntk->getInputNetId(orderMap[i], 1); assert (V3NetUD != p2cMap[id.id]);
            inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
         }
         else { _ntk->createConst(p2cMap[orderMap[i].id]); continue; }
         _ntk->setInput(p2cMap[orderMap[i].id], inputs); inputs.clear();
         _ntk->createGate(ntk->getGateType(orderMap[i]), p2cMap[orderMap[i].id]);
      }
      // Construct PO in Consistent Order
      for (i = 0; i < ntk->getOutputSize(); ++i) {
         id = ntk->getOutput(i); assert (V3NetUD != p2cMap[id.id]);
         //cout << id.id << endl;
         _ntk->createOutput(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
      }
   }
   /* set frame 0*/

   for (uint32_t j = i_for_latch + ntk->getLatchSize(); i_for_latch < j; ++i_for_latch) {
      if(!_cycle) continue;
      assert (parentNets > orderMap[i_for_latch].id); assert (V3NetUD == p2cMap[orderMap[i_for_latch].id]);
      assert (V3_FF == ntk->getGateType(orderMap[i_for_latch]));

      _p2cMap[0][orderMap[i_for_latch].id] = _ntk->createNet(1);
      id = ntk->getInputNetId( orderMap[i_for_latch], 0 );
      inputs.push_back(V3NetId::makeNetId(_p2cMap[0][id.id].id, _p2cMap[0][id.id].cp ^ id.cp));
      inputs.push_back(V3NetId::makeNetId(_p2cMap[1][id.id].id, _p2cMap[1][id.id].cp ^ id.cp));

      _ntk->setInput(_p2cMap[0][orderMap[i_for_latch].id], inputs); inputs.clear();
      _ntk->createGate(ntk->getGateType(orderMap[i_for_latch]), _p2cMap[0][orderMap[i_for_latch].id]);

      //const V3NetId id1 = ntk->getInputNetId(orderMap[i_for_latch], 1); assert (id1.id < ntk->getNetSize());
      //_p2cMap[0][orderMap[i_for_latch].id] = V3NetId::makeNetId(_p2cMap[0][id1.id].id, _p2cMap[0][id1.id].cp ^ id1.cp);
   }
   // Set Latches in Consistent Order as Primary Inputs
   for (uint32_t i = 0; i < ntk->getLatchSize(); ++i) {
      id = ntk->getLatch(i); assert (V3NetUD != _p2cMap[0][id.id]);
      //cout << id.id << endl;
      //cout << V3GateTypeStr[_ntk->getGateType(id)] << endl;
      if (V3_PI == _ntk->getGateType(_p2cMap[0][id.id])){
         _ntk->createInput(_p2cMap[0][id.id]);
         cout << "setted some latched to be free input\n";
      }
      //cout << V3GateTypeStr[_ntk->getGateType(_p2cMap[0][id.id])] << endl;
   }
   // Complete Mapping Table from Current Ntk to Parent Ntk
   _c2pMap = V3NetVec(_ntk->getNetSize(), V3NetUD); _c2pMap[0] = V3NetId::makeNetId(0);
   for (uint32_t cycle = 0; cycle < _cycle; ++cycle) {
      V3NetVec& p2cMap = _p2cMap[cycle]; assert (_c2pMap[0] == p2cMap[0]);
      for (uint32_t i = 0; i < p2cMap.size(); ++i) {
         if (V3NetUD == p2cMap[i] || V3NetUD != _c2pMap[p2cMap[i].id]) continue;
         else _c2pMap[p2cMap[i].id] = V3NetId::makeNetId(i, p2cMap[i].cp);
      }
   }
}

#endif

