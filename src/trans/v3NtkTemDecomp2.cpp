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
V3NtkTemDecomp::V3NtkTemDecomp(V3NtkHandler* const p, const uint32_t& cycle, V3BitVecX & data, bool first)
   : V3NtkHandler(p, createV3Ntk(dynamic_cast<V3BvNtk*>(p->getNtk()))), _cycle(cycle) {
   assert (_handler); assert (_ntk); assert (cycle);
   assert (!_handler->getNtk()->getModuleSize());  // Module Instance will be Changed !!
   // Perform Network Transformation
   _c2pMap.clear(); _p2cMap.clear(); performNtkTransformation(data, first);
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
V3NtkTemDecomp::performNtkTransformation(V3BitVecX & data, bool first) {
   V3Ntk* const ntk = _handler->getNtk(); assert (ntk);

   const uint32_t parentNets = ntk->getNetSize(); assert (parentNets);
   const bool isBvNtk = dynamic_cast<V3BvNtk*>(ntk);
   assert(!isBvNtk);

   if(!first){
      /*cout << "data size:" << data.size() << endl;
      cout << "data:";
      for (unsigned i = 0; i < data.size(); ++i){
          cout << data[i];
      }
      cout << endl;*/
      _p2cMap = V3NetTable(1, V3NetVec(parentNets, V3NetUD));

      _p2cMap[0][0] = V3NetId::makeNetId(0);

      // Compute DFS Order for Transformation
      V3NetVec orderMap; orderMap.clear(); dfsNtkForGeneralOrder(ntk, orderMap);
      assert (orderMap.size() <= parentNets); assert (!orderMap[0].id);

      V3NetId id; V3InputVec inputs; inputs.reserve(3); inputs.clear();
      uint32_t iS = ntk->getInputSize();
      uint32_t lS = ntk->getLatchSize();
      // Expand Network
      V3NetVec& p2cMap = _p2cMap[0]; assert (parentNets == p2cMap.size());
      // Construct PI / PIO in Consistent Order
      uint32_t i = 1;
      for (uint32_t j = i + iS; i < j; ++i) {
         assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
         p2cMap[orderMap[i].id] = _ntk->createNet(1);
         assert (V3_PI == ntk->getGateType(orderMap[i]));
         _ntk->createInput(p2cMap[orderMap[i].id]);
         //cout << "PI " << orderMap[i].id << endl;
         //cout << "PI " << p2cMap[orderMap[i].id].id << endl;
      }
      for (uint32_t j = i + lS; i < j; ++i) {
         if(data[i-1-iS] != 'X') continue;
         assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
         p2cMap[orderMap[i].id] = _ntk->createNet(1);
         assert (V3_FF == ntk->getGateType(orderMap[i]));
         _ntk->createLatch(p2cMap[orderMap[i].id]);
         //cout << "Latch " << orderMap[i].id << endl;
         //cout << "Latch " << p2cMap[orderMap[i].id].id << endl;
      }
      // Construct Nets and Connections
      for (i = 1+iS+lS; i < orderMap.size(); ++i) {
         assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
         p2cMap[orderMap[i].id] = _ntk->createNet(1);
         //cout << "AIG " << orderMap[i].id << endl;
         //cout << "AIG " << p2cMap[orderMap[i].id].id << endl;
      }
      //cerr << "// Connections\n";
      for (uint32_t i = 1+iS+lS; i < orderMap.size(); ++i) {
         const V3GateType& type = ntk->getGateType(orderMap[i]); assert (V3_XD > type);
         if (AIG_NODE == type) {
            //cerr << "Connecting: " << orderMap[i].id << endl;
            id = ntk->getInputNetId(orderMap[i], 0);
            //cerr << "input1: " << id.id << endl;
            if(ntk->getGateType(id) == V3_FF){
               for(uint32_t ii = 0; ii < ntk->getLatchSize(); ii++){
                  if(id.id == ntk->getLatch(ii).id){
                     if(data[ii] == '0'){
                        p2cMap[id.id] = V3NetId::makeNetId(0,p2cMap[id.id].cp ^ id.cp);
                        inputs.push_back(p2cMap[id.id]);
                     }
                     else if(data[ii] == '1'){
                        p2cMap[id.id] = V3NetId::makeNetId(0, p2cMap[id.id].cp ^ (~id.cp));
                        inputs.push_back(p2cMap[id.id]);
                     }
                     else
                        inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
                  }
               }
            }
            else
               inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));

            id = ntk->getInputNetId(orderMap[i], 1);
            /*cerr << "QAQ? input2: " << id.id << endl;
            cout << "type : " << V3GateTypeStr[ntk->getGateType(id)] << endl;*/
            if(ntk->getGateType(id) == V3_FF){
               for(uint32_t ii = 0; ii < ntk->getLatchSize(); ii++){
                  /*cout << "id.id: " << id.id << " and " << ii << " : " << ntk->getLatch(ii).id << endl;
                  cout << "id == ntk->getLatch(ii)" << (id == ntk->getLatch(ii)) << endl;*/
                  if(id.id == ntk->getLatch(ii).id){
                     //cout << "yes, ii = " << ii << endl;
                     if(data[ii] == '0'){
                        /*cerr << "id.id: " << id.id << endl;
                        cerr << "id.cp " << id.cp << endl;
                        cerr << "p2cMap[id.id].cp " << p2cMap[id.id].cp << endl;*/
                        p2cMap[id.id] = V3NetId::makeNetId(0,p2cMap[id.id].cp ^ id.cp);
                        inputs.push_back(p2cMap[id.id]);
                     }
                     else if(data[ii] == '1'){
                        p2cMap[id.id] = V3NetId::makeNetId(0, p2cMap[id.id].cp ^ (~id.cp));
                        inputs.push_back(p2cMap[id.id]);
                     }
                     else
                        inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
                  }
               }
            }
            else
               inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
         }
         else { _ntk->createConst(p2cMap[orderMap[i].id]); continue; }
         _ntk->setInput(p2cMap[orderMap[i].id], inputs); inputs.clear();
         _ntk->createGate(ntk->getGateType(orderMap[i]), p2cMap[orderMap[i].id]);
      }
      // Construct PO in Consistent Order
      for (uint32_t i = 0; i < ntk->getOutputSize(); ++i) {
         id = ntk->getOutput(i);
         cout << "created output1: " <<  id.id << endl;
         _ntk->createOutput(V3NetId::makeNetId(_p2cMap[0][id.id].id, _p2cMap[0][id.id].cp ^ id.cp));
      }

      // set FF
      for (uint32_t i = 1 + iS; i < 1 + iS + lS; ++i) {
         if(data[i-1-iS] != 'X') continue;
         id = ntk->getInputNetId( orderMap[i], 0 );
         inputs.push_back(V3NetId::makeNetId(_p2cMap[0][id.id].id, _p2cMap[0][id.id].cp ^ id.cp));
         id = ntk->getInputNetId( orderMap[i], 1 );
         inputs.push_back(V3NetId::makeNetId(id.id, id.cp));
         //cout << "id.id:" << id.id << "id.cp:" << id.cp << endl;
         _ntk->setInput(_p2cMap[0][orderMap[i].id], inputs); inputs.clear();
         assert(ntk->getGateType(orderMap[i]) == V3_FF);
         _ntk->createGate(ntk->getGateType(orderMap[i]), _p2cMap[0][orderMap[i].id]);

      }

      _c2pMap = V3NetVec(_ntk->getNetSize(), V3NetUD); _c2pMap[0] = V3NetId::makeNetId(0,0);
      assert (_c2pMap[0] == p2cMap[0]);
      for (uint32_t i = 0; i < p2cMap.size(); ++i) {
         if (V3NetUD == p2cMap[i] || V3NetUD != _c2pMap[p2cMap[i].id]) continue;
         else _c2pMap[p2cMap[i].id] = V3NetId::makeNetId(i, p2cMap[i].cp);
      }
   }
   else{

      _p2cMap = V3NetTable(_cycle, V3NetVec(parentNets, V3NetUD));

      for (uint32_t cycle = 0; cycle < _cycle; ++cycle) _p2cMap[cycle][0] = V3NetId::makeNetId(0,0);

      // Compute DFS Order for Transformation
      V3NetVec orderMap; orderMap.clear(); dfsNtkForGeneralOrder(ntk, orderMap);
      assert (orderMap.size() <= parentNets); assert (!orderMap[0].id);
      for (unsigned i = 0; i < orderMap.size(); ++i){
          cout << "orderMap : " << orderMap[i].id << endl;
      }
      V3NetId id; V3InputVec inputs; inputs.reserve(3); inputs.clear();
      uint32_t iS = ntk->getInputSize();
      uint32_t lS = ntk->getLatchSize();
      // Expand Network
      for (uint32_t cycle = 0; cycle < _cycle; ++cycle) {
         V3NetVec& p2cMap = _p2cMap[cycle]; assert (parentNets == p2cMap.size());
         // Construct PI / PIO in Consistent Order
         cerr << "// Construct PI/Latches in Consistent Order\n";
         uint32_t i = 1;
         for (uint32_t j = i + iS; i < j; ++i) {
            assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
            p2cMap[orderMap[i].id] = _ntk->createNet(1);
            assert (V3_PI == ntk->getGateType(orderMap[i]));
            _ntk->createInput(p2cMap[orderMap[i].id]);
            cout << "PI " << orderMap[i].id << endl;
            cout << "PI " << p2cMap[orderMap[i].id].id << endl;
         }
         for (uint32_t j = i + lS; i < j; ++i) {
            if(cycle) continue;
            assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
            p2cMap[orderMap[i].id] = _ntk->createNet(1);
            assert (V3_FF == ntk->getGateType(orderMap[i]));
            _ntk->createLatch(p2cMap[orderMap[i].id]);
            cout << "Latch " << orderMap[i].id << endl;
            cout << "Latch " << p2cMap[orderMap[i].id].id << endl;
         }
         cerr << "// Construct Nets\n";
         // Construct Nets and Connections
         for (i = 1+iS+lS; i < orderMap.size(); ++i) {
            assert (parentNets > orderMap[i].id); assert (V3NetUD == p2cMap[orderMap[i].id]);
            p2cMap[orderMap[i].id] = _ntk->createNet(1);
            cout << "AIG " << orderMap[i].id << endl;
            cout << "AIG " << p2cMap[orderMap[i].id].id << endl;
         }
      }
      for (uint32_t cycle = 1; cycle < _cycle; ++cycle){
         V3NetVec& p2cMap = _p2cMap[cycle];
         cerr << endl << cycle <<" Connections\n";
         for (uint32_t i = 1+iS; i < orderMap.size(); ++i) {
            const V3GateType& type = ntk->getGateType(orderMap[i]); assert (V3_XD > type);
            if (AIG_NODE == type) {
               cerr << "Connecting: " << orderMap[i].id << ":" << p2cMap[orderMap[i].id].id << endl;

               // input1
               id = ntk->getInputNetId(orderMap[i], 0);
               if(ntk->getGateType(id) == V3_FF){
                  if (cycle == 1) {
                     const V3NetId id1 = ntk->getInputNetId(id, 1);
                     inputs.push_back(V3NetId::makeNetId(p2cMap[id1.id].id, p2cMap[id1.id].cp ^ id1.cp ^ id.cp));
                     cerr << "input1: " << id.id << ":" << id1.id <<":"<<  p2cMap[id1.id].id << endl;
                  }else{
                     V3NetId id1 = ntk->getInputNetId(id, 0); //assert (V3NetUD != p2cMap[id1.id]);
                     inputs.push_back(V3NetId::makeNetId(_p2cMap[cycle-1][id1.id].id, _p2cMap[cycle-1][id1.id].cp ^ id1.cp));
                     cerr << "input1: " << id.id << ":" << id1.id <<":"<<  _p2cMap[cycle-1][id1.id].id << endl;
                  }
               }
               else{
                  inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
                  cerr << "input1: " << id.id << ":" <<  p2cMap[id.id].id << endl;
               }

               // input2
               id = ntk->getInputNetId(orderMap[i], 1);
               if(ntk->getGateType(id) == V3_FF){
                  if (cycle == 1) {
                     const V3NetId id1 = ntk->getInputNetId(id, 1);
                     inputs.push_back(V3NetId::makeNetId(p2cMap[id1.id].id, p2cMap[id1.id].cp ^ id1.cp ^ id.cp));
                     cerr << "input2: " << id.id << ":" << id1.id <<":"<<  p2cMap[id1.id].id << endl;
                  }else{
                     V3NetId id1 = ntk->getInputNetId(id, 0); //assert (V3NetUD != p2cMap[id1.id]);
                     inputs.push_back(V3NetId::makeNetId(_p2cMap[cycle-1][id1.id].id, _p2cMap[cycle-1][id1.id].cp ^ id1.cp));
                     cerr << "input2: " << id.id << ":" << id1.id <<":"<<  _p2cMap[cycle-1][id1.id].id << endl;
                  }
               }
               else{
                  inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
                  cerr << "input2: " << id.id << ":" <<  p2cMap[id.id].id << endl;
               }
            }
            else if (V3_FF == type && cycle > 1){
               id = ntk->getInputNetId(orderMap[i], 0);
               if(ntk->getGateType(id) == V3_FF){
                  if (cycle == 1) {
                     const V3NetId id1 = ntk->getInputNetId(id, 1);
                     inputs.push_back(V3NetId::makeNetId(p2cMap[id1.id].id, p2cMap[id1.id].cp ^ id1.cp ^ id.cp));
                     cerr << "input2: " << id.id << ":" << id1.id <<":"<<  p2cMap[id1.id].id << endl;
                  }else{
                     V3NetId id1 = ntk->getInputNetId(id, 0); //assert (V3NetUD != p2cMap[id1.id]);
                     inputs.push_back(V3NetId::makeNetId(_p2cMap[cycle-1][id1.id].id, _p2cMap[cycle-1][id1.id].cp ^ id1.cp));
                     cerr << "input2: " << id.id << ":" << id1.id <<":"<<  _p2cMap[cycle-1][id1.id].id << endl;
                  }
               }
               else{
                  inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
                  cerr << "input2: " << id.id << ":" <<  p2cMap[id.id].id << endl;
               }
            }
            else { _ntk->createConst(p2cMap[orderMap[i].id]); continue; }
            _ntk->setInput(p2cMap[orderMap[i].id], inputs); inputs.clear();
            _ntk->createGate(ntk->getGateType(orderMap[i]), p2cMap[orderMap[i].id]);
         }
      }
      V3NetVec& p2cMap = _p2cMap[0];
      cerr << endl << "0 Connections\n";
      for (uint32_t i = 1+iS+lS; i < orderMap.size(); ++i) {
         const V3GateType& type = ntk->getGateType(orderMap[i]); assert (V3_XD > type);
         if (AIG_NODE == type) {
            cerr << "Connecting: " << orderMap[i].id << ":" << p2cMap[orderMap[i].id].id << endl;

            // input1
            id = ntk->getInputNetId(orderMap[i], 0);
            if(ntk->getGateType(id) == V3_FF){
               V3NetId id1 = ntk->getInputNetId(id, 0); //assert (V3NetUD != p2cMap[id1.id]);
               inputs.push_back(V3NetId::makeNetId(_p2cMap[_cycle-1][id1.id].id, _p2cMap[_cycle-1][id1.id].cp ^ id1.cp));
               cerr << "input1: " << id.id << ":" << id1.id <<":"<<  _p2cMap[_cycle-1][id1.id].id << endl;
            }
            else{
               inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
               cerr << "input1: " << id.id << ":" <<  p2cMap[id.id].id << endl;
            }

            // input2
            id = ntk->getInputNetId(orderMap[i], 1);
            if(ntk->getGateType(id) == V3_FF){
               V3NetId id1 = ntk->getInputNetId(id, 0); //assert (V3NetUD != p2cMap[id1.id]);
               inputs.push_back(V3NetId::makeNetId(_p2cMap[_cycle-1][id1.id].id, _p2cMap[_cycle-1][id1.id].cp ^ id1.cp));
               cerr << "input2: " << id.id << ":" << id1.id <<":"<<  _p2cMap[_cycle-1][id1.id].id << endl;
            }
            else{
               inputs.push_back(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
               cerr << "input2: " << id.id << ":" <<  p2cMap[id.id].id << endl;
            }
         }
         else { _ntk->createConst(p2cMap[orderMap[i].id]); continue; }
         _ntk->setInput(p2cMap[orderMap[i].id], inputs); inputs.clear();
         _ntk->createGate(ntk->getGateType(orderMap[i]), p2cMap[orderMap[i].id]);
      }


      // Construct PO in Consistent Order
      for (uint32_t i = 0; i < ntk->getOutputSize(); ++i) {
         id = ntk->getOutput(i);
         cout << "created output2: " <<  id.id << endl;
         _ntk->createOutput(V3NetId::makeNetId(_p2cMap[0][id.id].id, _p2cMap[0][id.id].cp ^ id.cp));
      }

      // set FF
      for (uint32_t i = 1 + iS; i < 1 + iS + lS; ++i) {
         if(!_cycle) continue;
         id = ntk->getInputNetId( orderMap[i], 0 );
         inputs.push_back(V3NetId::makeNetId(_p2cMap[0][id.id].id, _p2cMap[0][id.id].cp ^ id.cp));
         inputs.push_back(V3NetId::makeNetId(_p2cMap[_cycle-1][id.id].id, _p2cMap[_cycle-1][id.id].cp ^ id.cp));
         _ntk->setInput(_p2cMap[0][orderMap[i].id], inputs); inputs.clear();
         assert(ntk->getGateType(orderMap[i]) == V3_FF);
         _ntk->createGate(ntk->getGateType(orderMap[i]), _p2cMap[0][orderMap[i].id]);

      }
      _c2pMap = V3NetVec(_ntk->getNetSize(), V3NetUD); _c2pMap[0] = V3NetId::makeNetId(0);
      for (uint32_t cycle = 0; cycle < _cycle; ++cycle) {
         V3NetVec& p2cMap = _p2cMap[cycle];
         /*for (uint32_t i = 0; i < p2cMap.size(); ++i) {
            cout << (p2cMap[i].cp ? "~": "") <<p2cMap[i].id << " ";
         }
         cout << endl;*/
         assert (_c2pMap[0] == p2cMap[0]);
         for (uint32_t i = 0; i < p2cMap.size(); ++i) {
            if (V3NetUD == p2cMap[i] || V3NetUD != _c2pMap[p2cMap[i].id]) continue;
            else _c2pMap[p2cMap[i].id] = V3NetId::makeNetId(i, p2cMap[i].cp);
         }
      }
   }
}

#endif

