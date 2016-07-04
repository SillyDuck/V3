/****************************************************************************
  FileName     [ v3sVrfIPDR.cpp ]
  PackageName  [ v3/src/vrf ]
  Synopsis     [ Simplified Property Directed Reachability on V3 Ntk. ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#define heavy_debug 0
#define frame_info 0

#ifndef V3S_VRF_IPDR_C
#define V3S_VRF_IPDR_C

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

//#define V3_IPDR_USE_PROPAGATE_BACKWARD
#define V3S_IPDR_USE_PROPAGATE_LOW_COST

bool V3NetIdCompare (const V3NetId& i, const V3NetId j) { return (i.id<j.id); }
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
   _sim_then_add_cube = false;
   //_tem_decomp = false;
   // Private Data Members
   _pdrFrame.clear(); _pdrBad = 0; _pdrSize = 0;

   // Private Engines
   _pdrSvr.clear(); _pdrSim = 0; _pdrGen = 0;
   // Private Tables
   _pdrInitValue.clear(); _temFrames.clear();
   // Extended Data Members
   _pdrPriority.clear();
   _decompDepth = 1;
   // Statistics
   setProfile(true);
   if (profileON()) {
      _totalStat     = new V3Stat("TOTAL");
      _initSvrStat   = new V3Stat("SVR INIT",    _totalStat);
      _solveStat     = new V3Stat("SVR SOLVE",   _totalStat);
      _BMCStat       = new V3Stat("BMC SOLVE",   _totalStat);
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
   _pdrInitValue.clear();
   // Extended Data Members
   _pdrPriority.clear();
   // Statistics
   if (profileON()) {
      if (_totalStat    ) delete _totalStat;
      if (_initSvrStat  ) delete _initSvrStat;
      if (_solveStat    ) delete _solveStat;
      if (_BMCStat    ) delete _BMCStat;
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
   // Initialize Parameters
   uint32_t proved = V3NtkUD, fired = V3NtkUD;
   struct timeval inittime, curtime; gettimeofday(&inittime, NULL);
   clearResult(p); if (profileON()) _totalStat->start(); assert (!_constr.size());
   const string flushSpace = string(100, ' ');
   uint32_t fired2 = V3NtkUD;
   uint32_t boundDepth = 0;

   // Start BMC Based Verification
   V3Ntk* simpNtk = 0; V3SvrBase* solver = 0;
   while (boundDepth <= _decompDepth) {
      // Check Time Bounds
      boundDepth += 1;

      // Expand Network and Set Initial States
      V3NtkExpand* const pNtk = new V3NtkExpand(_handler, boundDepth, true); assert (pNtk);

      V3NetId id; V3NetVec p2cMap, c2pMap; V3RepIdHash repIdHash; repIdHash.clear();
      simpNtk = duplicateNtk(pNtk, p2cMap, c2pMap); assert (simpNtk);

      // Create Outputs for the Unrolled Property Signals
      for (uint32_t i = boundDepth - 1, j = 0; j < 1; ++i, ++j) {
         id = pNtk->getNtk()->getOutput(p + (_vrfNtk->getOutputSize() * i));
         simpNtk->createOutput(V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp));
      }
      // Set CONST 0 to Proven Property Signals
      for (uint32_t i = 0, j = boundDepth - 1, k = p; i < j; ++i, k += _vrfNtk->getOutputSize()) {
         id = pNtk->getNtk()->getOutput(k); id = V3NetId::makeNetId(p2cMap[id.id].id, p2cMap[id.id].cp ^ id.cp);
         repIdHash.insert(make_pair(id.id, V3NetId::makeNetId(0, id.cp)));
      }
      if (repIdHash.size()) simpNtk->replaceFanin(repIdHash); delete pNtk; p2cMap.clear(); c2pMap.clear();

      // Initialize Solver
      solver = allocSolver(getSolver(), simpNtk); assert (solver);
      solver->addBoundedVerifyData(simpNtk->getOutput(0), 0); solver->simplify();
      solver->assumeRelease(); solver->assumeProperty(simpNtk->getOutput(0), false, 0);
      if (solver->assump_solve()) { fired2 = boundDepth; break; }
      solver->assertProperty(simpNtk->getOutput(0), true, 0);

      if (!endLineON()) Msg(MSG_IFO) << "\r" + flushSpace + "\r";
      Msg(MSG_IFO) << "Verification completed under depth = "  << boundDepth << endl;

      if (V3NtkUD != fired2) break; delete solver; delete simpNtk;

   }
   if (V3NtkUD != fired2){
      Msg(MSG_IFO) << "Counter-example found at depth = " << fired2;
      fired = fired2 -1;
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
            Msg(MSG_IFO) << *_BMCStat << endl;
            Msg(MSG_IFO) << *_generalStat << endl;
            Msg(MSG_IFO) << *_propagateStat << endl;
            Msg(MSG_IFO) << *_ternaryStat << endl;
            Msg(MSG_IFO) << *_totalStat << endl;
         }
      }
      return;
   }
   setEndline(true);
   _maxTime = 900;
   // Clear Verification Results
   

   bool verbose = false;

   uint32_t simDepth = 30;
   V3AlgAigSimulate* _temSim = new V3AlgAigSimulate(_handler);
   vector<V3BitVecX> history;
   int first = -2;
   // TODO turn off this
   if(true){
      _temFrames.push_back(new V3SIPDRFrame());
      for (unsigned i = 0; i < simDepth; ++i){
         _temFrames.push_back(new V3SIPDRFrame());
      }
      V3BitVecX value;
      value.resize(1);
      value.setX(0);
      assert(_vrfNtk->getInoutSize() == 0);
      for (uint32_t j = 0; j < simDepth; ++j) {
         V3BitVecX v_dff(_vrfNtk->getLatchSize());
         _temSim->updateNextStateValue();
         for (uint32_t i = 0; i < _vrfNtk->getInputSize(); ++i) {
            assert ( _vrfNtk->getNetWidth(_vrfNtk->getInput(i)) == 1);
            _temSim->setSource(_vrfNtk->getInput(i), value);
         }

         _temSim->simulate();
         _temSim->getStateBV(v_dff,verbose);
         for (uint32_t ii = 0; ii < history.size(); ++ii){
            if((history[ii] == v_dff) && (first == -2)) first = ii;
         }
         history.push_back(v_dff);
      }
      for (uint32_t j = 0; j < 31; ++j) {
         //cout << _temFrames[j]->getCubeList().size() << " ";
      }
      //cout << endl;
   }
   //_decompDepth = first + 2;
   //_decompDepth = 3;

   V3BitVecX transient_signals(_vrfNtk->getLatchSize());
   //if(first != -2 ){
   if(false){
      for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i) {
         if(history[first][i] == '0'){
            bool aaa = true;
            for (unsigned k = first; k < simDepth; ++k){
                aaa &= (history[k][i] == '0');
            }
            if(aaa)
               transient_signals.set0(i);
         }
         else if(history[first][i] == '1'){
            bool aaa = true;
            for (unsigned k = first; k < simDepth; ++k){
                aaa &= (history[k][i] == '1');
            }
            if(aaa)
               transient_signals.set1(i);
         }
      }
   }

   if(_tem_decomp == false) _decompDepth = 1;

   // set FFs to be constant
   cerr << "Original Circuit Latch Size : " << _vrfNtk->getLatchSize() << endl;
   V3NtkTemDecomp* const pNtk = new V3NtkTemDecomp(_handler, 1 , transient_signals , false); assert (pNtk);
   _handler->_ntk = pNtk->getNtk();
   V3NtkHandler::setStrash(true);
   V3NtkHandler::setReduce(true);
   V3NtkHandler::setRewrite(true);

   // Simplify them
   V3NtkSimplify* const pNtk2 = new V3NtkSimplify(_handler);
   //printNetlist(pNtk2->getNtk());
   //cout << pNtk2->getNtk()->getOutput(0).id << endl;
   _handler->_ntk = pNtk2->getNtk();
   _vrfNtk = pNtk2->getNtk();
   cerr << "Simplified Circuit Latch Size : " << _vrfNtk->getLatchSize() << endl;
   if (!reportUnsupportedInitialState()) return;

   /*V3NetVec outputNets;
   outputNets.push_back(_vrfNtk->getOutput(0));
   for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i)
      outputNets.push_back(_vrfNtk->getLatch(i));
   string gg = "before.ntk";
   V3PlotNtkByLevel(_handler, gg.c_str(), 20, outputNets, false);*/

   // prolong the circuit
   if(_decompDepth >=2){
      _finalNtk = new V3NtkTemDecomp(_handler, _decompDepth, transient_signals , true); assert (pNtk);
      //printNetlist(_finalNtk->getNtk());
      V3NtkHandler* newHandler = new V3NtkHandler(_handler,_finalNtk->getNtk());
      _vrfNtk = _finalNtk->getNtk();
      _pdrGen = new V3AlgAigGeneralize(newHandler); assert (_pdrGen);
      _pdrSim = dynamic_cast<V3AlgAigSimulate*>(_pdrGen); assert (_pdrSim);
   }else{
      _pdrGen = new V3AlgAigGeneralize(_handler); assert (_pdrGen);
      _pdrSim = dynamic_cast<V3AlgAigSimulate*>(_pdrGen); assert (_pdrSim);
   }
   //gg = "after.ntk";
   //V3PlotNtkByLevel(newHandler, gg.c_str(), 20, outputNets, false);
   cout << "decompDepth : " << _decompDepth << endl;

   

   V3NetVec simTargets(1, _vrfNtk->getOutput(p)); _pdrSim->reset(simTargets);
   // Initialize Pattern Input Size
   assert (p < _result.size()); assert (p < _vrfNtk->getOutputSize());
   const V3NetId& pId = _vrfNtk->getOutput(p); assert (V3NetUD != pId);
   _pdrSize = _vrfNtk->getInputSize() + _vrfNtk->getInoutSize();


   // Initialize Signal Priority List
   if (_pdrPriority.size() != _vrfNtk->getLatchSize()) _pdrPriority.resize(_vrfNtk->getLatchSize());

   // Initialize Bad Cube
   _pdrBad = new V3SIPDRCube(0); assert (_pdrBad); _pdrBad->setState(V3NetVec(1, pId));

   // Initialize Frame 0, Solver 0
   _pdrFrame.push_back(new V3SIPDRFrame()); assert (_pdrFrame.size() == 1);
   initializeSolver(0);

   assert (_pdrSvr.size() == 1); assert (_pdrSvr.back());
   if (_vrfNtk->getLatchSize()) _pdrSvr.back()->assertInit();  // R0 = I0

   cerr <<"Solver initialized, Now Simulating and adding TemFrames\n";
   uint32_t count1 = 0;
   uint32_t count2 = 0;
   if(_sim_then_add_cube) {
      for (uint32_t j = 0; j < simDepth; ++j) {
         for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i) {
            V3SIPDRCube* tmpCube = new V3SIPDRCube(0);
            V3NetVec tmpVec;
            if(history[j][i] == '0'){
               bool aaa = true;
               for (unsigned k = 0; k < j; ++k){
                   aaa &= (history[k][i] == '0');
               }
               tmpVec.push_back(V3NetId::makeNetId(i,0));
               if( aaa && !existInitial(tmpVec) ){ // the cube should be extract later
                  tmpCube->setState(tmpVec);

                  _temFrames[j+1]->pushCube( tmpCube );
               }
            }
            else if(history[j][i] == '1'){
               bool aaa = true;
               for (unsigned k = 0; k < j; ++k){
                   aaa &= (history[k][i] == '1');
               }
               tmpVec.push_back(V3NetId::makeNetId(i,1));
               if( aaa && !existInitial(tmpVec) ){
                  tmpCube->setState(tmpVec);
                  _temFrames[j+1]->pushCube( tmpCube );
               }
            }
         }
      }
   }
   cerr <<"Simulation done, Start Verification\n";


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
         initializeSolver(getPDRDepth()); addLastFrameInfoToSolvers();
         if(_sim_then_add_cube){
            uint32_t dd = getPDRDepth();
            if( dd <= simDepth ){
               const V3SIPDRCubeList& cubeList = _temFrames[dd]->getCubeList(); 
               //cout << "cubeList.size() : " << cubeList.size() << endl;
               if (cubeList.size()){
               //if (false){
                  V3SvrDataVec formula; formula.clear(); size_t fId;
                  for (V3SIPDRCubeList::const_reverse_iterator it = cubeList.rbegin(); it != cubeList.rend(); ++it) {
                     count2++;
                     if(!_pdrFrame[dd]->pushCube(*it)){ count1++; continue; }
                     const V3NetVec& state = (*it)->getState(); assert (state.size());
                     formula.reserve(state.size()); addCubeToSolver(dd, state, 0);
                     for (uint32_t i = 0; i < state.size(); ++i) {
                        fId = _pdrSvr[dd]->getFormula(_vrfNtk->getLatch(state[i].id), 0);
                        formula.push_back(state[i].cp ? fId : _pdrSvr[dd]->getNegFormula(fId));
                     }
                     _pdrSvr[dd]->assertImplyUnion(formula); formula.clear();
                  }
               }
            }
         }

         assert (_pdrSvr.back()); assert (_pdrSvr.size() == _pdrFrame.size());

         if (propagateCubes()) {

            for (unsigned i = 1; i < _pdrFrame.size(); ++i){
                const V3SIPDRCubeList& cubeList = _pdrFrame[i]->getCubeList();
                for (V3SIPDRCubeList::const_reverse_iterator it = cubeList.rbegin(); it != cubeList.rend(); ++it) {
                  /*const V3NetVec& state = (*it)->getState();
                  cout << "Frame " << i << " : ";
                  printState(state);
                  cout << endl;*/
                }
            }

            proved = getPDRDepth(); break;
         }

      }
      else {
         //cout << "Found Bad Cube\n";
         //printState(badCube->getState());cout << endl;
         badCube = recursiveBlockCube(badCube);
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

   //cout << count1 << "/" << count2;

   // Record CounterExample Trace or Invariant
   if (V3NtkUD != fired) {  // Record Counter-Example
      // Compute PatternCount
      const V3SIPDRCube* traceCube = badCube; assert (traceCube);
      /*cout << "cex : ";
      printState(traceCube->getState());
      assert (existInitial(traceCube->getState()));*/
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
      /*if (_pdrInitValue.size()) {
         V3BitVecX initValue(_pdrInitValue.size());
         for (uint32_t i = 0; i < badCube->getState().size(); ++i) {
            assert (initValue.size() > badCube->getState()[i].id);
            if (badCube->getState()[i].cp) initValue.set0(badCube->getState()[i].id);
            else initValue.set1(badCube->getState()[i].id);
         }
         for (uint32_t i = 0; i < _pdrInitValue.size(); ++i)
            if (_pdrInitConst[i]) { if (_pdrInitValue[i]) initValue.set0(i); else initValue.set1(i); }
         //cex->setInit(initValue);
      }*/
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
   //cerr << "initializeSolver depth: " << d << endl;
   if (profileON()) _initSvrStat->start();
   if(!d){
      _pdrSvr.push_back(allocSolver(getSolver(), _vrfNtk));

      for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i)
         _pdrSvr[0]->addBoundedVerifyData(_vrfNtk->getLatch(i), 0);

      _pdrSvr[0]->addBoundedVerifyData(_pdrBad->getState()[0], 0);
      if (0 != getPDRDepth()) _pdrSvr[0]->assertProperty(_pdrBad->getState()[0], true, 0);
      assert (_pdrFrame.size() == _pdrSvr.size());

      // pseudo TODO
      /*for(uint32_t k = 0; k< _decompDepth; k++){// Set SATVar
         _ntkData[out.id].push_back(newVar(width));
         const Var& var = _ntkData[out.id].back();
         // Build FF Initial State
         const V3NetId in1 = _ntk->getInputNetId(out, 1); assert (validNetId(in1));
         if (AIG_FALSE == _ntk->getGateType(in1)) {
            //cerr << "AIG_FALSE" << endl;
            _init.push_back(mkLit(var, !isV3NetInverted(in1)));
         }
         else if (out.id != in1.id) {  // Build Initial Circuit
            //cerr << "not AIG_FALSE, " << in1.id << endl;
            const Var var1 = getVerifyData(in1, 0); assert (var1);
            const Var initVar = newVar(1);
            xor_2(_Solver, mkLit(initVar, true), mkLit(var), mkLit(var1, isV3NetInverted(in1)));
            _init.push_back(mkLit(initVar));
         }
      }*/

      _pdrSvr[0]->simplify();
   }
   else{
      assert (d == _pdrSvr.size());
      _pdrSvr.push_back(d ? referenceSolver(_pdrSvr[0]) : allocSolver(getSolver(), _vrfNtk));
      assert (_pdrSvr[d]->totalSolves() == 0);

      if(false){
         for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i)
            _pdrSvr[d]->addBoundedVerifyData(_vrfNtk->getLatch(i), 0);
         _pdrSvr[d]->addBoundedVerifyData(_pdrBad->getState()[0], 0);
      }else{
         for (uint32_t i = 0; i < _vrfNtk->getLatchSize(); ++i)
            _pdrSvr[d]->addBoundedVerifyDataTem(_vrfNtk->getLatch(i), 0);
         _pdrSvr[d]->addBoundedVerifyDataTem(_pdrBad->getState()[0], 0);
      }

      if (d != getPDRDepth()) _pdrSvr[d]->assertProperty(_pdrBad->getState()[0], true, 0);

      // Consistency Check
      assert (_pdrFrame.size() == _pdrSvr.size());
      _pdrSvr[d]->simplify();
   }
   if (profileON()) _initSvrStat->end();
   _pdrSvr[d]->printVerbose();
}

void
V3SVrfIPDR::addCubeToSolver(const uint32_t& frame, const V3NetVec& state, const uint32_t& d) {
   assert (frame < _pdrSvr.size()); assert (_pdrSvr[frame]);
   assert (state.size()); assert (d < 2);
   for (uint32_t i = 0; i < state.size(); ++i) {
      assert (state[i].id < _vrfNtk->getLatchSize());
      if(frame == 0)
         _pdrSvr[0]->addBoundedVerifyData(_vrfNtk->getLatch(state[i].id), d);
      else
         _pdrSvr[frame]->addBoundedVerifyDataTem(_vrfNtk->getLatch(state[i].id), d);
   }
}
void
V3SVrfIPDR::addLastFrameInfoToSolvers() {
   assert (_pdrFrame.size() > 1);
   const V3SIPDRCubeList& cubeList = _pdrFrame.back()->getCubeList(); 
   //cout << "c size" << cubeList.size() << endl;
   // seems like an empty function when no restart
   if (!cubeList.size()) return;
   cerr << " _pdrFrame.back() is not empty!!!\n ";
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
   if(heavy_debug) cerr << "\n\n\ngetInitialObligation depth : " << d << endl;
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
   if(heavy_debug) cerr << "\n\n\nrecursiveBlockCube\n";
   // Create a Queue for Blocking Cubes
   V3BucketList<V3SIPDRCube*> badQueue(getPDRFrame());
   assert (badCube); badQueue.add(getPDRDepth(), badCube);
   // Block Cubes from the Queue
   V3SIPDRTimedCube baseCube, generalizedCube;
   while (badQueue.pop(baseCube.first, baseCube.second)) {
      if(heavy_debug){
         cerr << "Poped: baseCube frame: " << baseCube.first <<  ", cube: ";
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
         assert (!existInitial(baseCube.second->getState()));
         // Check Reachability : SAT (R ^ ~cube ^ T ^ cube')
         if (checkReachability(baseCube.first, baseCube.second->getState())) {  // SAT, Not Blocked Yet
            if(heavy_debug){
               cerr << "SAT, generalizing... Frame before gen:" << baseCube.first << " Cube before gen:";
               printState(baseCube.second->getState());
            }
            if (profileON()) _ternaryStat->start();
            generalizedCube.second = extractModel(baseCube.first - 1, baseCube.second);
            if (profileON()) _ternaryStat->end();
            if(heavy_debug){
               cerr << "SAT, pushing to queue, Frame after gen:" << generalizedCube.first << " Cube after gen:";
               printState(generalizedCube.second->getState());
            }
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
   if(heavy_debug) cerr << "\n\n\npropagateCubes\n";
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
V3SVrfIPDR::checkReachability(const uint32_t& frame, const V3NetVec& cubeState, const bool& extend, const bool& notImportant) {
   if(heavy_debug && !notImportant){
      cerr << "\ncheckReachability frame : " << frame << " cube : ";
      printState(cubeState);
      cerr << endl;
   }
   assert (frame > 0); assert (frame < getPDRFrame());
   const uint32_t& d = frame - 1;


   _pdrSvr[d]->assumeRelease();
   // Assume cube'
   addCubeToSolver(d, cubeState, 1);

   /*if(d == 0){
      //for(uint32_t k = 0; k < _decompDepth ; k++){
         //_pdrSvr[0]->assumeInit( _decompDepth - k);
      for (uint32_t i = 0; i < cubeState.size(); ++i)
         _pdrSvr[0]->assumeProperty(_pdrSvr[0]->getFormula(_vrfNtk->getLatch(cubeState[i].id), 1), cubeState[i].cp);
      if (profileON()) _solveStat->start();
      _pdrSvr[0]->simplify();
      const bool result = _pdrSvr[0]->assump_solve();
      if (profileON()) _solveStat->end();
      if(heavy_debug && !notImportant) cerr << "result: " << result << endl;
      return result;
   }else{*/
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
         if(heavy_debug && !notImportant) cerr << "result: " << result << endl;
         return result;
      }
      else {
         if (profileON()) _solveStat->start();
         _pdrSvr[d]->simplify();
         const bool result = _pdrSvr[d]->assump_solve();
         if (profileON()) _solveStat->end();
         if(heavy_debug && !notImportant) cerr << "result: " << result << endl;
         return result;
      }
   //}
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
V3SVrfIPDR::existInitial(const V3NetVec& cubeState) {
   // checkReachability frame : 0 cube : state
   //cerr << "\ncheckexistInitial , cube : "; printState(cubeState); 
   if(false){
      bool tmpBool = true;
      for (uint32_t i = 0; i < cubeState.size(); ++i) {
         assert (cubeState[i].id < _pdrInitValue.size());
         if ((_pdrInitValue[cubeState[i].id] ^ cubeState[i].cp)) tmpBool = false;
      }
      if(tmpBool == true) return true;
      // if SAT, return true

      if(_decompDepth < 2)
         return false;

      _pdrSvr[0]->assumeRelease();
      for (uint32_t i = 0; i < cubeState.size(); ++i)
         _pdrSvr[0]->assumeProperty(_pdrSvr[0]->getFormula(_vrfNtk->getLatch(cubeState[i].id), 0), cubeState[i].cp);
      if (profileON()) _BMCStat->start();
      _pdrSvr[0]->simplify();
      const bool result = _pdrSvr[0]->assump_solve();
      if (profileON()) _BMCStat->end();
      if(result == true) return true;

      //if(_decompDepth < 3)
      return false;
   }else{
      if(_decompDepth == 1){
         for (uint32_t i = 0; i < cubeState.size(); ++i) {
            assert (cubeState[i].id < _pdrInitValue.size());
            if ((_pdrInitValue[cubeState[i].id] ^ cubeState[i].cp)) return false;
         }
         return true;
      }else{
         _pdrSvr[0]->assumeRelease();
         for (uint32_t i = 0; i < cubeState.size(); ++i)
            _pdrSvr[0]->assumeProperty(_pdrSvr[0]->getFormula(_vrfNtk->getLatch(cubeState[i].id), 0), cubeState[i].cp);
         if (profileON()) _BMCStat->start();
         _pdrSvr[0]->simplify();
         const bool result = _pdrSvr[0]->assump_solve();
         if (profileON()) _BMCStat->end();
         if(result == true) return true;
         return false;
      }
   }

   /*for (unsigned j = 1; j < _decompDepth - 1; ++j){
      _pdrSvr[0]->assumeRelease();
      V3NetId tmpId;
      for (uint32_t i = 0; i < cubeState.size(); ++i){
         tmpId = _finalNtk->_latchMap[j][cubeState[i].id];
         if(heavy_debug) cerr << "existInitial , getting " << _vrfNtk->getLatch(cubeState[i].id).id << " whose tmpId is " << tmpId.id  << ":" << tmpId.cp << endl;
         _pdrSvr[0]->assumeProperty2( tmpId , 0 , cubeState[i].cp  );
      }
      if (profileON()) _BMCStat->start();
      _pdrSvr[0]->simplify();
      const bool result = _pdrSvr[0]->assump_solve();
      if (profileON()) _BMCStat->end();
      //cerr << "GGGGGGG\n";
      //if(result == true) return true;
   }*/

   //cerr << "result == false\n";
   return false;
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
   if(heavy_debug){
      cerr << "UNSAT, generalizing... Frame before gen:" << generalizedCube.first << " Cube before gen:";
      printState(generalizedCube.second->getState());
   }

   removeFromProof(generalizedCube);
   generalizeProof(generalizedCube);
   forwardProof(generalizedCube);

   if(heavy_debug){
      cerr << "After generalize... Frame after gen:" << generalizedCube.first << " Cube after gen:";
      printState(generalizedCube.second->getState());
   }
}

void
V3SVrfIPDR::addBlockedCube(const V3SIPDRTimedCube& cube) {
   assert (cube.first < _pdrSvr.size()); assert (cube.second->getState().size());
   // Push cube into corresponding frame that it should be blocked
   if (!_pdrFrame[cube.first]->pushCube(cube.second)) return;
   if(heavy_debug){
      cerr << "Block Cube in Solver, Frame:" << cube.first << " Cube:";
      printState(cube.second->getState());
   }
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
   //_pdrGen->_tem = true;
   _pdrGen->performSetXForNotCOIVars(true); _pdrGen->performXPropForExtensibleVars(prioNets);
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
   //bool conflictInitial = false; V3NetId conflictId = V3NetUD; uint32_t pos = 0;
   V3NetVec newState; newState.reserve(state.size());
   V3NetVec notCoreState; notCoreState.reserve(state.size());
   for (uint32_t i = 0; i < state.size(); ++i) {
      if (coreProofVarSet.end() != coreProofVarSet.find(
            _pdrSvr[timedCube.first - 1]->getFormula(_vrfNtk->getLatch(state[i].id), 1)
         )) {
         newState.push_back(state[i]);
      }
      else{
         notCoreState.push_back(state[i]);
      }
   }

   for(uint32_t i = 0; i < notCoreState.size(); ++i){
      if(existInitial(newState)){
         newState.push_back(notCoreState[i]);
         sort(newState.begin(),newState.end(),V3NetIdCompare);
      }else{
         //cerr << "pushed ... : " << i << endl;
         break;
      }
   }
   if (newState.size() < state.size()) timedCube.second->setState(newState);
   checkCubeSorted(newState);
   assert (!existInitial(timedCube.second->getState()));
   assert (!checkReachability(timedCube.first, timedCube.second->getState(),true,true));

   // this return value isn't important...
   return isSvrDataInvolved;
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
      assert (!existInitial(state)); assert (!checkReachability(timedCube.first, state,true,true));
   }
   if (state.size() < cube->getState().size()) cube->setState(state);
   assert (!existInitial(timedCube.second->getState()));
   assert (!checkReachability(timedCube.first, timedCube.second->getState(),true,true));
}

void
V3SVrfIPDR::forwardProof(V3SIPDRTimedCube& timedCube) {
   // Try Pushing the cube to higher frames if possible
   assert (getPDRDepth());  // R0 can never call this function
   while (timedCube.first < getPDRDepth()) {
      if (!checkReachability(++timedCube.first, timedCube.second->getState(),true,true)) removeFromProof(timedCube);
      else {
         --timedCube.first; break;
      }
      assert (!existInitial(timedCube.second->getState()));
      assert (!(checkReachability(timedCube.first, timedCube.second->getState(),true,true)));
   }
}

const bool
V3SVrfIPDR::generalizeCex(V3SIPDRTimedCube& timedCube) {
   return 0;
}

// PDR Helper Functions
const bool
V3SVrfIPDR::reportUnsupportedInitialState() {

   V3NetId id; V3BitVecX value; bool ok = true;
   const V3AigNtk* const aigNtk = _vrfNtk; assert (aigNtk);
   //cerr << "reportUnsupportedInitialState, Latch Size: " << aigNtk->getLatchSize() << endl;
   for (uint32_t i = 0; i < aigNtk->getLatchSize(); ++i) {
      id = aigNtk->getInputNetId(aigNtk->getLatch(i), 1);
      //cerr << "reportUnsupportedInitialState" << id.id << endl;
      if (AIG_FALSE == aigNtk->getGateType(id)) {
         _pdrInitValue.push_back(!id.cp);
         //cerr << id.cp ;
      }
      else {
         Msg(MSG_WAR) << "DFF " << i << " : " << _handler->getNetName(aigNtk->getLatch(i))
                      << " has Non-Constant Initial Value !!" << endl; ok = false; }
      //cerr << endl;
   }

   return ok;
}

// PDR Debug Functions
void
V3SVrfIPDR::printState(const V3NetVec& state) const {
   cerr << "printState : ";
   for (uint32_t i = 0; i < state.size(); ++i)
      cerr << (state[i].cp ? "~" : "") << state[i].id << " ";
   cerr << endl;
}

#endif