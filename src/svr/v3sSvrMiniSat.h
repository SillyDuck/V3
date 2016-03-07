/*
typedef size_t                      V3SvrData;   // why size_t here... should just use var(int)
typedef V3Vec<V3SvrData>::Vec       V3SvrDataVec;
typedef V3Set<V3SvrData>::Set       V3SvrDataSet;
typedef V3Vec<V3SvrDataVec>::Vec    V3SvrDataTable;
*/

#ifndef V3S_SVR_MSAT_H
#define V3S_SVR_MSAT_H

//#include "v3Ntk.h"
#include "v3sNtk.h"
#include "v3SvrType.h"

class V3SSvrMiniSat
{
   public : 
      // Constructor and Destructor
      V3SSvrMiniSat(const V3SNtk* const, const bool& = false);
      virtual ~V3SSvrMiniSat();
      // Basic Operation Functions
      void reset();
      void update();
      void assumeInit();
      void assertInit();
      void initRelease();
      void assumeRelease();
      void assumeProperty(const size_t&, const bool& = false);
      void assertProperty(const size_t&, const bool& = false);
      void assumeProperty(const V3NetId&, const bool&, const uint32_t&);
      void assertProperty(const V3NetId&, const bool&, const uint32_t&);
      const bool simplify();
      const bool solve();
      const bool assump_solve();
      // Manipulation Helper Functions
      void setTargetValue(const V3NetId&, const V3BitVecX&, const uint32_t&, V3SvrDataVec&);
      void assertImplyUnion(const V3SvrDataVec&);
      const size_t setTargetValue(const V3NetId&, const V3BitVecX&, const uint32_t&, const size_t&);
      const size_t setImplyUnion(const V3SvrDataVec&);
      const size_t setImplyIntersection(const V3SvrDataVec&);
      const size_t setImplyInit();
      // Retrieval Functions
      const V3BitVecX getDataValue(const V3NetId&, const uint32_t&) const;
      const bool getDataValue(const size_t&) const;
      void getDataConflict(V3SvrDataVec&) const;
      const size_t getFormula(const V3NetId&, const uint32_t&);
      const size_t getFormula(const V3NetId&, const uint32_t&, const uint32_t&);
      // Variable Interface Functions
      inline const size_t reserveFormula() { return getPosVar(newVar(1)); }
      inline const bool isNegFormula(const size_t& v) const { return (v & 1ul); }
      inline const size_t getNegFormula(const size_t& v) const { return (v ^ 1ul); }
      // Print Data Functions
      void printInfo() const;
      void printVerbose() const;
      // Resource Functions
      const double getTime() const;
      const int getMemory() const;

      // Gate Formula to Solver Functions
      void add_FALSE_Formula(const V3NetId&, const uint32_t&);
      void add_PI_Formula(const V3NetId&, const uint32_t&);
      void add_FF_Formula(const V3NetId&, const uint32_t&);
      void add_AND_Formula(const V3NetId&, const uint32_t&);
      const bool existVerifyData(const V3NetId&, const uint32_t&);
      // Network to Solver Functions
      void addBoundedVerifyData(const V3NetId&, const uint32_t&);
      void assertBoundedVerifyData(const V3NetVec&, const uint32_t&);
      // Inline Base Functions
      inline const uint32_t totalSolves() const { return _solves; }
      inline const double totalTime() const { return _runTime; }
      inline const bool validNetId(const V3NetId& id) const { return _ntk->getNetSize() > id.id; }

      //v3s functions
      void addFF_FaninConetoSolver(MSolver* solver, V3NetId id){
         assert( getGateType(id) == 'V3_FF' );
      }
   protected : 
      // Private Network to Solver Functions
      void addVerifyData(const V3NetId&, const uint32_t&);
      void addSimpleBoundedVerifyData(V3NetId, uint32_t);
      // Data Members
      const V3SNtk* const   _ntk;       // Network Under Verification
      uint32_t             _solves;    // Number of Solve Called
      double               _runTime;   // Total Runtime in Solving
      // Configurations
      const bool           _freeBound; // Set FF Bounds Free

      ////////////////////////////////////////////////////////////////////////////////////////////

      // MiniSat Functions
      const Var newVar(const uint32_t&);
      const Var getVerifyData(const V3NetId&, const uint32_t&) const;
      // Helper Functions : Transformation Between Internal and External Representations
      inline const Var getOriVar(const size_t& v) const { return (Var)(v >> 1ul); }
      inline const size_t getPosVar(const Var& v) const { return (((size_t)v) << 1ul); }
      inline const size_t getNegVar(const Var& v) const { return ((getPosVar(v)) | 1ul); }
      // Data Members
      MSolver*       _Solver;    // Pointer to a Minisat solver
      Var            _curVar;    // Latest Fresh Variable
      vec<Lit>       _assump;    // Assumption List for assumption solve
      V3SvrMLitData  _init;      // Initial state Var storage
      V3SvrMVarTable _ntkData;   // Mapping between V3NetId and Solver Data
};

#endif 