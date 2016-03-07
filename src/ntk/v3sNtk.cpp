/****************************************************************************
  FileName     [ V3SNTk.cpp ]
  PackageName  [ v3/src/ntk ]
  Synopsis     [ V3 Network. ]
  Author       [ Cheng-Yin Wu ]
  Copyright    [ Copyright(c) 2012-2014 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef V3S_NTK_C
#define V3S_NTK_C

#include "v3sNtk.h"
#include "v3Msg.h"
#include "v3StrUtil.h"

/* -------------------------------------------------- *\
 * Class V3SNTk Implementations
\* -------------------------------------------------- */
// Constuctor and Destructor
V3SNtk::V3SNtk() {
    for (uint32_t i = 0; i < 3; ++i) _IOList[i].clear();
    _FFList.clear();
    const V3NetId id = V3NetId::makeNetId(0);
    createGate(AIG_FALSE, id); _AIGFalse = id;
    _typeMisc.clear();
    _fanin0Data.clear();
    _fanin1Data.clear();
    _globalMisc = 0;
}

V3SNtk::~V3SNtk() {
}
void
V3SNtk::createGate(const V3GateType& type, const V3NetId& id) {
    _typeMisc[id.id].type = type;
}
//V3InputTable   _inputData;    // Fanin Table for V3NetId   (V3NetId, V3BVXId, V3BusId)
/////
/*
      V3NetVec       _IOList[3];    // V3NetId of PI / PO / PIO
      V3NetVec       _FFList;       // V3NetId of FF

      V3NetId        _AIGFalse;
      V3TypeVec      _typeMisc;      // GateType with Misc Data
      V3NetVec       _fanin0Data;    // Fanin Vec for V3NetId
      V3NetVec       _fanin1Data;    // Fanin vec for V3NetId
      //V3NetTable     _fanoutData;
      uint32_t       _globalMisc;   // Global Misc Data for V3NetId in Ntk
*/
///////

void V3SNtk::cloneFromV3Ntk( V3Ntk* ntk ){
    assert (!dynamic_cast<const V3BvNtk*>(this));
    //only valid when ntk is aig

    for(uint i = 0; i < 3; i++){
        _IOList[i] = ntk->_IOList[i];
    }
    _FFList = ntk->_FFList;
    _AIGFalse = ntk->_ConstList[0];
    _typeMisc = ntk->_typeMisc;
    for(uint i = 0; i < ntk->_inputData.size(); i++){
        _fanin0Data.push_back( ntk->_inputData[i][0] );
        _fanin1Data.push_back( ntk->_inputData[i][1] );
    }
    _globalMisc = ntk->_globalMisc;
}


/*
void
V3Ntk::replaceFanin(const V3RepIdHash& repIdHash) {
   assert (repIdHash.size());
   uint32_t i, inSize; V3GateType type;
   V3RepIdHash::const_iterator it;
   for (V3NetId id = V3NetId::makeNetId(1); id.id < _inputData.size(); ++id.id) {
      type = getGateType(id);
      if (V3_MODULE == type) {
         V3NtkModule* const module = getModule(_inputData[id.id][0].value); assert (module);
         for (i = 0; i < module->getInputList().size(); ++i) {
            it = repIdHash.find(module->getInputList()[i].id); if (repIdHash.end() == it) continue;
            module->updateInput(i, module->getInputList()[i].cp ? ~(it->second) : it->second);
         }
      }
      else {
         inSize = (AIG_NODE == type || isV3PairType(type)) ? 2 : (BV_MUX == type) ? 3 :
                  (V3_FF == type || BV_SLICE == type || isV3ReducedType(type)) ? 1 : 0;
         for (i = 0; i < inSize; ++i) {
            it = repIdHash.find(_inputData[id.id][i].id.id); if (repIdHash.end() == it) continue;
            _inputData[id.id][i] = V3NetType(_inputData[id.id][i].id.cp ? ~(it->second) : it->second);
         }
      }
   }
}
*/



#endif

