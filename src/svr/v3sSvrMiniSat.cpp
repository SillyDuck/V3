
#ifndef V3S_SVR_MSAT_C
#define V3S_SVR_MSAT_C

#include "v3Msg.h"
#include "v3sSvrMiniSat.h"
#include "v3SvrSatHelper.h"

#define V3S_SATSOLVER_PROFILE_ON

/* -------------------------------------------------- *\
 * Class V3sSvrMiniSat Implementations
\* -------------------------------------------------- */
 /*
// Constructor and Destructor
V3sSvrMiniSat::V3sSvrMiniSat(const V3sNtk* const ntk, const bool& freeBound) : _ntk(ntk), _freeBound(freeBound) {
   _Solver = new MSolver(); assert (_Solver); assumeRelease(); initRelease();
   _curVar = 0; _Solver->newVar(l_Undef, false); ++_curVar;  // 0 for Recycle Literal, if Needed
   _ntkData.clear(); _ntkData.reserve(ntk->getNetSize());
   for (uint32_t i = 0; i < ntk->getNetSize(); ++i) {
      _ntkData.push_back(V3SvrMVarData()); _ntkData.back().clear();
   }
}

V3sSvrMiniSat::V3sSvrMiniSat(const V3sSvrMiniSat& solver) : _ntk(solver._ntk), _freeBound(solver._freeBound) {
   _Solver = new MSolver(); assert (_Solver); assumeRelease(); initRelease();
   _curVar = 0; _Solver->newVar(l_Undef, false); ++_curVar;  // 0 for Recycle Literal, if Needed
   _ntkData.clear(); _ntkData.reserve(_ntk->getNetSize());
   for (uint32_t i = 0; i < _ntk->getNetSize(); ++i) {
      _ntkData.push_back(V3SvrMVarData()); _ntkData.back().clear();
   }
}

V3sSvrMiniSat::~V3sSvrMiniSat() {
   delete _Solver; assumeRelease(); initRelease();
   for (uint32_t i = 0; i < _ntkData.size(); ++i) _ntkData[i].clear(); _ntkData.clear();
}

// Basic Operation Functions
void
V3sSvrMiniSat::reset() {
   delete _Solver; _Solver = new MSolver(); assert (_Solver); assumeRelease(); initRelease();
   _curVar = 0; _Solver->newVar(l_Undef, false); ++_curVar;  // 0 for Recycle Literal, if Needed
   for (uint32_t i = 0; i < _ntkData.size(); ++i) _ntkData[i].clear();
   if (_ntkData.size() == _ntk->getNetSize()) return; _ntkData.clear(); _ntkData.reserve(_ntk->getNetSize());
   for (uint32_t i = 0; i < _ntk->getNetSize(); ++i) { _ntkData.push_back(V3SvrMVarData()); _ntkData.back().clear(); }
}

void
V3sSvrMiniSat::update() {
   assert (_Solver); assert (_ntk->getNetSize() >= _ntkData.size());
   V3SvrMVarData svrData; svrData.clear(); _ntkData.resize(_ntk->getNetSize(), svrData);
}

void
V3sSvrMiniSat::assumeInit() {
   for (uint32_t i = 0; i < _init.size(); ++i) _assump.push(_init[i]);
}

void
V3sSvrMiniSat::assertInit() {
   for (uint32_t i = 0; i < _init.size(); ++i) _Solver->addClause(_init[i]);
}

void
V3sSvrMiniSat::initRelease() { _init.clear(); }

void
V3sSvrMiniSat::assumeRelease() { _assump.clear(); }

void
V3sSvrMiniSat::assumeProperty(const size_t& var, const bool& invert) {
   _assump.push(mkLit(getOriVar(var), invert ^ isNegFormula(var)));
}

void
V3sSvrMiniSat::assertProperty(const size_t& var, const bool& invert) {
   _Solver->addClause(mkLit(getOriVar(var), invert ^ isNegFormula(var)));
}

void
V3sSvrMiniSat::assumeProperty(const V3NetId& id, const bool& invert, const uint32_t& depth) {
   assert (validNetId(id)); assert (1 == _ntk->getNetWidth(id));
   const Var var = getVerifyData(id, depth); assert (var);
   _assump.push(mkLit(var, invert ^ isV3NetInverted(id)));
}

void
V3sSvrMiniSat::assertProperty(const V3NetId& id, const bool& invert, const uint32_t& depth) {
   assert (validNetId(id)); assert (1 == _ntk->getNetWidth(id));
   const Var var = getVerifyData(id, depth); assert (var);
   _Solver->addClause(mkLit(var, invert ^ isV3NetInverted(id)));
}

const bool
V3sSvrMiniSat::simplify() { return _Solver->simplify(); }

const bool
V3sSvrMiniSat::solve() {
   #ifdef V3S_SATSOLVER_PROFILE_ON
   double ctime = (double)clock() / CLOCKS_PER_SEC;
   #endif

   _Solver->solve();

   #ifdef V3S_SATSOLVER_PROFILE_ON
   ++_solves;
   _runTime += (((double)clock() / CLOCKS_PER_SEC) - ctime);
   #endif
   return _Solver->okay();
}

const bool
V3sSvrMiniSat::assump_solve() {
   #ifdef V3S_SATSOLVER_PROFILE_ON
   double ctime = (double)clock() / CLOCKS_PER_SEC;
   #endif

   _Solver->solve();

   #ifdef V3S_SATSOLVER_PROFILE_ON
   ++_solves;
   _runTime += (((double)clock() / CLOCKS_PER_SEC) - ctime);
   #endif
   return result;
}

// Manipulation Helper Functions
void
V3sSvrMiniSat::setTargetValue(const V3NetId& id, const V3BitVecX& value, const uint32_t& depth, V3SvrDataVec& formula) {
   // Note : This Function will set formula such that AND(formula) represents (id == value)
   uint32_t i, size = value.size(); assert (size == _ntk->getNetWidth(id));
   const Var var = getVerifyData(id, depth); assert (var);
   if (isV3NetInverted(id)) {
      for (i = 0; i < size; ++i) {
         if ('1' == value[i]) formula.push_back(getNegVar(var + i));
         else if ('0' == value[i]) formula.push_back(getPosVar(var + i));
      }
   }
   else {
      for (i = 0; i < size; ++i) {
         if ('1' == value[i]) formula.push_back(getPosVar(var + i));
         else if ('0' == value[i]) formula.push_back(getNegVar(var + i));
      }
   }
}

void
V3sSvrMiniSat::assertImplyUnion(const V3SvrDataVec& vars) {
   // Construct a CNF formula (var1 + var2 + ... + varn) and add to the solver
   if (vars.size() == 0) return; vec<Lit> lits; lits.clear();
   for (V3SvrDataVec::const_iterator it = vars.begin(); it != vars.end(); ++it) {
      assert (*it); lits.push(mkLit(getOriVar(*it), isNegFormula(*it)));
   }
   _Solver->addClause(lits); lits.clear();
}

const size_t
V3sSvrMiniSat::setTargetValue(const V3NetId& id, const V3BitVecX& value, const uint32_t& depth, const size_t& prev) {
   // Construct formula y = b0 & b1' & b3 & ... & bn', and return variable y
   assert (!prev || !isNegFormula(prev));  // Constrain input prev variable should NOT be negative!
   uint32_t i, size = value.size(); assert (size == _ntk->getNetWidth(id));
   const Var _var = getVerifyData(id, depth); assert (_var);
   Lit aLit = (prev) ? mkLit(getOriVar(prev)) : lit_Undef, bLit, yLit;
   vec<Lit> lits; lits.clear();
   for (i = 0; i < size; ++i) {
      if ('1' == value[i]) bLit = mkLit(_var + i, isV3NetInverted(id));
      else if ('0' == value[i]) bLit = ~mkLit(_var + i, isV3NetInverted(id));
      else bLit = lit_Undef;
      if (!(bLit == lit_Undef)) {
         if (!(aLit == lit_Undef)) {
            yLit = mkLit(newVar(1));
            lits.push(aLit); lits.push(~yLit); _Solver->addClause(lits); lits.clear();
            lits.push(bLit); lits.push(~yLit); _Solver->addClause(lits); lits.clear();
            lits.push(~aLit); lits.push(~bLit); lits.push(yLit); _Solver->addClause(lits); lits.clear();
            aLit = yLit; assert (!sign(aLit));
         }
         else aLit = bLit;
      }
   }
   if (aLit == lit_Undef) return 0;
   else if (sign(aLit)) {
      yLit = mkLit(newVar(1));
      lits.push(~aLit); lits.push(yLit); _Solver->addClause(lits); lits.clear();
      lits.push(aLit); lits.push(~yLit); _Solver->addClause(lits); lits.clear();
      aLit = yLit;
   }
   assert (!isNegFormula(getPosVar(var(aLit))));
   return getPosVar(var(aLit));
}

const size_t
V3sSvrMiniSat::setImplyUnion(const V3SvrDataVec& vars) {
   // Construct a CNF formula (y' + var1 + var2 + ... + varn), and return variable y
   if (vars.size() == 0) return 0; vec<Lit> lits; lits.clear();
   Lit lit = mkLit(newVar(1), true); lits.push(lit);
   for (V3SvrDataVec::const_iterator it = vars.begin(); it != vars.end(); ++it) {
      assert (*it); lits.push(mkLit(getOriVar(*it), isNegFormula(*it)));
   }
   _Solver->addClause(lits); lits.clear();
   assert (!isNegFormula(getPosVar(var(lit))));
   return getPosVar(var(lit));
}

const size_t
V3sSvrMiniSat::setImplyIntersection(const V3SvrDataVec& vars) {
   // Goal : y --> (var1 && var2 && ... && varn)
   // Construct CNF formulas (y' + var1) && (y' + var2) &&  ... (y' + varn), and return variable y
   if (vars.size() == 0) return 0;
   Lit lit = mkLit(newVar(1), true);
   vec<Lit> lits; lits.clear();
   for (V3SvrDataVec::const_iterator it = vars.begin(); it != vars.end(); ++it) {
      assert (*it); lits.push(lit);
      lits.push(mkLit(getOriVar(*it), isNegFormula(*it)));
      _Solver->addClause(lits); lits.clear();
   }
   assert (!isNegFormula(getPosVar(var(lit))));
   return getPosVar(var(lit));
}

const size_t
V3sSvrMiniSat::setImplyInit() {
   Lit lit = mkLit(newVar(1), true);
   vec<Lit> lits; lits.clear();
   for (uint32_t i = 0; i < _init.size(); ++i) {
      lits.push(lit); lits.push(_init[i]); _Solver->addClause(lits); lits.clear();
   }
   assert (!isNegFormula(getPosVar(var(lit))));
   return getPosVar(var(lit));
}

const V3BitVecX
V3sSvrMiniSat::getDataValue(const V3NetId& id, const uint32_t& depth) const {
   Var var = getVerifyData(id, depth); assert (var);
   uint32_t i, width = _ntk->getNetWidth(id);
   V3BitVecX value(width);
   if (isV3NetInverted(id)) {
      for (i = 0; i < width; ++i)
         if (l_True == _Solver->model[var + i]) value.set0(i);
         else value.set1(i);
   }
   else {
      for (i = 0; i < width; ++i)
         if (l_True == _Solver->model[var + i]) value.set1(i);
         else value.set0(i);
   }
   return value;
}

const bool
V3sSvrMiniSat::getDataValue(const size_t& var) const {
   return (isNegFormula(var)) ^ (l_True == _Solver->model[getOriVar(var)]);
}

void
V3sSvrMiniSat::getDataConflict(V3SvrDataVec& vars) const {
   for (int i = 0; i < _Solver->conflict.size(); ++i)
      vars.push_back(getPosVar(var(_Solver->conflict[i])));
}

const size_t
V3sSvrMiniSat::getFormula(const V3NetId& id, const uint32_t& depth) {
   Var var = getVerifyData(id, depth); assert (var);
   assert (!isNegFormula(getPosVar(var)));
   return (isV3NetInverted(id) ? getNegVar(var) : getPosVar(var));
}

const size_t
V3sSvrMiniSat::getFormula(const V3NetId& id, const uint32_t& bit, const uint32_t& depth) {
   Var var = getVerifyData(id, depth); assert (var);
   assert (bit < _ntk->getNetWidth(id)); assert (!isNegFormula(getPosVar(var + bit)));
   return (isV3NetInverted(id) ? getNegVar(var + bit) : getPosVar(var + bit));
}

// Print Data Functions
void
V3sSvrMiniSat::printInfo() const {
   Msg(MSG_IFO) << "#Vars = " << _Solver->nVars() << ", #Cls = " << _Solver->nClauses() << ", " 
                << "#SV = " << totalSolves() << ", AccT = " << totalTime();
}

void
V3sSvrMiniSat::printVerbose() const {
   //_Solver->toDimacs(stdout, vec<Lit>());
}

// Resource Functions
const double
V3sSvrMiniSat::getTime() const {
   return totalTime();
}

const int
V3sSvrMiniSat::getMemory() const {
   // NOTE: 1G for 16M clauses
   return _Solver->nClauses() >> 4;
}

// Gate Formula to Solver Functions
void
V3sSvrMiniSat::add_FALSE_Formula(const V3NetId& out, const uint32_t& depth) {
   // Check Output Validation
   assert (validNetId(out)); assert (AIG_FALSE == _ntk->getGateType(out)); assert (!getVerifyData(out, depth));
   const uint32_t index = getV3NetIndex(out); assert (depth == _ntkData[index].size());
   // Set SATVar
   _ntkData[index].push_back(newVar(1)); assert (getVerifyData(out, depth));
   _Solver->addClause(mkLit(_ntkData[index].back(), true));
}

void
V3sSvrMiniSat::add_PI_Formula(const V3NetId& out, const uint32_t& depth) {
   // Check Output Validation
   assert (validNetId(out)); assert (V3_PI == _ntk->getGateType(out)); assert (!getVerifyData(out, depth));
   const uint32_t index = getV3NetIndex(out); assert (depth == _ntkData[index].size());
   // Set SATVar
   _ntkData[index].push_back(newVar(_ntk->getNetWidth(out))); assert (getVerifyData(out, depth));
}

void
V3sSvrMiniSat::add_FF_Formula(const V3NetId& out, const uint32_t& depth) {
   // Check Output Validation
   assert (validNetId(out)); assert (V3_FF == _ntk->getGateType(out)); assert (!getVerifyData(out, depth));
   const uint32_t index = getV3NetIndex(out); assert (depth == _ntkData[index].size());
   const uint32_t width = _ntk->getNetWidth(out); assert (width);
   if (_freeBound) {
      // Set SATVar
      _ntkData[index].push_back(newVar(width));
   }
   else if (depth) {
      // Build FF I/O Relation
      const V3NetId in1 = _ntk->getInputNetId(out, 0); assert (validNetId(in1));
      const Var var1 = getVerifyData(in1, depth - 1); assert (var1);
      // Set SATVar
      if (isV3NetInverted(in1)) {
         _ntkData[index].push_back(newVar(width));
         for (uint32_t i = 0; i < width; ++i) 
            buf(_Solver, mkLit(_ntkData[index].back() + i), mkLit(var1 + i, true));
      }
      else _ntkData[index].push_back(var1);
   }
   else {
      // Set SATVar
      _ntkData[index].push_back(newVar(width));
      const Var& var = _ntkData[index].back();
      // Build FF Initial State
      const V3NetId in1 = _ntk->getInputNetId(out, 1); assert (validNetId(in1));
      const V3BvNtk* const ntk = dynamic_cast<const V3BvNtk*>(_ntk);
      if (ntk) {
         if (BV_CONST == ntk->getGateType(in1)) {
            const V3BitVecX value = ntk->getInputConstValue(in1); assert (width == value.size());
            for (uint32_t i = 0; i < width; ++i)
               if ('X' != value[i]) _init.push_back(mkLit(var + i, '0' == value[i]));
         }
         else if (out.id != in1.id) {  // Build Initial Circuit
            const Var var1 = getVerifyData(in1, 0); assert (var1);
            const Var initVar = newVar(width + 1);
            for (uint32_t i = 0; i < width; ++i) 
               xor_2(_Solver, mkLit(1 + initVar + i, true), mkLit(var + i), mkLit(var1 + i, isV3NetInverted(in1)));
            and_red(_Solver, mkLit(initVar), mkLit(1 + initVar), width); _init.push_back(mkLit(initVar));
         }
      }
      else {
         if (AIG_FALSE == _ntk->getGateType(in1)) _init.push_back(mkLit(var, !isV3NetInverted(in1)));
         else if (out.id != in1.id) {  // Build Initial Circuit
            const Var var1 = getVerifyData(in1, 0); assert (var1);
            const Var initVar = newVar(1);
            xor_2(_Solver, mkLit(initVar, true), mkLit(var), mkLit(var1, isV3NetInverted(in1)));
            _init.push_back(mkLit(initVar));
         }
      }
   }
   assert (getVerifyData(out, depth));
}

void
V3sSvrMiniSat::add_AND_Formula(const V3NetId& out, const uint32_t& depth) {
   // Check Output Validation
   assert (validNetId(out)); assert (!getVerifyData(out, depth));
   assert ((AIG_NODE == _ntk->getGateType(out)) || (BV_AND == _ntk->getGateType(out)));
   const uint32_t index = getV3NetIndex(out); assert (depth == _ntkData[index].size());
   const uint32_t width = _ntk->getNetWidth(out); assert (width);
   // Set SATVar
   _ntkData[index].push_back(newVar(_ntk->getNetWidth(out))); assert (getVerifyData(out, depth));
   const Var& var = _ntkData[index].back();
   // Build AND I/O Relation
   const V3NetId in1 = _ntk->getInputNetId(out, 0); assert (validNetId(in1));
   const V3NetId in2 = _ntk->getInputNetId(out, 1); assert (validNetId(in2));
   const Var var1 = getVerifyData(in1, depth); assert (var1);
   const Var var2 = getVerifyData(in2, depth); assert (var2);
   for (uint32_t i = 0; i < width; ++i) 
      and_2(_Solver, mkLit(var + i), mkLit(var1 + i, isV3NetInverted(in1)), mkLit(var2 + i, isV3NetInverted(in2)));
}

// Network to Solver Functions
const bool
V3sSvrMiniSat::existVerifyData(const V3NetId& id, const uint32_t& depth) {
   return getVerifyData(id, depth);
}

// MiniSat Functions
const Var
V3sSvrMiniSat::newVar(const uint32_t& width) {
   Var cur_var = _curVar;
   for (uint32_t i = 0; i < width; ++i) _Solver->newVar();
   _curVar += width; return cur_var;
}

const Var
V3sSvrMiniSat::getVerifyData(const V3NetId& id, const uint32_t& depth) const {
   assert (validNetId(id));
   if (depth >= _ntkData[id.id].size()) return 0;
   else return _ntkData[id.id][depth];
}

void
V3sSvrMiniSat::addSimpleBoundedVerifyData(V3NetId id, uint32_t depth) {
   V3Stack<pair<V3NetId, uint32_t> >::Stack netIdList; assert (!netIdList.size());
   pair<V3NetId, uint32_t> netId = make_pair(id, depth); netIdList.push(netId);
   V3GateType type;

   while (!netIdList.empty()) {
      netId = netIdList.top(); id = netId.first; depth = netId.second;
      assert (validNetId(id)); assert (!existVerifyData(id, depth));
      type = _ntk->getGateType(id); assert (type < V3_XD);
      if (V3_PIO >= type) add_PI_Formula(id, depth);
      else if (V3_FF == type) {
         if (depth) {
            netId.first = _ntk->getInputNetId(id, 0); --netId.second;
            if (!existVerifyData(netId.first, netId.second)) { netIdList.push(netId); continue; }
         }
         else {
            netId.first = _ntk->getInputNetId(id, 1);
            if (id.id != netId.first.id && !existVerifyData(netId.first, depth)) { netIdList.push(netId); continue; }
         }
         add_FF_Formula(id, depth);
      }
      else if (AIG_FALSE >= type) {
         if (AIG_NODE == type) {
            netId.first = _ntk->getInputNetId(id, 0); if (!existVerifyData(netId.first, depth)) { netIdList.push(netId); continue; }
            netId.first = _ntk->getInputNetId(id, 1); if (!existVerifyData(netId.first, depth)) { netIdList.push(netId); continue; }
            add_AND_Formula(id, depth);
         }
         else { assert (AIG_FALSE == type); add_FALSE_Formula(id, depth); }
      }
      else if (isV3PairType(type)) {
         assert (BV_AND == type);
         netId.first = _ntk->getInputNetId(id, 0); if (!existVerifyData(netId.first, depth)) { netIdList.push(netId); continue; }
         netId.first = _ntk->getInputNetId(id, 1); if (!existVerifyData(netId.first, depth)) { netIdList.push(netId); continue; }
         add_AND_Formula(id, depth);      break;
      }
      else { assert (BV_CONST == type); add_CONST_Formula(id, depth); }
      netIdList.pop();
   }
}
*/
#endif

