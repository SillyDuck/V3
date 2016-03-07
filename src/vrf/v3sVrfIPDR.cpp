/****************************************************************************
  FileName     [ v3sVrfIPDR.cpp ]
  PackageName  [ v3/src/vrf ]
  Synopsis     [ Simplified Property Directed Reachability on V3 Ntk. ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#ifndef V3S_VRF_IPDR_C
#define V3S_VRF_IPDR_C

#include "v3Msg.h"
#include "v3Bucket.h"
#include "v3sVrfIPDR.h"
#include "v3NtkTemDecomp.h"

#include <iomanip>

//#define V3_IPDR_USE_PROPAGATE_BACKWARD
#define V3S_IPDR_USE_PROPAGATE_LOW_COST

/* -------------------------------------------------- *\
 * Class V3SIPDRFrame Implementations
\* -------------------------------------------------- */
// Constructor and Destructor
V3SIPDRFrame::V3SIPDRFrame() {
   _cubeList.clear();
}

V3SIPDRFrame::~V3SIPDRFrame() {
   for (V3SIPDRCubeList::iterator it = _cubeList.begin(); it != _cubeList.end(); ++it) delete *it;
   _cubeList.clear();
}

// Retrieval Functions
const bool
V3SIPDRFrame::pushCube(V3SIPDRCube* const c) {
   const uint32_t cubeSize = _cubeList.size(); _cubeList.insert(c);
   return cubeSize != _cubeList.size();
}

// Cube Containment Functions
const bool
V3SIPDRFrame::subsumes(const V3SIPDRCube* const cube) const {
   // This function checks whether cube is subsumed by any cube in this frame.
   assert (cube); const V3NetVec& cubeState = cube->getState();
   const uint64_t cubeSignature = ~(cube->getSignature());
   V3SIPDRCubeList::const_reverse_iterator it = _cubeList.rbegin();
   for (; it != _cubeList.rend(); ++it) {
      const V3NetVec& state = (*it)->getState(); assert (state.size());
      // Early Return
      if (!cubeState.size() || cubeState[0].id > state[0].id) return false;
      if (cubeState.size() < state.size()) continue;
      if (cubeSignature & (*it)->getSignature()) continue;
      // General Check
      uint32_t j = 0, k = 0;
      while (j < cubeState.size() && k < state.size()) {
         assert (!j || (cubeState[j].id > cubeState[j - 1].id));
         assert (!k || (state[k].id > state[k - 1].id));
         if (cubeState[j].id > state[k].id) { assert (j >= k); if (j == k) return false; break; }
         else if (cubeState[j].id < state[k].id) ++j;
         else { if (cubeState[j].cp ^ state[k].cp) break; ++j; ++k; }
      }
      if (k == state.size()) return true;
   }
   return false;
}

void
V3SIPDRFrame::removeSubsumed(const V3SIPDRCube* const cube) {
   // This function checks whether there's any existing cube in this frame subsumed by cube.
   // If such cube is found, remove it from _cubeList
   assert (cube); const V3NetVec& cubeState = cube->getState();
   const uint64_t cubeSignature = cube->getSignature();
   V3SIPDRCubeList::iterator it = _cubeList.begin();
   while (it != _cubeList.end()) {
      const V3NetVec& state = (*it)->getState();
      // Early Return
      if (cubeState[0].id < state[0].id) return;
      if (state.size() < cubeState.size()) { ++it; continue; }
      if (cubeSignature & ~((*it)->getSignature())) { ++it; continue; }
      // General Check
      uint32_t j = 0, k = 0;
      while (j < cubeState.size() && k < state.size()) {
         assert (!j || (cubeState[j].id > cubeState[j - 1].id));
         assert (!k || (state[k].id > state[k - 1].id));
         if (cubeState[j].id < state[k].id) { assert (j <= k); if (j == k) return; break; }
         else if (cubeState[j].id > state[k].id) ++k;
         else { if (cubeState[j].cp ^ state[k].cp) break; ++j; ++k; }
      }
      if (j != cubeState.size()) ++it;
      else { delete *it; _cubeList.erase(it++); }
   }
}

void
V3SIPDRFrame::removeSubsumed(const V3SIPDRCube* const cube, const V3SIPDRCubeList::const_iterator& ix) {
   // This function checks whether there's any existing cube in this frame subsumed by cube.
   // If such cube is found, remove it from _cubeList
   assert (cube); const V3NetVec& cubeState = cube->getState();
   const uint64_t cubeSignature = cube->getSignature();
   V3SIPDRCubeList::iterator it = _cubeList.begin();
   while (it != ix) {
      const V3NetVec& state = (*it)->getState();
      // Early Return
      assert (cubeState[0].id >= state[0].id);
      if (state.size() < cubeState.size()) { ++it; continue; }
      if (cubeSignature & ~((*it)->getSignature())) { ++it; continue; }
      // General Check
      uint32_t j = 0, k = 0;
      while (j < cubeState.size() && k < state.size()) {
         assert (!j || (cubeState[j].id > cubeState[j - 1].id));
         assert (!k || (state[k].id > state[k - 1].id));
         if (cubeState[j].id < state[k].id) { assert (j < k); break; }
         else if (cubeState[j].id > state[k].id) ++k;
         else { if (cubeState[j].cp ^ state[k].cp) break; ++j; ++k; }
      }
      if (j != cubeState.size()) ++it;
      else { delete *it; _cubeList.erase(it++); }
   }
}

void
V3SIPDRFrame::removeSelfSubsumed() {
   // This function checks whether some cubes in this frame can be subsumed by the other cube
   // Remove all cubes that are subsumed by the cube in this frame from _cubeList
   V3SIPDRCubeList::const_iterator ix;
   uint32_t candidates = 1;
   while (candidates <= _cubeList.size()) {
      ix = _cubeList.begin(); for (uint32_t i = candidates; i < _cubeList.size(); ++i) ++ix;
      removeSubsumed(*ix, ix); ++candidates;
   }
}

/* -------------------------------------------------- *\
 * Class V3SVrfIPDR Implementations
\* -------------------------------------------------- */
// Constructor and Destructor
V3SVrfIPDR::V3SVrfIPDR(const V3NtkHandler* const handler) : V3VrfBase(handler) {
   // Private Data Members
   _pdrFrame.clear(); _pdrBad = 0; _pdrSize = 0;

   // Private Engines
   _pdrSvr.clear(); _pdrSim = 0; _pdrGen = 0;
   // Private Tables
   _pdrInitConst.clear(); _pdrInitValue.clear();
   // Extended Data Members
   _pdrPriority.clear();
   // Statistics
   if (profileON()) {
      _totalStat     = new V3Stat("TOTAL");
      _initSvrStat   = new V3Stat("SVR INIT",    _totalStat);
      _solveStat     = new V3Stat("SVR SOLVE",   _totalStat);
      _generalStat   = new V3Stat("GENERALIZE",  _totalStat);
      _propagateStat = new V3Stat("PROPAGATION", _totalStat);
      _ternaryStat   = new V3Stat("TERNARY SIM", _totalStat);
   }
}

V3SVrfIPDR::~V3SVrfIPDR() {
   // Private Data Members
   for (uint32_t i = 0; i < _pdrFrame.size(); ++i) delete _pdrFrame[i]; _pdrFrame.clear();
   if (_pdrBad) delete _pdrBad; _pdrBad = 0;

   // Private Engines
   for (uint32_t i = 0; i < _pdrSvr.size(); ++i) delete _pdrSvr[i];
   _pdrSvr.clear(); if (_pdrGen) delete _pdrGen; _pdrGen = 0;
   // Private Tables
   _pdrInitConst.clear(); _pdrInitValue.clear();
   // Extended Data Members
   _pdrPriority.clear();
   // Statistics
   if (profileON()) {
      if (_totalStat    ) delete _totalStat;
      if (_initSvrStat  ) delete _initSvrStat;
      if (_solveStat    ) delete _solveStat;
      if (_generalStat  ) delete _generalStat;
      if (_propagateStat) delete _propagateStat;
      if (_ternaryStat  ) delete _ternaryStat;
   }
}

// Verification Main Functions
/* ---------------------------------------------------------------------------------------------------- *\
isIncKeepLastReachability(): If the last result is unsat, put the inductive invariant into the last frame.
isIncContinueOnLastSolver(): Valid only if isIncKeepLastReachability() is true.
\* ---------------------------------------------------------------------------------------------------- */
void
V3SVrfIPDR::startVerify(const uint32_t& p) {

   // Clear Verification Results
   clearResult(p); if (profileON()) _totalStat->start();

   // Consistency Check
   consistencyCheck(); assert (!_constr.size());

   if (!reportUnsupportedInitialState()) return;                           
/*
   V3NtkTemDecomp* const pNtk = new V3NtkTemDecomp(_handler, boundDepth, true); assert (pNtk);

   //printNetlist(pNtk);

   V3NetId id; V3NetVec p2cMap, c2pMap; V3RepIdHash repIdHash; repIdHash.clear();
   simpNtk = duplicateNtk(pNtk, p2cMap, c2pMap); assert (simpNtk);

   printNetlist(simpNtk);
*/

   _pdrGen = new V3AlgAigGeneralize(_handler); assert (_pdrGen);
   _pdrSim = dynamic_cast<V3AlgAigSimulate*>(_pdrGen); assert (_pdrSim);

   V3NetVec simTargets(1, _vrfNtk->getOutput(p)); _pdrSim->reset(simTargets);

   // Initialize Pattern Input Size
   assert (p < _result.size()); assert (p < _vrfNtk->getOutputSize());
   const V3NetId& pId = _vrfNtk->getOutput(p); assert (V3NetUD != pId);
   _pdrSize = _vrfNtk->getInputSize() + _vrfNtk->getInoutSize();

   // Initialize Parameters
   const string flushSpace = string(100, ' ');
   uint32_t proved = V3NtkUD, fired = V3NtkUD;
   struct timeval inittime, curtime; gettimeofday(&inittime, NULL);

   // Initialize Signal Priority List
   if (_pdrPriority.size() != _vrfNtk->getLatchSize()) _pdrPriority.resize(_vrfNtk->getLatchSize());

   // Initialize Bad Cube
   _pdrBad = new V3SIPDRCube(0); assert (_pdrBad); _pdrBad->setState(V3NetVec(1, pId));

   // Initialize Frame 0
   _pdrFrame.push_back(new V3SIPDRFrame()); assert (_pdrFrame.size() == 1);

   // Initialize Solver 0

   initializeSolver(0);
   assert (_pdrSvr.size() == 1); assert (_pdrSvr.back());
   if (_vrfNtk->getLatchSize()) _pdrSvr.back()->assertInit();  // R0 = I0


   // Start PDR Based Verification
   V3SIPDRCube* badCube = 0;
   while (true) {
      // Check Time Bounds
      gettimeofday(&curtime, NULL);
      if (_maxTime < getTimeUsed(inittime, curtime)) break;

      // Find a Bad Cube as Initial Proof Obligation
      badCube = getInitialObligation();  // SAT(R ^ T ^ !p)
      if (!badCube) {
         if (!isIncKeepSilent() && intactON()) {
            if (!endLineON()) Msg(MSG_IFO) << "\r" + flushSpace + "\r";
            Msg(MSG_IFO) << setw(3) << left << getPDRDepth() << " :";
            const uint32_t j = (_pdrFrame.size() > 25) ? _pdrFrame.size() - 25 : 0; if (j) Msg(MSG_IFO) << " ...";
            for (uint32_t i = j; i < _pdrFrame.size(); ++i) 
               Msg(MSG_IFO) << " " << _pdrFrame[i]->getCubeList().size();
            Msg(MSG_IFO) << endl;  // Always Endline At the End of Each Frame
         }
         // Set p to the Last Frame
         _pdrSvr.back()->assertProperty(pId, true, 0);
         // Push New Frame
         _pdrFrame.push_back(new V3SIPDRFrame());

         initializeSolver(getPDRDepth()); addLastFrameInfoToSolvers();
         assert (_pdrSvr.back()); assert (_pdrSvr.size() == _pdrFrame.size());

         if (propagateCubes()) { proved = getPDRDepth(); break; }

      }
      else {
         badCube = recursiveBlockCube(badCube);
         if (badCube) { fired = getPDRDepth(); break; }
         // Interactively Show the Number of Bad Cubes in Frames
         if (!isIncKeepSilent() && intactON()) {
            if (!endLineON()) Msg(MSG_IFO) << "\r" + flushSpace + "\r";
            Msg(MSG_IFO) << setw(3) << left << getPDRDepth() << " :";
            const uint32_t j = (_pdrFrame.size() > 25) ? _pdrFrame.size() - 25 : 0; if (j) Msg(MSG_IFO) << " ...";
            for (uint32_t i = j; i < _pdrFrame.size(); ++i) 
               Msg(MSG_IFO) << " " << _pdrFrame[i]->getCubeList().size();
            if (endLineON()) Msg(MSG_IFO) << endl; else Msg(MSG_IFO) << flush;
         }
      }
   }

   // Report Verification Result
   if (!isIncKeepSilent() && reportON()) {
      if (intactON()) {
         if (endLineON()) Msg(MSG_IFO) << endl;
         else Msg(MSG_IFO) << "\r" << flushSpace << "\r";
      }
      if (V3NtkUD != proved) Msg(MSG_IFO) << "Inductive Invariant found at depth = " << ++proved;
      else if (V3NtkUD != fired) Msg(MSG_IFO) << "Counter-example found at depth = " << ++fired;
      else Msg(MSG_IFO) << "UNDECIDED at depth = " << _maxDepth;
      if (usageON()) {
         gettimeofday(&curtime, NULL);
         Msg(MSG_IFO) << "  (time = " << setprecision(5) << getTimeUsed(inittime, curtime) << "  sec)" << endl;
      }
      if (profileON()) {
         _totalStat->end();
         Msg(MSG_IFO) << *_initSvrStat << endl;
         Msg(MSG_IFO) << *_solveStat << endl;
         Msg(MSG_IFO) << *_generalStat << endl;
         Msg(MSG_IFO) << *_propagateStat << endl;
         Msg(MSG_IFO) << *_ternaryStat << endl;
         Msg(MSG_IFO) << *_totalStat << endl;
      }
   }


   // Record CounterExample Trace or Invariant
   if (V3NtkUD != fired) {  // Record Counter-Example
      // Compute PatternCount
      const V3SIPDRCube* traceCube = badCube; assert (traceCube); assert (existInitial(traceCube->getState()));
      uint32_t patternCount = 0; while (_pdrBad != traceCube) { traceCube = traceCube->getNextCube(); ++patternCount; }
      V3CexTrace* const cex = new V3CexTrace(patternCount); assert (cex);
      _result[p].setCexTrace(cex); assert (_result[p].isCex());
      // Set Pattern Value
      traceCube = badCube; assert (traceCube); assert (existInitial(traceCube->getState()));
      while (_pdrBad != traceCube) {
         if (_pdrSize) cex->pushData(traceCube->getInputData());
         traceCube = traceCube->getNextCube(); assert (traceCube);
      }
      // Set Initial State Value
      if (_pdrInitValue.size()) {
         V3BitVecX initValue(_pdrInitValue.size());
         for (uint32_t i = 0; i < badCube->getState().size(); ++i) {
            assert (initValue.size() > badCube->getState()[i].id);
            if (badCube->getState()[i].cp) initValue.set0(badCube->getState()[i].id);
            else initValue.set1(badCube->getState()[i].id);
         }
         for (uint32_t i = 0; i < _pdrInitValue.size(); ++i)
            if (_pdrInitConst[i]) { if (_pdrInitValue[i]) initValue.set0(i); else initValue.set1(i); }
         cex->setInit(initValue);
      }
      // Delete Cubes on the Trace
      const V3SIPDRCube* lastCube; traceCube = badCube;
      while (_pdrBad != traceCube) { lastCube = traceCube->getNextCube(); delete traceCube; traceCube = lastCube; }

   }
   else if (V3NtkUD != proved) {  // Record Inductive Invariant
      _result[p].setIndInv(_vrfNtk); assert (_result[p].isInv());
      // Put the Inductive Invariant to the Last Frame
      uint32_t f = 1; for (; f < getPDRDepth(); ++f) if (!_pdrFrame[f]->getCubeList().size()) break;
      assert (f < getPDRDepth());
      for (uint32_t i = 1 + f; i < getPDRDepth(); ++i) {
         const V3SIPDRCubeList& cubeList = _pdrFrame[i]->getCubeList(); V3SIPDRCubeList::const_iterator it;
         for (it = cubeList.begin(); it != cubeList.end(); ++it) _pdrFrame.back()->pushCube(*it);
         _pdrFrame[i]->clearCubeList(); delete _pdrFrame[i]; delete _pdrSvr[i];
      }
      // Remove Empty Frames
      _pdrFrame[f] = _pdrFrame.back(); while ((1 + f) != _pdrFrame.size()) _pdrFrame.pop_back();
      _pdrFrame.back()->removeSelfSubsumed(); delete _pdrSvr.back(); while ((1 + f) != _pdrSvr.size()) _pdrSvr.pop_back();
   }
}

// PDR Initialization Functions
void
V3SVrfIPDR::initializeSolver(const uint32_t& d, const bool& isReuse) {
   if (profileON()) _initSvrStat->start();
   // New Solver
   assert (d == _pdrSvr.size());
   _pdrSvr.push_back(d ? referenceSolver(_pdrSvr[0]) : allocSolver(getSolver(), _vrfNtk));
   assert (_pdrSvr[d]->totalSolves() == 0);

   // Set Initial State to Solver //can use a dirty function here
   for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i) _pdrSvr[d]->addBoundedVerifyData(_vrfNtk->getLatch(i), 0);

   // Set p to this Frame if it is NOT the Last Frame
   _pdrSvr[d]->addBoundedVerifyData(_pdrBad->getState()[0], 0);
   if (d != getPDRDepth()) _pdrSvr[d]->assertProperty(_pdrBad->getState()[0], true, 0);

   // Consistency Check
   assert (_pdrFrame.size() == _pdrSvr.size());
   _pdrSvr[d]->simplify();
   if (profileON()) _initSvrStat->end();
}

void
V3SVrfIPDR::addCubeToSolver(const uint32_t& frame, const V3NetVec& state, const uint32_t& d) {
   assert (frame < _pdrSvr.size()); assert (_pdrSvr[frame]);
   assert (state.size()); assert (d < 2);
   for (uint32_t i = 0; i < state.size(); ++i) {
      assert (state[i].id < _vrfNtk->getLatchSize());
      _pdrSvr[frame]->addBoundedVerifyData(_vrfNtk->getLatch(state[i].id), d);
   }
}
void
V3SVrfIPDR::addLastFrameInfoToSolvers() {
   assert (_pdrFrame.size() > 1);
   const V3SIPDRCubeList& cubeList = _pdrFrame.back()->getCubeList(); 
   //cout << "c size" << cubeList.size() << endl;
   // seems like an empty function when no restart
   if (!cubeList.size()) return;
   V3SvrDataVec formula; formula.clear(); size_t fId;
   for (V3SIPDRCubeList::const_reverse_iterator it = cubeList.rbegin(); it != cubeList.rend(); ++it) {
      const V3NetVec& state = (*it)->getState(); assert (state.size());
      for (uint32_t d = 1; d < _pdrFrame.size(); ++d) {
         formula.reserve(state.size()); addCubeToSolver(d, state, 0);
         for (uint32_t i = 0; i < state.size(); ++i) {
            fId = _pdrSvr[d]->getFormula(_vrfNtk->getLatch(state[i].id), 0);
            formula.push_back(state[i].cp ? fId : _pdrSvr[d]->getNegFormula(fId));
         }
         _pdrSvr[d]->assertImplyUnion(formula); formula.clear();
      }
   }
}

void
V3SVrfIPDR::recycleSolver(const uint32_t& d) {
}

// PDR Main Functions
V3SIPDRCube* const
V3SVrfIPDR::getInitialObligation() {  // If SAT(R ^ T ^ !p)
   const uint32_t d = getPDRDepth(); _pdrSvr[d]->assumeRelease(); assert (_pdrBad);
   const V3NetVec& state = _pdrBad->getState(); assert (1 == state.size());
   _pdrSvr[d]->assumeProperty(state[0], false, 0);
   if (profileON()) _solveStat->start();
   _pdrSvr[d]->simplify();
   const bool result = _pdrSvr[d]->assump_solve();
   if (profileON()) _solveStat->end();
   if (!result) return 0;
   V3SIPDRCube* const cube = extractModel(d, _pdrBad);
   assert (cube); return cube;
}

V3SIPDRCube* const
V3SVrfIPDR::recursiveBlockCube(V3SIPDRCube* const badCube) {
   // Create a Queue for Blocking Cubes
   V3BucketList<V3SIPDRCube*> badQueue(getPDRFrame());
   assert (badCube); badQueue.add(getPDRDepth(), badCube);
   // Block Cubes from the Queue
   V3SIPDRTimedCube baseCube, generalizedCube;
   while (badQueue.pop(baseCube.first, baseCube.second)) {
      assert (baseCube.first < getPDRFrame());
      if (!baseCube.first) {
         // Clear All Cubes in badQueue before Return
         V3Set<const V3SIPDRCube*>::Set traceCubes; traceCubes.clear();
         const V3SIPDRCube* traceCube = baseCube.second;
         while (true) {
            traceCubes.insert(traceCube); if (_pdrBad == traceCube) break;
            traceCube = traceCube->getNextCube();
         }
         while (badQueue.pop(generalizedCube.first, generalizedCube.second)) {
            if (traceCubes.end() == traceCubes.find(generalizedCube.second)) delete generalizedCube.second;
         }
         return baseCube.second;  // A Cube cannot be blocked by R0 --> Cex
      }
      if (!isBlocked(baseCube)) {
         assert (!existInitial(baseCube.second->getState()));
         // Check Reachability : SAT (R ^ ~cube ^ T ^ cube')
         if (checkReachability(baseCube.first, baseCube.second->getState())) {  // SAT, Not Blocked Yet
            if (profileON()) _ternaryStat->start();
            generalizedCube.second = extractModel(baseCube.first - 1, baseCube.second);
            if (profileON()) _ternaryStat->end();
            badQueue.add(baseCube.first - 1, generalizedCube.second);  // This Cube should be blocked in previous frame
            badQueue.add(baseCube.first, baseCube.second);  // This Cube has not yet been blocked (postpone to future)
         }
         else {  // UNSAT, Blocked
            if (profileON()) _generalStat->start();
            generalizedCube.first = baseCube.first;
            generalizedCube.second = new V3SIPDRCube(*(baseCube.second));
            generalization(generalizedCube);  // Generalization
            if (profileON()) _generalStat->end();
            addBlockedCube(generalizedCube);  // Record this Cube that is bad and to be blocked
            if ((baseCube.first < getPDRDepth()) && (generalizedCube.first < getPDRFrame()))
               badQueue.add(baseCube.first + 1, baseCube.second);
         }
      }
   }
   return 0;
}


const bool
V3SVrfIPDR::propagateCubes() {
   if (profileON()) _propagateStat->start();
   // Check Each Frame if some Cubes can be Further Propagated
   for (uint32_t i = 1; i < getPDRDepth(); ++i) {
      const V3SIPDRCubeList& cubeList = _pdrFrame[i]->getCubeList();
      V3SIPDRCubeList::const_iterator it = cubeList.begin();
      while (it != cubeList.end()) {
         // Check if this cube can be pushed forward (closer to All Frame)
         if (!checkReachability(i + 1, (*it)->getState(), false)) {
            V3SIPDRTimedCube cube = make_pair(i + 1, new V3SIPDRCube(*(*it)));
            // Remove cubes in this frame that can be subsumed by the cube
            _pdrFrame[i]->removeSubsumed(cube.second, ++it);
            // Remove Cubes in the Next Frame that can be Subsumed by the cube
            removeFromProof(cube); _pdrFrame[i + 1]->removeSubsumed(cube.second);
            // Block this cube again at higher frames
            addBlockedCube(cube);
         }
         else {
            // Remove cubes in this frame that can be subsumed by the cube
            _pdrFrame[i]->removeSubsumed(*it, it);
            ++it;
         }
      }
      // Check if Any Remaining Cubes in this Frame can be Subsumed
      _pdrFrame[i]->removeSelfSubsumed();
      // Check Inductive Invariant
      if (!_pdrFrame[i]->getCubeList().size()) {
         if (profileON()) _propagateStat->end();
         return true;
      }
   }
   // Check if Any Remaining Cubes in the Latest Frame can be Subsumed
   _pdrFrame[getPDRDepth()]->removeSelfSubsumed();
   if (profileON()) _propagateStat->end();
   return false;
}


// PDR Auxiliary Functions
const bool
V3SVrfIPDR::checkReachability(const uint32_t& frame, const V3NetVec& cubeState, const bool& extend) {
   assert (frame > 0); assert (frame < getPDRFrame());
   const uint32_t& d = frame - 1;
   _pdrSvr[d]->assumeRelease();
   // Assume cube'
   addCubeToSolver(d, cubeState, 1);
   for (uint32_t i = 0; i < cubeState.size(); ++i)
      _pdrSvr[d]->assumeProperty(_pdrSvr[d]->getFormula(_vrfNtk->getLatch(cubeState[i].id), 1), cubeState[i].cp);
   if (extend) {
      // Assume ~cube
      addCubeToSolver(d, cubeState, 0);
      V3SvrDataVec blockCube; blockCube.clear(); blockCube.reserve(cubeState.size()); size_t fId;
      for (uint32_t i = 0; i < cubeState.size(); ++i) {
         fId = _pdrSvr[d]->getFormula(_vrfNtk->getLatch(cubeState[i].id), 0);
         blockCube.push_back(cubeState[i].cp ? fId : _pdrSvr[d]->getNegFormula(fId));
      }
      _pdrSvrData = _pdrSvr[d]->setImplyUnion(blockCube);
      assert (_pdrSvrData); _pdrSvr[d]->assumeProperty(_pdrSvrData);
      // Check Reachability by SAT Calling
      if (profileON()) _solveStat->start();
      _pdrSvr[d]->simplify();
      const bool result = _pdrSvr[d]->assump_solve();
      if (profileON()) _solveStat->end();
      _pdrSvr[d]->assertProperty(_pdrSvr[d]->getNegFormula(_pdrSvrData));  // Invalidate ~cube in future solving
      return result;
   }
   else {
      if (profileON()) _solveStat->start();
      _pdrSvr[d]->simplify();
      const bool result = _pdrSvr[d]->assump_solve();
      if (profileON()) _solveStat->end();
      return result;
   }
}

const bool
V3SVrfIPDR::isBlocked(const V3SIPDRTimedCube& timedCube) {
   // Check if cube has already been blocked by R (at specified frame)
   // Perform Subsumption Check : cube implies some C in R (at specified frame)
   for (uint32_t i = timedCube.first; i < _pdrFrame.size(); ++i) 
      if (_pdrFrame[i]->subsumes(timedCube.second)) return true;
   return false;
}

const bool
V3SVrfIPDR::existInitial(const V3NetVec& state) {
   for (uint32_t i = 0; i < state.size(); ++i) {
      assert (state[i].id < _pdrInitConst.size());
      assert (state[i].id < _pdrInitValue.size());
      if (_pdrInitConst[state[i].id] && (_pdrInitValue[state[i].id] ^ state[i].cp)) return false;
   }
   return true;
}

V3SIPDRCube* const
V3SVrfIPDR::extractModel(const uint32_t& d, const V3SIPDRCube* const nextCube) {
   // This function can ONLY be called after SAT of (R ^ T ^ nextCube') and generalize curCube from R
   V3SIPDRCube* const cube = new V3SIPDRCube(nextCube);  // Create Cube
   generalizeSimulation(d, cube, nextCube);  // Apply Simulation for the witness
   // Record Input to Proof Obligation for Trace Logging
   if (_pdrSize) recordCubeInputForTraceLog(cube); return cube;
}

V3SIPDRCube* const
V3SVrfIPDR::forwardModel(const V3SIPDRCube* const curCube) {
   assert (curCube); if (!curCube->getNextCube()) return 0;
   // Set Pattern Values for Simulator
   if (_pdrSize) {
      const V3BitVecX& inputData = curCube->getInputData();
      assert (inputData.size() == _pdrSize); uint32_t j = 0;
      for (uint32_t i = 0; i < _vrfNtk->getInputSize(); ++i, ++j)
         _pdrSim->setSource(_vrfNtk->getInput(i), inputData.bv_slice(j, j));
      for (uint32_t i = 0; i < _vrfNtk->getInoutSize(); ++i, ++j)
         _pdrSim->setSource(_vrfNtk->getInout(i), inputData.bv_slice(j, j));
      assert (j == inputData.size());
   }
   // Set State Variable Values for Simulator
   const V3NetVec& state = curCube->getState(); _pdrSim->setSourceFree(V3_FF, false);
   V3BitVecX value0(1), value1(1); value0.set0(0); value1.set1(0);
   for (uint32_t i = 0; i < state.size(); ++i)
      _pdrSim->setSource(_vrfNtk->getLatch(state[i].id), (state[i].cp ? value0 : value1));
   // Simulate for One Cycle
   _pdrSim->simulate(); _pdrSim->updateNextStateValue();
   // Return the Cube that it Reached
   V3NetVec nextState; nextState.clear(); nextState.reserve(_vrfNtk->getLatchSize());
   for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i) {
      switch (_pdrSim->getSimValue(_vrfNtk->getLatch(i))[0]) {
         case '0' : nextState.push_back(V3NetId::makeNetId(i, 1)); break;
         case '1' : nextState.push_back(V3NetId::makeNetId(i, 0)); break;
      }
   }
   if (existInitial(nextState)) return 0;
   V3SIPDRCube* const cube = new V3SIPDRCube(0); assert (cube);
   cube->setState(nextState); return cube;
}


void
V3SVrfIPDR::generalization(V3SIPDRTimedCube& generalizedCube) {
   removeFromProof(generalizedCube);
   generalizeProof(generalizedCube);
   forwardProof(generalizedCube);
}

void
V3SVrfIPDR::addBlockedCube(const V3SIPDRTimedCube& cube) {
   assert (cube.first < _pdrSvr.size()); assert (cube.second->getState().size());
   // Push cube into corresponding frame that it should be blocked
   if (!_pdrFrame[cube.first]->pushCube(cube.second)) return;
   // Renders this cube to be blocked for frames lower than cube.first
   const V3NetVec& state = cube.second->getState();
   V3SvrDataVec formula; formula.clear(); size_t fId;
   for (uint32_t d = 1; d <= cube.first; ++d) {
      formula.reserve(state.size()); addCubeToSolver(d, state, 0);
      for (uint32_t i = 0; i < state.size(); ++i) {
         fId = _pdrSvr[d]->getFormula(_vrfNtk->getLatch(state[i].id), 0);
         formula.push_back(state[i].cp ? fId : _pdrSvr[d]->getNegFormula(fId));
         ++_pdrPriority[state[i].id];  // Increase Signal Priority
      }
      _pdrSvr[d]->assertImplyUnion(formula); formula.clear();
   }
}

void
V3SVrfIPDR::recordCubeInputForTraceLog(V3SIPDRCube* const& cube) {
   assert (cube); assert (_pdrSim);
   V3BitVecX value(_pdrSize), tempValue;
   uint32_t j = 0;
   for (uint32_t i = 0; i < _vrfNtk->getInputSize(); ++i, ++j) {
      tempValue = _pdrSim->getSimValue(_vrfNtk->getInput(i));
      if ('1' == tempValue[0]) value.set1(j);
      else if ('0' == tempValue[0]) value.set0(j);
   }
   for (uint32_t i = 0; i < _vrfNtk->getInoutSize(); ++i, ++j) {
      tempValue = _pdrSim->getSimValue(_vrfNtk->getInout(i));
      if ('1' == tempValue[0]) value.set1(j);
      else if ('0' == tempValue[0]) value.set0(j);
   }
   assert (j == value.size()); cube->setInputData(value);
}

// PDR Generalization Functions
void
V3SVrfIPDR::generalizeSimulation(const uint32_t& d, V3SIPDRCube* const cube, const V3SIPDRCube* const nextCube) {
   assert (d < _pdrSvr.size()); assert (cube); assert (nextCube);
   assert (nextCube == cube->getNextCube()); assert (_pdrSim);
   // Set Values for Simulator
   for (uint32_t i = 0; i < _vrfNtk->getInputSize(); ++i) {
      if (!_pdrSvr[d]->existVerifyData(_vrfNtk->getInput(i), 0)) _pdrSim->clearSource(_vrfNtk->getInput(i), true);
      else _pdrSim->setSource(_vrfNtk->getInput(i), _pdrSvr[d]->getDataValue(_vrfNtk->getInput(i), 0));
   }
   for (uint32_t i = 0; i < _vrfNtk->getInoutSize(); ++i) {
      if (!_pdrSvr[d]->existVerifyData(_vrfNtk->getInout(i), 0)) _pdrSim->clearSource(_vrfNtk->getInout(i), true);
      else _pdrSim->setSource(_vrfNtk->getInout(i), _pdrSvr[d]->getDataValue(_vrfNtk->getInout(i), 0));
   }
   for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i) {
      if (!_pdrSvr[d]->existVerifyData(_vrfNtk->getLatch(i), 0)) _pdrSim->clearSource(_vrfNtk->getLatch(i), true);
      else _pdrSim->setSource(_vrfNtk->getLatch(i), _pdrSvr[d]->getDataValue(_vrfNtk->getLatch(i), 0));
   }
   _pdrSim->simulate();

   // Perform SAT Generalization
   if (_pdrBad != nextCube) _pdrGen->setTargetNets(V3NetVec(), nextCube->getState());
   else {
      V3NetVec constrCube(1, nextCube->getState()[0]);
      assert (1 == nextCube->getState().size()); _pdrGen->setTargetNets(constrCube);
   }
   // Set Priority
   V3UI32Vec prioNets; prioNets.clear(); prioNets.reserve(_pdrPriority.size());
   for (uint32_t i = 0; i < _pdrPriority.size(); ++i) if (!_pdrPriority[i]) prioNets.push_back(i);
   for (uint32_t i = 0; i < _pdrPriority.size(); ++i) if ( _pdrPriority[i]) prioNets.push_back(i);
   _pdrGen->performSetXForNotCOIVars(); _pdrGen->performXPropForExtensibleVars(prioNets);
   cube->setState(_pdrGen->getGeneralizationResult());
}

const bool
V3SVrfIPDR::removeFromProof(V3SIPDRTimedCube& timedCube) {
   // This function can ONLY be called after UNSAT of (R ^ T ^ cube')
   // Generate UNSAT Source from Solver if Possible
   V3SvrDataVec coreProofVars; coreProofVars.clear(); assert (timedCube.first < _pdrSvr.size());
   assert (timedCube.first); _pdrSvr[timedCube.first - 1]->getDataConflict(coreProofVars);
   if (!coreProofVars.size()) return false;  // Solver does not Support Analyze Conflict
   V3SvrDataSet coreProofVarSet; coreProofVarSet.clear();
   for (uint32_t i = 0; i < coreProofVars.size(); ++i) coreProofVarSet.insert(coreProofVars[i]);
   const bool isSvrDataInvolved = coreProofVarSet.end() != coreProofVarSet.find(_pdrSvrData);
   // Get Proof Related State Variables in UNSAT core
   assert (!existInitial(timedCube.second->getState()));

   const V3NetVec& state = timedCube.second->getState();
   // Remove Variables to Form New State
   assert (!existInitial(state));
   bool conflictInitial = false;
   V3NetId conflictId = V3NetUD;
   uint32_t pos = 0;
   V3NetVec newState; newState.reserve(state.size());
   for (uint32_t i = 0; i < state.size(); ++i) {
      if (coreProofVarSet.end() != coreProofVarSet.find(
            _pdrSvr[timedCube.first - 1]->getFormula(_vrfNtk->getLatch(state[i].id), 1))) {
         newState.push_back(state[i]);
         if (!conflictInitial && (_pdrInitConst[state[i].id] && (_pdrInitValue[state[i].id] ^ state[i].cp))) {
            assert (!existInitial(newState)); conflictInitial = true;
         }
      }
      else if (!conflictInitial && V3NetUD == conflictId) {
         if (_pdrInitConst[state[i].id] && (_pdrInitValue[state[i].id] ^ state[i].cp)) {
            conflictId = state[i]; assert (V3NetUD != conflictId); pos = newState.size();
         }
      }
   }
   // Resolve Intersection with Initial State
   if (!conflictInitial && V3NetUD != conflictId) { newState.insert(newState.begin() + pos, conflictId); }
   else if( !conflictInitial ){ cerr << "GGGG in removing UNSATCore" << endl; assert(0);}
   if (newState.size() < state.size()) timedCube.second->setState(newState);

   assert (!existInitial(timedCube.second->getState()));
   assert (!checkReachability(timedCube.first, timedCube.second->getState())); return isSvrDataInvolved;
}

void
V3SVrfIPDR::generalizeProof(V3SIPDRTimedCube& timedCube) {
   // Apply SAT Solving to further generalize cube
   // Remove Variables from cube after Proof Success
   V3SIPDRCube* const& cube = timedCube.second; assert (!existInitial(cube->getState()));
   V3NetVec state(cube->getState()); V3NetId id;
   // Sort Priority of Signals
   V3Map<uint32_t, uint32_t, V3UI32LessOrEq<uint32_t> >::Map priorityMap; priorityMap.clear();
   V3Map<uint32_t, uint32_t>::Map::iterator it, is;
   for (uint32_t i = 0; i < state.size(); ++i) {
      assert (state[i].id < _pdrPriority.size()); priorityMap.insert(make_pair(_pdrPriority[state[i].id], i));
   }
   for (it = priorityMap.begin(); it != priorityMap.end(); ++it) {
      // Try Removing A State Variable on this Cube
      // Don't really care detail here, it's right
      id = state[it->second]; assert (state.size() >= (1 + it->second));
      if (state.size() != (1 + it->second)) state[it->second] = state.back();
      state.pop_back();
      if (existInitial(state) || checkReachability(timedCube.first, state)) {
         if (state.size() == it->second) state.push_back(id);
         else { state.push_back(state[it->second]); state[it->second] = id; }
         assert (state.size() >= (1 + it->second));
      }
      else {
         if (state.size() > (1 + it->second)) {
            id = state[it->second];
            for (uint32_t i = it->second; i < (state.size() - 1); ++i) state[i] = state[1 + i];
            state.back() = id;
         }
         for (is = it, ++is; is != priorityMap.end(); ++is) {
            assert (is->second != it->second); if (is->second > it->second) --(is->second);
         }
      }
      assert (!existInitial(state)); assert (!checkReachability(timedCube.first, state));
   }
   if (state.size() < cube->getState().size()) cube->setState(state);
   assert (!existInitial(timedCube.second->getState()));
   assert (!checkReachability(timedCube.first, timedCube.second->getState()));
}

void
V3SVrfIPDR::forwardProof(V3SIPDRTimedCube& timedCube) {
   // Try Pushing the cube to higher frames if possible
   assert (getPDRDepth());  // R0 can never call this function
   while (timedCube.first < getPDRDepth()) {
      if (!checkReachability(++timedCube.first, timedCube.second->getState())) removeFromProof(timedCube);
      else {
         --timedCube.first; break;
      }
      assert (!existInitial(timedCube.second->getState()));
      assert (!(checkReachability(timedCube.first, timedCube.second->getState())));
   }
}

const bool
V3SVrfIPDR::generalizeCex(V3SIPDRTimedCube& timedCube) {
   return 0;
}

// PDR Helper Functions
const bool
V3SVrfIPDR::reportUnsupportedInitialState() {
   _pdrInitConst.clear(); _pdrInitConst.reserve(_vrfNtk->getLatchSize());
   _pdrInitValue.clear(); _pdrInitValue.reserve(_vrfNtk->getLatchSize());
   // Currently Not Support Non-Constant Initial State
   V3NetId id; V3BitVecX value; bool ok = true;
   const V3AigNtk* const aigNtk = _vrfNtk; assert (aigNtk);
   for (uint32_t i = 0; i < aigNtk->getLatchSize(); ++i) {
      id = aigNtk->getInputNetId(aigNtk->getLatch(i), 1);
      if (AIG_FALSE == aigNtk->getGateType(id)) {
         _pdrInitConst.push_back(1);
         _pdrInitValue.push_back(!id.cp);
      }
      else if (aigNtk->getLatch(i) == id) {
         _pdrInitConst.push_back(0);
         _pdrInitValue.push_back(0);
      }
      else {
         Msg(MSG_WAR) << "DFF " << i << " : " << _handler->getNetName(aigNtk->getLatch(i))
                      << " has Non-Constant Initial Value !!" << endl; ok = false; }
   }
   assert (_pdrInitConst.size() == _pdrInitValue.size());
   return ok;
}

// PDR Debug Functions
void
V3SVrfIPDR::printState(const V3NetVec& state) const {
   for (uint32_t i = 0; i < state.size(); ++i)
      Msg(MSG_IFO) << (state[i].cp ? "~" : "") << state[i].id << " ";
}

#endif