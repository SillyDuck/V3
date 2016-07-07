/****************************************************************************
  FileName     [ v3sVrfIPDR.h ]
  PackageName  [ v3/src/vrf ]
  Synopsis     [ Simplified Property Directed Reachability on V3 Ntk. ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef V3S_VRF_IPDR_H
#define V3S_VRF_IPDR_H

#include "v3Usage.h"
#include "v3SvrType.h"
#include "v3VrfBase.h"
#include "v3AlgGeneralize.h"
#include "v3NtkTemDecomp.h"

// Forward Declaration
class V3SIPDRCube;
class V3SIPDRFrame;

// Defines for Incremental PDR Global Data
typedef pair<uint32_t, V3SIPDRCube*>          V3SIPDRTimedCube;
typedef V3Vec<V3SIPDRFrame*>::Vec             V3SIPDRFrameVec;
typedef V3Vec<V3SvrBase*>::Vec                V3SIPDRSvrList;

// class V3SIPDRCube : Cube Data Structue for Incremental PDR
// NOTE: State Variables are Intrinsically Sorted by their indices (small to large)
class V3SIPDRCube
{
   public : 
      V3SIPDRCube(const V3SIPDRCube* const c) : _nextCube(c) { _stateId.clear(); _signature = 0; }
      ~V3SIPDRCube() { _stateId.clear(); }
      // Cube Setting Functions
      inline void setState(const V3NetVec&);
      inline const V3NetVec& getState() const { return _stateId; }
      inline const uint64_t getSignature() const { return _signature; }
      // Trace Logging Functions
      inline void setInputData(const V3BitVecX& v) { _inputData = v; }
      inline const V3SIPDRCube* const getNextCube() const { return _nextCube; }
      inline const V3BitVecX& getInputData() const { return _inputData; }
   private : 
      // Private Cube Data
      V3NetVec                _stateId;      // State Variable Index (id) with Value (cp)
      // be careful it's index
      uint64_t                _signature;    // Subsumption Marker
      // Trace Logging Members
      V3BitVecX               _inputData;    // Primary Input / Inout Values
      const V3SIPDRCube* const _nextCube;    // Successor Proof Obligation
};

// Defines for Sorted Incremental PDR Cube List
struct V3SIPDRCubeCompare {
   const bool operator() (const V3SIPDRCube* const c1, const V3SIPDRCube* const c2) const {
      assert (c1); const V3NetVec& state1 = c1->getState();
      assert (c2); const V3NetVec& state2 = c2->getState();
      uint32_t i = 0;
      // Render the One with Larger Leading Id the Latter
      while (i < state1.size() && i < state2.size()) {
         assert (!i || (state1[i].id > state1[i-1].id));
         assert (!i || (state2[i].id > state2[i-1].id));
         if (state1[i].id == state2[i].id) {
            if (state1[i].cp == state2[i].cp) ++i;
            else return state1[i].cp < state2[i].cp;
         }
         else return state1[i].id < state2[i].id;
      }
      // Render the One with Less Variables the Latter
      return (state2.size() > i);  //return (state1.size() <= i);
   }
};

typedef V3Set<V3SIPDRCube*, V3SIPDRCubeCompare>::Set   V3SIPDRCubeList;

// class V3SIPDRFrame : Frame in Incremental PDR
// NOTE: Cubes are Intentionally Sorted for Fast Containment Check
class V3SIPDRFrame
{
   public : 
      // Constructor and Destructor
      V3SIPDRFrame();
      ~V3SIPDRFrame();
      // Retrieval Functions
      inline const V3SIPDRCubeList& getCubeList() const { return _cubeList; }
      const bool pushCube(V3SIPDRCube* const);
      inline void clearCubeList() { _cubeList.clear(); }
      // Cube Containment Functions
      const bool subsumes(const V3SIPDRCube* const) const;
      void removeSubsumed(const V3SIPDRCube* const);
      void removeSubsumed(const V3SIPDRCube* const, const V3SIPDRCubeList::const_iterator&);
      void removeSelfSubsumed();
   private : 
      // Private Data Members
      V3SIPDRCubeList _cubeList;     // List of Cubes Blocked in this Frame
};

// class V3VrfIPDR : Verification Handler for Incremental Property Directed Reachability
class V3SVrfIPDR : public V3VrfBase
{
   public : 
      // Constructor and Destructor
      V3SVrfIPDR(const V3NtkHandler* const);
      ~V3SVrfIPDR();

   //private : 
      // Private Attribute Setting Functions
      //inline const bool isForwardSATGen()   const { return _pdrAttr & 1ul; }
      //inline const bool isForwardUNSATGen() const { return _pdrAttr & 2ul; }
      // Private Verification Main Functions
      void startVerify(const uint32_t&);
      void startVerify2(const uint32_t&);
      // PDR Initialization Functions
      void initializeSolver(const uint32_t&, const bool& = false);
      void initializeSolver2(const uint32_t&, const bool& = false);
      void addCubeToSolver(const uint32_t&, const V3NetVec&, const uint32_t&);
      void addCubeToSolver2(const uint32_t&, const V3NetVec&, const uint32_t&);
      void addLastFrameInfoToSolvers();
      void recycleSolver(const uint32_t&);
      // PDR Main Functions
      V3SIPDRCube* const getInitialObligation();
      V3SIPDRCube* const recursiveBlockCube(V3SIPDRCube* const);
      V3SIPDRCube* const recursiveBlockCube2(V3SIPDRCube* const);
      const bool propagateCubes();
      // PDR Auxiliary Functions
      const bool checkReachability(const uint32_t&, const V3NetVec&, const bool& = true, const bool& = false);
      const bool checkReachability2(const uint32_t&, const V3NetVec&, const bool& = true, const bool& = false);
      const bool isBlocked(const V3SIPDRTimedCube&);
      const bool existInitial(const V3NetVec&);
      const bool existInitial2(const V3NetVec&);

      V3SIPDRCube* const extractModel(const uint32_t&, const V3SIPDRCube* const);
      V3SIPDRCube* const extractModel2(const uint32_t&, const V3SIPDRCube* const);
      V3SIPDRCube* const forwardModel(const V3SIPDRCube* const);

      void generalization(V3SIPDRTimedCube&);
      void generalization2(V3SIPDRTimedCube&);
      void addBlockedCube(const V3SIPDRTimedCube&);
      void recordCubeInputForTraceLog(V3SIPDRCube* const&);

      // PDR Generalization Functions
      void generalizeSimulation(const uint32_t&, V3SIPDRCube* const, const V3SIPDRCube* const);
      void generalizeSimulation2(const uint32_t&, V3SIPDRCube* const, const V3SIPDRCube* const);
      const bool removeFromProof(V3SIPDRTimedCube&);
      const bool removeFromProof2(V3SIPDRTimedCube&);
      void generalizeProof(V3SIPDRTimedCube&);
      void forwardProof(V3SIPDRTimedCube&);
      const bool generalizeCex(V3SIPDRTimedCube&);
      // PDR Helper Functions
      inline const uint32_t getPDRDepth() const { return _pdrFrame.size() - 1; }
      inline const uint32_t getPDRFrame() const { return _pdrFrame.size(); }
      const bool reportUnsupportedInitialState();
      // PDR Debug Functions
      void checkCubeSorted(const V3NetVec& state) const {
         for (unsigned i = 0, s = state.size()-1; i < s; ++i){
               if(state[i].id >= state[i+1].id) assert(0);
            }
      };
      void printState(const V3NetVec&) const;
      // Private Data Members
      V3SIPDRFrameVec    _pdrFrame;        // List of Frames (Ri) in Incremental PDR
      V3SIPDRCube*       _pdrBad;          // Cube for the Bad State (!p)
      uint32_t          _pdrSize;         // Input Size for the Instance
      //unsigned char     _pdrAttr;         // Specific Attributes for MPDR
      V3UI32Vec         _pdrActCount;     // List of Activation Variable Counts
      // Private Engines
      V3SIPDRSvrList     _pdrSvr;          // List of Incremental SAT Solvers
      V3AlgSimulate*    _pdrSim;          // Simulation Handler
      V3AlgGeneralize*  _pdrGen;          // Generalization Handler
      // Private Tables
      //V3BoolVec         _pdrInitConst;    // Initial State of a State Variable (whether it is a const)
      V3BoolVec         _pdrInitValue;    // Initial State of a State Variable (value of the const)
      // Extended Data Members
      V3SvrData         _pdrSvrData;      // Solver Data of the Latest Activation Variable
      V3UI32Vec         _pdrPriority;     // Priority List for State Variables
      // Statistics
      V3Stat*           _totalStat;       // Total Statistic (Should be Called Only Once)
      V3Stat*           _initSvrStat;     // CNF Computation
      V3Stat*           _solveStat;       // SAT Solving
      V3Stat*           _BMCStat;         // SAT Solving
      V3Stat*           _generalStat;     // UNSAT Generalization
      V3Stat*           _propagateStat;   // Propagation
      V3Stat*           _ternaryStat;     // SAT Generalization
      uint32_t          _decompDepth;      // Decomposition Depth
      // Data Members for Temporal Decompostition
      V3SIPDRFrameVec   _temFrames;
      V3NtkTemDecomp*   _finalNtk;
      bool             _sim_then_add_cube;
      bool             _tem_decomp;
};

// Inline Function Implementations of Cube Setting Functions
inline void V3SIPDRCube::setState(const V3NetVec& v) {
   _stateId = v; _signature = 0;
   for (uint32_t i = 0; i < _stateId.size(); ++i) _signature |= (1ul << (_stateId[i].id % 64)); }

#endif
