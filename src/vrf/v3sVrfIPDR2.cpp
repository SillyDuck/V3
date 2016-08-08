/****************************************************************************
  FileName     [ v3sVrfIPDR2.cpp ]
  PackageName  [ v3/src/vrf ]
  Synopsis     [ Simplified Property Directed Reachability on V3 Ntk. ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#define heavy_debug 0
#define frame_info 0

#ifndef V3S_VRF_IPDR2_C
#define V3S_VRF_IPDR2_C

#include "v3Msg.h"
#include "v3Bucket.h"
#include "v3NtkUtil.h"
#include "v3sVrfIPDR.h"
#include "v3NtkTemDecomp.h"
#include "v3NtkExpand.h"
#include "v3NtkSimplify.h"
#include <algorithm>
#include <iomanip>
#include "v3NtkWriter.h"



void
V3SVrfIPDR::startVerify2(const uint32_t& p) {
   // Initialize Parameters
   cerr << "Multi-Step PDR\n";
   uint32_t proved = V3NtkUD, fired = V3NtkUD;
   struct timeval inittime, curtime; gettimeofday(&inittime, NULL);
   clearResult(p); if (profileON()) _totalStat->start(); assert (!_constr.size());
   const string flushSpace = string(100, ' ');
   setEndline(true);
   _maxTime = 900;
   // Clear Verification Results

   if(_tem_decomp == false) _decompDepth = 1;
   if (!reportUnsupportedInitialState()) return;
   //printNetlist(_vrfNtk);
   V3NtkExpand2* const pNtk = new V3NtkExpand2(_handler, _decompDepth+1, true); assert (pNtk);
   _handler->_ntk = pNtk->getNtk();
   _vrfNtk = pNtk->getNtk();
   //_handler->_latchMap = V3NetTable(_cycle, V3NetVec(parentNets, V3NetUD));
   _handler->_latchMap = &(pNtk->_latchMap);
   if(_decompDepth >1) _handler->_decDep = _decompDepth;
   v3Handler.pushAndSetCurHandler(_handler);
   //printNetlist(pNtk->getNtk());

   /*for (unsigned i = 0; i < 3; ++i){
      for (unsigned j = 0; j < 6; ++j){
          cout << _handler->_latchMap->at(i)[j].id << ":" << _handler->_latchMap->at(i)[j].cp << endl;
      }
   }*/
   _pdrGen = new V3AlgAigGeneralize(_handler); assert (_pdrGen);
   _pdrSim = dynamic_cast<V3AlgAigSimulate*>(_pdrGen); assert (_pdrSim);
   V3NetVec simTargets(1, _vrfNtk->getOutput(p)); _pdrSim->reset2(simTargets);
   // Initialize Pattern Input Size
   assert (p < _result.size()); assert (p < _vrfNtk->getOutputSize());
   const V3NetId& pId = _vrfNtk->getOutput(p); assert (V3NetUD != pId);
   //cout << "outputId: " << pId.id << endl;
   //cout << "netSize: " << _vrfNtk->getNetSize() << endl;
   _pdrSize = _vrfNtk->getInputSize() + _vrfNtk->getInoutSize();


   // Initialize Signal Priority List
   if (_pdrPriority.size() != _vrfNtk->getLatchSize()) _pdrPriority.resize(_vrfNtk->getLatchSize());

   // Initialize Bad Cube
   _pdrBad = new V3SIPDRCube(0); assert (_pdrBad); _pdrBad->setState(V3NetVec(1, pId));

   // Initialize Frame 0, Solver 0
   _pdrFrame.push_back(new V3SIPDRFrame()); assert (_pdrFrame.size() == 1);
   initializeSolver2(0);

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
      if(heavy_debug){
         if(!badCube) cerr << "the Cube is NULL\n";
         if(badCube){
            cerr << "the Cube is NOT NULL\n";
            printState(badCube->getState());
         }
      }
      if (!badCube) {
         if (!isIncKeepSilent() && intactON() && frame_info ) {
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
         initializeSolver2(getPDRDepth());
         assert (_pdrSvr.back()); assert (_pdrSvr.size() == _pdrFrame.size());

         if (propagateCubes()) {
            proved = getPDRDepth(); break;
         }

      }
      else {
         badCube = recursiveBlockCube2(badCube);
         if (badCube) { fired = getPDRDepth(); break; }
         // Interactively Show the Number of Bad Cubes in Frames
         if (!isIncKeepSilent() && intactON() && frame_info) {
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
      uint32_t c_size = 0;
      for (uint32_t i = 0; i < _pdrFrame.size(); ++i)
         c_size += _pdrFrame[i]->getCubeList().size();
      cout << "CubeSize : " << c_size << endl;
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
         Msg(MSG_IFO) << *_BMCStat << endl;
         Msg(MSG_IFO) << *_generalStat << endl;
         Msg(MSG_IFO) << *_propagateStat << endl;
         Msg(MSG_IFO) << *_ternaryStat << endl;
         Msg(MSG_IFO) << *_totalStat << endl;
      }
   }

   // Record CounterExample Trace or Invariant
   if (V3NtkUD != fired) {  // Record Counter-Example
      // Compute PatternCount
      const V3SIPDRCube* traceCube = badCube; assert (traceCube);
      uint32_t patternCount = 0; while (_pdrBad != traceCube) { traceCube = traceCube->getNextCube(); ++patternCount; }
      V3CexTrace* const cex = new V3CexTrace(patternCount); assert (cex);
      _result[p].setCexTrace(cex); assert (_result[p].isCex());
      // Set Pattern Value
      traceCube = badCube; assert (traceCube); assert (existInitial2(traceCube->getState()));
      while (_pdrBad != traceCube) {
         if (_pdrSize) cex->pushData(traceCube->getInputData());
         traceCube = traceCube->getNextCube(); assert (traceCube);
      }
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

void
V3SVrfIPDR::initializeSolver2(const uint32_t& d, const bool& isReuse) {
   //cerr << "initializeSolver depth: " << d << endl;
   if (profileON()) _initSvrStat->start();
   if(!d){
      _pdrSvr.push_back(allocSolver(getSolver(), _vrfNtk));
   }
   else{
      assert (d == _pdrSvr.size());
      _pdrSvr.push_back(d ? referenceSolver(_pdrSvr[0]) : allocSolver(getSolver(), _vrfNtk));
   }
   for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i)
      _pdrSvr[d]->addBoundedVerifyData(_vrfNtk->getLatch(i), 0);
   _pdrSvr[d]->addBoundedVerifyData(_pdrBad->getState()[0], 0);

    if (d != getPDRDepth()) _pdrSvr[d]->assertProperty(_pdrBad->getState()[0], true, 0);

    // Consistency Check
    assert (_pdrFrame.size() == _pdrSvr.size());
    _pdrSvr[d]->simplify();
   if (profileON()) _initSvrStat->end();
   _pdrSvr[d]->printVerbose();
}

void
V3SVrfIPDR::addCubeToSolver2(const uint32_t& frame, const V3NetVec& state, const uint32_t& d) {
   assert (frame < _pdrSvr.size()); assert (_pdrSvr[frame]);
   assert (state.size()); assert (d < 2);
   for (uint32_t i = 0; i < state.size(); ++i) {
      assert (state[i].id < _vrfNtk->getLatchSize());
      _pdrSvr[frame]->addBoundedVerifyData(_handler->_latchMap->at(_decompDepth)[state[i].id], d);
   }
}


V3SIPDRCube* const
V3SVrfIPDR::recursiveBlockCube2(V3SIPDRCube* const badCube) {
   //unfolding_depth
   uint32_t d = _decompDepth;
   //cerr << "_decompDepth : " << d << endl;
   if(heavy_debug) cerr << "\n\n\nrecursiveBlockCube2\n";
   // Create a Queue for Blocking Cubes
   V3BucketList<V3SIPDRCube*> badQueue(getPDRFrame());
   assert (badCube); badQueue.add(getPDRDepth(), badCube);

   vector<uint32_t> v;
   for (unsigned i = 0; i <= getPDRDepth(); ++i){
     v.push_back(0);
   }
   // Block Cubes from the Queue
   V3SIPDRTimedCube baseCube, generalizedCube;
   while (badQueue.pop(baseCube.first, baseCube.second)) {
      v[baseCube.first]++;
      if(heavy_debug){
         cerr << "\nPoped: baseCube frame: " << baseCube.first <<  ", cube: ";
         printState( baseCube.second->getState() );
      }
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
         assert (!existInitial2(baseCube.second->getState()));
         // Check Reachability : SAT (R ^ ~cube ^ T ^ cube')

         if( baseCube.first >= d && v[baseCube.first] > 10){
         //if( false ){
            if(checkReachability2(baseCube.first - d + 1, baseCube.second->getState(),false)){
               if(heavy_debug){
                  cerr << "SAT, generalizing... Frame before gen:" << baseCube.first << " Cube before gen:";
                  printState(baseCube.second->getState());
               }
               if (profileON()) _ternaryStat->start();
               generalizedCube.second = extractModel2(baseCube.first - d, baseCube.second);
               if (profileON()) _ternaryStat->end();
               if(heavy_debug){
                  cerr << "SAT, pushing to queue, Frame after gen:" << baseCube.first-d << " Cube after gen:";
                  printState(generalizedCube.second->getState());
               }
               badQueue.add(baseCube.first - d, generalizedCube.second);  // This Cube should be blocked in previous frame
               badQueue.add(baseCube.first, baseCube.second);  // This Cube has not yet been blocked (postpone to future)
               //cout << "WOW" << endl;
               continue;
            }
         }


         if (checkReachability(baseCube.first, baseCube.second->getState())) {  // SAT, Not Blocked Yet
            if(heavy_debug){
               cerr << "SAT, generalizing... Frame before gen:" << baseCube.first << " Cube before gen:";
               printState(baseCube.second->getState());
            }
            if (profileON()) _ternaryStat->start();
            generalizedCube.second = extractModel(baseCube.first - 1, baseCube.second);
            if (profileON()) _ternaryStat->end();
            if(heavy_debug){
               cerr << "SAT, pushing to queue, Frame after gen:" << baseCube.first-1 << " Cube after gen:";
               printState(generalizedCube.second->getState());
            }
            badQueue.add(baseCube.first - 1, generalizedCube.second);  // This Cube should be blocked in previous frame
            badQueue.add(baseCube.first, baseCube.second);  // This Cube has not yet been blocked (postpone to future)
         }
         else {  // UNSAT, Blocked
            if (profileON()) _generalStat->start();
            generalizedCube.first = baseCube.first;
            generalizedCube.second = new V3SIPDRCube(*(baseCube.second));
            generalization2(generalizedCube);  // Generalization
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
V3SVrfIPDR::checkReachability2(const uint32_t& frame, const V3NetVec& cubeState, const bool& extend, const bool& notImportant) {
   if(heavy_debug && !notImportant){
      cerr << "\n!!!!!!checkReachability2 frame : " << frame << " cube : ";
      printState(cubeState);
      cerr << endl;
   }
   assert (frame > 0); assert (frame < getPDRFrame());
   const uint32_t& d = frame - 1;


   _pdrSvr[d]->assumeRelease();
   // Assume cube'
   addCubeToSolver2(d, cubeState, 0);

   /*V3SSvrMiniSat * gg= (V3SSvrMiniSat *)_pdrSvr[d];
   for (unsigned i = 0; i < 40; ++i){
      cerr << i << " : " << gg->getVerifyData( i ,0 ) << endl;
   }*/
    for (uint32_t i = 0; i < cubeState.size(); ++i)
     _pdrSvr[d]->assumeProperty(_pdrSvr[d]->getFormula(_handler->_latchMap->at(_decompDepth)[cubeState[i].id], 0), cubeState[i].cp);
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
     if(heavy_debug && !notImportant) cerr << "result: " << result << endl << endl;
     return result;
    }
    else {
     if (profileON()) _solveStat->start();
     /*V3SSvrMiniSat * GG = (V3SSvrMiniSat *)_pdrSvr[d];
     for (unsigned i = 0, s = GG->_assump.size(); i < s; ++i){
        cout << var(GG->_assump[i]) << ":" << sign(GG->_assump[i]) << endl;
     }*/
     _pdrSvr[d]->simplify();
     const bool result = _pdrSvr[d]->assump_solve();
     if (profileON()) _solveStat->end();
     if(heavy_debug && !notImportant) cerr << "result: " << result << endl << endl;
     return result;
    }

}

const bool
V3SVrfIPDR::existInitial2(const V3NetVec& state) {
   for (uint32_t i = 0; i < state.size(); ++i) {
      assert (state[i].id < _pdrInitValue.size());
      if (_pdrInitValue[state[i].id] ^ state[i].cp) return false;
   }
   return true;
}

V3SIPDRCube* const
V3SVrfIPDR::extractModel2(const uint32_t& d, const V3SIPDRCube* const nextCube) {
   // This function can ONLY be called after SAT of (R ^ T ^ nextCube') and generalize curCube from R
   V3SIPDRCube* const cube = new V3SIPDRCube(nextCube);  // Create Cube
   generalizeSimulation2(d, cube, nextCube);  // Apply Simulation for the witness
   // Record Input to Proof Obligation for Trace Logging
   if (_pdrSize) recordCubeInputForTraceLog2(cube); return cube;
}

void
V3SVrfIPDR::recordCubeInputForTraceLog2(V3SIPDRCube* const& cube) {
   //to find the right input trace... TBD....
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

void
V3SVrfIPDR::generalizeSimulation2(const uint32_t& d, V3SIPDRCube* const cube, const V3SIPDRCube* const nextCube) {
   assert (d < _pdrSvr.size()); assert (cube); assert (nextCube);
   assert (nextCube == cube->getNextCube()); assert (_pdrSim);
   // Set Values for Simulator
   for (uint32_t i = 0; i < _vrfNtk->getInputSize(); ++i) {
      if (!_pdrSvr[d]->existVerifyData(_vrfNtk->getInput(i), 0)) _pdrSim->clearSource(_vrfNtk->getInput(i), true);
      else _pdrSim->setSource(_vrfNtk->getInput(i), _pdrSvr[d]->getDataValue(_vrfNtk->getInput(i), 0));
   }
   for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i) {
      if (!_pdrSvr[d]->existVerifyData(_vrfNtk->getLatch(i), 0)) _pdrSim->clearSource(_vrfNtk->getLatch(i), true);
      else _pdrSim->setSource(_vrfNtk->getLatch(i), _pdrSvr[d]->getDataValue(_vrfNtk->getLatch(i), 0));
   }
   _pdrSim->simulate();

   // Perform SAT Generalization
   if (_pdrBad != nextCube) _pdrGen->setTargetNets2(V3NetVec(), nextCube->getState(), _decompDepth);
   else {
      V3NetVec constrCube(1, nextCube->getState()[0]);
      assert (1 == nextCube->getState().size()); _pdrGen->setTargetNets(constrCube);
   }
   // Set Priority
   V3UI32Vec prioNets; prioNets.clear(); prioNets.reserve(_pdrPriority.size());
   for (uint32_t i = 0; i < _pdrPriority.size(); ++i) if (!_pdrPriority[i]) prioNets.push_back(i);
   for (uint32_t i = 0; i < _pdrPriority.size(); ++i) if ( _pdrPriority[i]) prioNets.push_back(i);
   //_pdrGen->_tem = true;
   _pdrGen->performSetXForNotCOIVars(false); _pdrGen->performXPropForExtensibleVars(prioNets,false);
   cube->setState(_pdrGen->getGeneralizationResult());
}


void
V3SVrfIPDR::generalization2(V3SIPDRTimedCube& generalizedCube) {
   if(heavy_debug){
      cerr << "UNSAT, generalizing... Frame before gen:" << generalizedCube.first << " Cube before gen:";
      printState(generalizedCube.second->getState());
   }

   removeFromProof2(generalizedCube);
   generalizeProof(generalizedCube);
   forwardProof(generalizedCube);

   if(heavy_debug){
      cerr << "After generalize... Frame after gen:" << generalizedCube.first << " Cube after gen:";
      printState(generalizedCube.second->getState());
   }
}


const bool
V3SVrfIPDR::removeFromProof2(V3SIPDRTimedCube& timedCube) {
   // This function can ONLY be called after UNSAT of (R ^ T ^ cube')
   // Generate UNSAT Source from Solver if Possible
   V3SvrDataVec coreProofVars; coreProofVars.clear(); assert (timedCube.first < _pdrSvr.size());
   assert (timedCube.first); _pdrSvr[timedCube.first - 1]->getDataConflict(coreProofVars);
   if (!coreProofVars.size()) return false;  // Solver does not Support Analyze Conflict
   V3SvrDataSet coreProofVarSet; coreProofVarSet.clear();
   for (uint32_t i = 0; i < coreProofVars.size(); ++i) coreProofVarSet.insert(coreProofVars[i]);
   const bool isSvrDataInvolved = coreProofVarSet.end() != coreProofVarSet.find(_pdrSvrData); // not used var
   // Get Proof Related State Variables in UNSAT core
   assert (!existInitial2(timedCube.second->getState()));

   const V3NetVec& state = timedCube.second->getState();
   // Remove Variables to Form New State
   assert (!existInitial2(state));
   bool conflictInitial = false;
   V3NetId conflictId = V3NetUD;
   uint32_t pos = 0;
   V3NetVec newState; newState.reserve(state.size());
   for (uint32_t i = 0; i < state.size(); ++i) {
      if (coreProofVarSet.end() != coreProofVarSet.find(
            _pdrSvr[timedCube.first - 1]->getFormula(_vrfNtk->getLatch(state[i].id), 1))) {
         newState.push_back(state[i]); //OrzOrz
         if (!conflictInitial && (_pdrInitValue[state[i].id] ^ state[i].cp)) {
            assert (!existInitial2(newState)); conflictInitial = true;
         }
      }
      else if (!conflictInitial && V3NetUD == conflictId) {
         if (_pdrInitValue[state[i].id] ^ state[i].cp) {
            conflictId = state[i]; assert (V3NetUD != conflictId); pos = newState.size();
         }
      }
   }
   // Resolve Intersection with Initial State
   if (!conflictInitial && V3NetUD != conflictId) { newState.insert(newState.begin() + pos, conflictId); }
   else if( !conflictInitial ) cerr << "GGGG in removing UNSATCore" << endl;
   if (newState.size() < state.size()) timedCube.second->setState(newState);
   //checkCubeSorted(newState);
   assert (!existInitial2(timedCube.second->getState()));
   assert (!checkReachability(timedCube.first, timedCube.second->getState())); return isSvrDataInvolved;
}



#endif