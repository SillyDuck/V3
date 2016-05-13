
#ifndef V3S_SVR_MSAT_C
#define V3S_SVR_MSAT_C

#include "v3Msg.h"
#include "v3sSvrMiniSat.h"
#include "v3SvrSatHelper.h"

#define V3S_SATSOLVER_PROFILE_ON

/* -------------------------------------------------- *\
 * Class V3SSvrMiniSat Implementations
\* -------------------------------------------------- */
// Constructor and Destructor
V3SSvrMiniSat::V3SSvrMiniSat(const V3Ntk* const ntk, const bool& freeBound) : V3SvrBase(ntk, freeBound) {
   _Solver = new MSolver(); assert (_Solver); assumeRelease(); initRelease();
   _curVar = 0; _Solver->newVar(l_Undef, false); ++_curVar;  // 0 for Recycle Literal, if Needed
   _ntkData.clear(); _ntkData.reserve(ntk->getNetSize());
   for (uint32_t i = 0; i < ntk->getNetSize(); ++i) { _ntkData.push_back(V3SvrMVarData()); _ntkData.back().clear(); }
}

V3SSvrMiniSat::V3SSvrMiniSat(const V3SSvrMiniSat& solver) : V3SvrBase(solver._ntk, solver._freeBound) {
   _Solver = new MSolver(); assert (_Solver); assumeRelease(); initRelease();
   _curVar = 0; _Solver->newVar(l_Undef, false); ++_curVar;  // 0 for Recycle Literal, if Needed
   _ntkData.clear(); _ntkData.reserve(_ntk->getNetSize());
   for (uint32_t i = 0; i < _ntk->getNetSize(); ++i) { _ntkData.push_back(V3SvrMVarData()); _ntkData.back().clear(); }
}

V3SSvrMiniSat::~V3SSvrMiniSat() {
   delete _Solver; assumeRelease(); initRelease();
   for (uint32_t i = 0; i < _ntkData.size(); ++i) _ntkData[i].clear(); _ntkData.clear();
}

// Basic Operation Functions
void
V3SSvrMiniSat::reset() {
   delete _Solver; _Solver = new MSolver(); assert (_Solver); assumeRelease(); initRelease();
   _curVar = 0; _Solver->newVar(l_Undef, false); ++_curVar;  // 0 for Recycle Literal, if Needed
   for (uint32_t i = 0; i < _ntkData.size(); ++i) _ntkData[i].clear();
   if (_ntkData.size() == _ntk->getNetSize()) return; _ntkData.clear(); _ntkData.reserve(_ntk->getNetSize());
   for (uint32_t i = 0; i < _ntk->getNetSize(); ++i) { _ntkData.push_back(V3SvrMVarData()); _ntkData.back().clear(); }
}

void
V3SSvrMiniSat::update() {
   assert (_Solver); assert (_ntk->getNetSize() >= _ntkData.size());
   V3SvrMVarData svrData; svrData.clear(); _ntkData.resize(_ntk->getNetSize(), svrData);
}

void
V3SSvrMiniSat::assumeInit() {
   for (uint32_t i = 0; i < _init.size(); ++i) _assump.push(_init[i]);
}

void
V3SSvrMiniSat::assertInit() {
   //cerr << "assertInit\n";
   for (uint32_t i = 0; i < _init.size(); ++i) _Solver->addClause(_init[i]);
}

void
V3SSvrMiniSat::initRelease() { _init.clear(); }

void
V3SSvrMiniSat::assumeRelease() { _assump.clear(); }

void
V3SSvrMiniSat::assumeProperty(const size_t& var, const bool& invert) {
   _assump.push(mkLit(getOriVar(var), invert ^ isNegFormula(var)));
}

void
V3SSvrMiniSat::assertProperty(const size_t& var, const bool& invert) {
   //cerr << "assertProperty " << var << endl;
   _Solver->addClause(mkLit(getOriVar(var), invert ^ isNegFormula(var)));
}

void
V3SSvrMiniSat::assumeProperty(const V3NetId& id, const bool& invert, const uint32_t& depth) {
   assert (validNetId(id)); assert (1 == _ntk->getNetWidth(id));
   const Var var = getVerifyData(id, depth); assert (var);
   _assump.push(mkLit(var, invert ^ id.cp));
   //cerr << "assumeProperty : " <<  var  << endl;
}

void
V3SSvrMiniSat::assertProperty(const V3NetId& id, const bool& invert, const uint32_t& depth) {
   assert (validNetId(id)); assert (1 == _ntk->getNetWidth(id));
   const Var var = getVerifyData(id, depth); assert (var);
   _Solver->addClause(mkLit(var, invert ^ isV3NetInverted(id)));
   //cerr << "assertProperty " << var << endl;
}

const bool
V3SSvrMiniSat::simplify() { return _Solver->simplify(); }

const bool
V3SSvrMiniSat::solve() {
   double ctime = (double)clock() / CLOCKS_PER_SEC;
   _Solver->solve(); ++_solves;
   _runTime += (((double)clock() / CLOCKS_PER_SEC) - ctime);
   return _Solver->okay();
}

const bool
V3SSvrMiniSat::assump_solve() {
   double ctime = (double)clock() / CLOCKS_PER_SEC;
   bool result = _Solver->solve(_assump); ++_solves;
   _runTime += (((double)clock() / CLOCKS_PER_SEC) - ctime);
   return result;
}

// Manipulation Helper Functions
void
V3SSvrMiniSat::setTargetValue(const V3NetId& id, const V3BitVecX& value, const uint32_t& depth, V3SvrDataVec& formula) {
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
V3SSvrMiniSat::assertImplyUnion(const V3SvrDataVec& vars) {
   // Construct a CNF formula (var1 + var2 + ... + varn) and add to the solver
   if (vars.size() == 0) return; vec<Lit> lits; lits.clear();
   for (V3SvrDataVec::const_iterator it = vars.begin(); it != vars.end(); ++it) {
      assert (*it); lits.push(mkLit(getOriVar(*it), isNegFormula(*it)));
   }
   _Solver->addClause(lits); lits.clear();
   //cerr << "assertImplyUnion " << endl;
}

const size_t
V3SSvrMiniSat::setTargetValue(const V3NetId& id, const V3BitVecX& value, const uint32_t& depth, const size_t& prev) {
   //cerr << "setTargetValue" << endl;
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
V3SSvrMiniSat::setImplyUnion(const V3SvrDataVec& vars) {
   //cerr << "setImplyUnion" << endl;
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
V3SSvrMiniSat::setImplyIntersection(const V3SvrDataVec& vars) {
   //cerr << "setImplyIntersection" << endl;
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
V3SSvrMiniSat::setImplyInit() {
   //cerr << "setImplyInit" << endl;
   Lit lit = mkLit(newVar(1), true);
   vec<Lit> lits; lits.clear();
   for (uint32_t i = 0; i < _init.size(); ++i) {
      lits.push(lit); lits.push(_init[i]); _Solver->addClause(lits); lits.clear();
   }
   assert (!isNegFormula(getPosVar(var(lit))));
   return getPosVar(var(lit));
}

const V3BitVecX
V3SSvrMiniSat::getDataValue(const V3NetId& id, const uint32_t& depth) const {
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
V3SSvrMiniSat::getDataValue(const size_t& var) const {
   return (isNegFormula(var)) ^ (l_True == _Solver->model[getOriVar(var)]);
}

void
V3SSvrMiniSat::getDataConflict(V3SvrDataVec& vars) const {
   for (int i = 0; i < _Solver->conflict.size(); ++i)
      vars.push_back(getPosVar(var(_Solver->conflict[i])));
}

const size_t
V3SSvrMiniSat::getFormula(const V3NetId& id, const uint32_t& depth) {
   Var var = getVerifyData(id, depth); assert (var);
   assert (!isNegFormula(getPosVar(var)));
   return (isV3NetInverted(id) ? getNegVar(var) : getPosVar(var));
}

const size_t
V3SSvrMiniSat::getFormula(const V3NetId& id, const uint32_t& bit, const uint32_t& depth) {
   Var var = getVerifyData(id, depth); assert (var);
   assert (bit < _ntk->getNetWidth(id)); assert (!isNegFormula(getPosVar(var + bit)));
   return (isV3NetInverted(id) ? getNegVar(var + bit) : getPosVar(var + bit));
}

// Print Data Functions
void
V3SSvrMiniSat::printInfo() const {
   Msg(MSG_IFO) << "#Vars = " << _Solver->nVars() << ", #Cls = " << _Solver->nClauses() << ", " 
                << "#SV = " << totalSolves() << ", AccT = " << totalTime();
   //cerr << endl;
}

void
V3SSvrMiniSat::printVerbose() const {
   for (unsigned i = 0, s = _ntk->getNetSize(); i < s; ++i){
       //cerr << i << " " << getVerifyData(i,0) << endl;
    }
   //_Solver->toDimacs(stdout, vec<Lit>());
}

// Resource Functions
const double
V3SSvrMiniSat::getTime() const {
   return totalTime();
}

const int
V3SSvrMiniSat::getMemory() const {
   // NOTE: 1G for 16M clauses
   return _Solver->nClauses() >> 4;
}

// Gate Formula to Solver Functions
void
V3SSvrMiniSat::add_FALSE_Formula(const V3NetId& out, const uint32_t& depth) {
   // Check Output Validation
   assert (validNetId(out)); assert (AIG_FALSE == _ntk->getGateType(out)); assert (!getVerifyData(out, depth));
   const uint32_t index = getV3NetIndex(out); assert (depth == _ntkData[index].size());
   // Set SATVar
   _ntkData[index].push_back(newVar(1)); assert (getVerifyData(out, depth));
   _Solver->addClause(mkLit(_ntkData[index].back(), true));
   //cerr << "add_FALSE_Formula id: " << out.id << " var : " << _ntkData[index].back() << endl;
}

void
V3SSvrMiniSat::add_PI_Formula(const V3NetId& out, const uint32_t& depth) {
   // Check Output Validation
   assert (validNetId(out)); assert (V3_PI == _ntk->getGateType(out)); assert (!getVerifyData(out, depth));
   const uint32_t index = getV3NetIndex(out); assert (depth == _ntkData[index].size());
   // Set SATVar
   _ntkData[index].push_back(newVar(1)); assert (getVerifyData(out, depth));
   //cerr << "add_PI_Formula : "  << out.id << endl;
}

void
V3SSvrMiniSat::add_FF_Formula(const V3NetId& out, const uint32_t& depth) {
   //cerr << "add_FF_Formula : " << out.id << " : " << depth << endl;
   // Check Output Validation
   assert (validNetId(out)); assert (V3_FF == _ntk->getGateType(out)); assert (!getVerifyData(out, depth));
   const uint32_t index = out.id; assert (depth == _ntkData[index].size());
   const uint32_t width = _ntk->getNetWidth(out); assert (width == 1);
   if (_freeBound) {
      // Set SATVar
      _ntkData[index].push_back(newVar(width));
   }
   else if (depth) {
      // Build FF I/O Relation
      const V3NetId in1 = _ntk->getInputNetId(out, 0); assert (validNetId(in1));
      const Var var1 = getVerifyData(in1, depth - 1); assert (var1);
      // Set SATVar
      if (in1.cp) {
         _ntkData[index].push_back(newVar(width));
         buf(_Solver, mkLit(_ntkData[index].back()), mkLit(var1, true));
      }
      else _ntkData[index].push_back(var1);
   }
   else {
      // Set SATVar
      _ntkData[index].push_back(newVar(width));
      const Var& var = _ntkData[index].back();
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
   }
   assert (getVerifyData(out, depth));
}

void
V3SSvrMiniSat::add_AND_Formula(const V3NetId& out, const uint32_t& depth) {

   // Check Output Validation
   assert (validNetId(out)); assert (!getVerifyData(out, depth));
   assert (AIG_NODE == _ntk->getGateType(out));
   const uint32_t index = out.id; assert (depth == _ntkData[index].size());
   const uint32_t width = _ntk->getNetWidth(out); assert (width == 1);
   // Set SATVar
   _ntkData[index].push_back(newVar(width)); assert (getVerifyData(out, depth));
   const Var& var = _ntkData[index].back();
   // Build AND I/O Relation
   const V3NetId in1 = _ntk->getInputNetId(out, 0); assert (validNetId(in1));
   const V3NetId in2 = _ntk->getInputNetId(out, 1); assert (validNetId(in2));
   const Var var1 = getVerifyData(in1, depth); assert (var1);
   const Var var2 = getVerifyData(in2, depth); assert (var2);
   and_2(_Solver, mkLit(var), mkLit(var1, in1.cp), mkLit(var2 , in2.cp));
   //cerr << "add_AND_Formula : " << out.id << " " << var1 << " " << var2  << " depth : " << depth << endl;
}



// Network to Solver Functions
const bool
V3SSvrMiniSat::existVerifyData(const V3NetId& id, const uint32_t& depth) {
   return getVerifyData(id, depth);
}

// MiniSat Functions
const Var
V3SSvrMiniSat::newVar(const uint32_t& width) {
   Var cur_var = _curVar;
   for (uint32_t i = 0; i < width; ++i) _Solver->newVar();
   _curVar += width; return cur_var;
}

const Var
V3SSvrMiniSat::getVerifyData(const V3NetId& id, const uint32_t& depth) const {
   assert (validNetId(id));
   if (depth >= _ntkData[id.id].size()) return 0;
   else return _ntkData[id.id][depth];
}

const Var
V3SSvrMiniSat::getVerifyData(const uint32_t& id, const uint32_t& depth) const {
   if (depth >= _ntkData[id].size()) return 0;
   else return _ntkData[id][depth];
}

#endif

