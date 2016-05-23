/****************************************************************************
  FileName     [ v3NtkTemDecomp.h ]
  PackageName  [ v3/src/trans ]
  Synopsis     [ Temporal Decomposition to V3 Network. ]
  Author       [ SillyDuck ]
  Copyright    [ Copyright(c) 2016 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef V3_NTK_TEMDECOMP_H
#define V3_NTK_TEMDECOMP_H

#include "v3NtkHandler.h"
#include "v3BitVec.h"

// class V3NtkExpand : Temporal Decomposition
class V3NtkTemDecomp : public V3NtkHandler
{
   public : 
      // Constructor and Destructor
      V3NtkTemDecomp(V3NtkHandler* const, const uint32_t&, V3BitVecX &, bool);
      ~V3NtkTemDecomp();
      // Inline Member Functions
      inline const uint32_t& getCycle() const { return _cycle; }
      // I/O Ancestry Functions
      const string getInputName(const uint32_t&) const;
      const string getOutputName(const uint32_t&) const;
      const string getInoutName(const uint32_t&) const;
      // Net Ancestry Functions
      void getNetName(V3NetId&, string&) const;
      const V3NetId getNetFromName(const string&) const;
      const V3NetId getParentNetId(const V3NetId&) const;
      const V3NetId getCurrentNetId(const V3NetId&, const uint32_t&) const;
   //private : 
      // Transformation Functions
      void performNtkTransformation(V3BitVecX &, bool);
      // Private Members
      const uint32_t _cycle;     // Number of Cycles for Expansion
      V3NetVec       _c2pMap;    // V3NetId Mapping From Current to Parent Ntk
      V3NetTable     _p2cMap;    // V3NetId Mapping From Parent to Current Ntk
      V3NetTable     _latchMap;
};

#endif

