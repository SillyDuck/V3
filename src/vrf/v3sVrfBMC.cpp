/****************************************************************************
  FileName     [ v3sVrfBMC.cpp ]
  PackageName  [ v3/src/vrf ]
  Synopsis     [ Simplified Bounded Model Checking on V3 Ntk. ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2015-2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#ifndef V3S_VRF_BMC_C
#define V3S_VRF_BMC_C

#include "v3sVrfBMC.h"
#include "v3NtkUtil.h"
#include "v3NtkExpand.h"

/* -------------------------------------------------- *\
 * Class V3VrfBMC Implementations
\* -------------------------------------------------- */
// Constructor and Destructor
V3SVrfBMC::V3SVrfBMC(const V3NtkHandler* const handler) : V3VrfBase(handler) {
   // Private Data Members
   _preDepth = 0; _incDepth = 1;
}

V3SVrfBMC::~V3SVrfBMC() {
}

// Private Verification Main Functions
void
V3SVrfBMC::startVerify(const uint32_t& p) {

   // Clear Verification Results
   clearResult(p);

   // Initialize Parameters
   const string flushSpace = string(100, ' ');
   uint32_t fired = V3NtkUD;
   struct timeval inittime, curtime; gettimeofday(&inittime, NULL);
   uint32_t lastDepth = getIncLastDepthToKeepGoing(); if (10000000 < lastDepth) lastDepth = 0;
   uint32_t boundDepth = lastDepth ? lastDepth : 0;

   // Start BMC Based Verification
   V3Ntk* simpNtk = 0; V3SvrBase* solver = 0;
   while (boundDepth < _maxDepth) {
      // Check Time Bounds
      gettimeofday(&curtime, NULL);
      if (_maxTime < getTimeUsed(inittime, curtime)) break;

      boundDepth += 1;

      // Expand Network and Set Initial States
      V3NtkExpand* const pNtk = new V3NtkExpand(_handler, boundDepth, true); assert (pNtk);

      //printNetlist(pNtk);

      V3NetId id; V3NetVec p2cMap, c2pMap; V3RepIdHash repIdHash; repIdHash.clear();
      simpNtk = duplicateNtk(pNtk, p2cMap, c2pMap); assert (simpNtk);

      //printNetlist(simpNtk);


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
      if (solver->assump_solve()) { fired = boundDepth; break; }
      solver->assertProperty(simpNtk->getOutput(0), true, 0);

      if (!endLineON()) Msg(MSG_IFO) << "\r" + flushSpace + "\r";
      Msg(MSG_IFO) << "Verification completed under depth = "  << boundDepth;

      if (V3NtkUD != fired) break; delete solver; delete simpNtk;

   }

   // Report Verification Result

   if (V3NtkUD != fired) Msg(MSG_IFO) << "Counter-example found at depth = " << fired;
   else Msg(MSG_IFO) << "UNDECIDED at depth = " << _maxDepth;


   // Record CounterExample Trace or Invariant
   if (V3NtkUD != fired) {  // Record Counter-Example
      V3CexTrace* const cex = new V3CexTrace(fired); assert (cex);
      // Set Pattern Value
      uint32_t patternSize = _vrfNtk->getInputSize() + _vrfNtk->getInoutSize();
      V3BitVecX dataValue, patternValue(patternSize ? patternSize : 1);
      for (uint32_t i = 0, inSize = 0, ioSize = 0; i < fired; ++i) {
         patternSize = 0; patternValue.clear();
         for (uint32_t j = 0; j < _vrfNtk->getInputSize(); ++j, ++patternSize, ++inSize) {
            if (!solver->existVerifyData(simpNtk->getInput(inSize), 0)) continue;
            dataValue = solver->getDataValue(simpNtk->getInput(inSize), 0);
            if ('0' == dataValue[0]) patternValue.set0(patternSize);
            else if ('1' == dataValue[0]) patternValue.set1(patternSize);
         }
         for (uint32_t j = 0; j < _vrfNtk->getInoutSize(); ++j, ++patternSize, ++ioSize) {
            if (!solver->existVerifyData(simpNtk->getInout(ioSize), 0)) continue;
            dataValue = solver->getDataValue(simpNtk->getInout(ioSize), 0);
            if ('0' == dataValue[0]) patternValue.set0(patternSize);
            else if ('1' == dataValue[0]) patternValue.set1(patternSize);
         }
         assert (!patternSize || patternSize == patternValue.size()); cex->pushData(patternValue);
      }
      // Set Initial State Value
      if (_vrfNtk->getLatchSize()) {
         const uint32_t piSize = boundDepth * _vrfNtk->getInputSize();
         patternValue.resize(_vrfNtk->getLatchSize());
         patternValue.clear(); V3NetId id; uint32_t k = 0;
         for (uint32_t j = 0; j < _vrfNtk->getLatchSize(); ++j) {
            id = _vrfNtk->getInputNetId(_vrfNtk->getLatch(j), 1);
            if (!id.id) { if (id.cp) patternValue.set1(j); else patternValue.set0(j); }
            else {
               if (solver->existVerifyData(simpNtk->getInput(piSize + k), 0)) {
                  dataValue = solver->getDataValue(simpNtk->getInput(piSize + k), 0);
                  if ('0' == dataValue[0]) patternValue.set0(j);
                  else if ('1' == dataValue[0]) patternValue.set1(j);
               }
               ++k;
            }
         }
         cex->setInit(patternValue);
      }
      delete solver; delete simpNtk;
      _result[p].setCexTrace(cex);

   }
}

#endif

