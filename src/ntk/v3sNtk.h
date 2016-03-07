/****************************************************************************
  FileName     [ v3sNtk.h ]
  PackageName  [ v3/src/ntk ]
  Synopsis     [ rewrited version of V3 Network.
                 try to remove some rare-used function and merge
                 v3SvrMinisat v3AlgSim and v3NtkStrash v3WhateverType to here
               ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2015-2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef V3S_NTK_H
#define V3S_NTK_H

#include "Solver.h"
#include "v3Ntk.h"
#include "v3Misc.h"
#include "v3Type.h"
#include "v3SvrType.h"
#include "v3BitVec.h"
#include "v3NtkStrash.h"

// Type defined -- for a glimpse
/*

typedef V3Vec<V3NetId      >::Vec   V3NetVec;
typedef V3Vec<V3MiscType   >::Vec   V3TypeVec;
typedef V3Vec<V3NetType    >::Vec   V3InputVec;
typedef V3Vec<V3BitVecX*   >::Vec   V3BitVecXVec;

typedef V3Vec<V3InputVec   >::Vec   V3InputTable;
typedef V3Vec<V3NetVec     >::Vec   V3NetTable;

typedef V3HashMap<string,     V3BVXId>::Hash V3ConstHash;
typedef V3HashMap<uint64_t,   V3BusId>::Hash V3BusIdHash;
typedef V3HashMap<uint32_t,   V3NetId>::Hash V3RepIdHash;

typedef size_t                      V3SvrData;
typedef V3Vec<V3SvrData>::Vec       V3SvrDataVec;
typedef V3Set<V3SvrData>::Set       V3SvrDataSet;
typedef V3Vec<V3SvrDataVec>::Vec    V3SvrDataTable;


struct V3NetId {  // 4 Bytes
   uint32_t    cp :  1;
   uint32_t    id : 31;
   static V3NetId makeNetId(uint32_t i = V3NtkUD, uint32_t c = 0) { V3NetId j; j.cp = c; j.id = i; return j; }
   V3NetId operator ~ () const { return makeNetId(id, cp ^ 1); }
   const bool operator == (const V3NetId& i) const { return cp == i.cp && id == i.id; }
   const bool operator != (const V3NetId& i) const { return !(*this == i); }
};

struct V3MiscType {  // 4 Bytes
   uint32_t    type : 6;
   uint32_t    misc : 26;
   V3MiscType(uint32_t t = 0, uint32_t m = 0) { type = t; misc = m; }
};
*/


// V3SNtk : smaller aig network
class V3SNtk
{
   public : 

      V3SNtk();
      virtual ~V3SNtk();
      void cloneFromV3Ntk( V3Ntk* );
      void createGate(const V3GateType& type, const V3NetId& id) ;
      inline const V3GateType getGateType(const V3NetId& id) const { return (V3GateType)_typeMisc[id.id].type; }
      inline const uint32_t getNetSize() const { return _fanin0Data.size(); }
      //make sure add NULL fanin1 for FF and PO
      inline const uint32_t getInputSize() const { return _IOList[0].size(); }
      inline const uint32_t getOutputSize() const { return _IOList[1].size(); }
      inline const uint32_t getInoutSize() const { return _IOList[2].size(); }
      inline const uint32_t getLatchSize() const { return _FFList.size(); }
      inline const V3NetId& getInput(const uint32_t& i) const { assert (i < getInputSize()); return _IOList[0][i]; }
      inline const V3NetId& getOutput(const uint32_t& i) const { assert (i < getOutputSize()); return _IOList[1][i]; }
      inline const V3NetId& getInout(const uint32_t& i) const { assert (i < getInoutSize()); return _IOList[2][i]; }
      inline const V3NetId& getLatch(const uint32_t& i) const { assert (i < getLatchSize()); return _FFList[i]; }
      inline const V3NetId& getConst(const uint32_t& i) const { return _AIGFalse; }
      // Ntk Traversal Functions
      inline const V3NetId& getFanin(const V3NetId& id, const uint32_t& i) const {
         assert ( !i || i == 1); return i ?  _fanin1Data[id.id].id : _fanin0Data[id.id].id; }
      // Ntk Misc Data Functions
      inline void newMiscData() { assert (_globalMisc < V3MiscType(0, V3SNTkUD).misc); ++_globalMisc; }
      inline const bool isLatestMiscData(const V3NetId& id) const{ return _globalMisc == _typeMisc[id.id].misc; }
      inline void setLatestMiscData(const V3NetId& id) { _typeMisc[id.id].misc = _globalMisc; }

      // Ntk Cut Signal Functions
      //inline void setCutSignals(const V3NetVec& cut) { _cutSignals = cut; }
      //inline void clearCutSignals() { _cutSignals.clear(); }
      //inline const uint32_t getCutSize() const { return _cutSignals.size(); }
      //inline const V3NetId& getCutSignal(const uint32_t& i) const { return _cutSignals[i]; }

      /*void computeFanout(){ // implement this when needed
         _fanoutData.clear();
      };*/


      void strash(){
         _fanin0Data.clear();
         _fanin1Data.clear();
      };
      void fraig(){
         _fanin0Data.clear();
         _fanin1Data.clear();
      };

   protected :

      V3NetVec       _IOList[3];    // V3NetId of PI / PO / PIO
      V3NetVec       _FFList;       // V3NetId of FF

      V3NetId        _AIGFalse;
      V3TypeVec      _typeMisc;      // GateType with Misc Data
      V3InputVec       _fanin0Data;    // Fanin Vec for V3NetId
      V3InputVec       _fanin1Data;    // Fanin vec for V3NetId
      //V3NetTable     _fanoutData;
      uint32_t       _globalMisc;   // Global Misc Data for V3NetId in Ntk

      // should write in v3sPDR lol
      /*MSolver*       _Solver;    // Pointer to a Minisat solver
      Var            _curVar;    // Latest Fresh Variable
      vec<Lit>       _assump;    // Assumption List for assumption solve
      V3Vec<Lit>::Vec  _init;      // Initial state Var storage
      V3SvrMVarTable _ntkData;   // Mapping between V3NetId and Solver Data*/

};


#endif

