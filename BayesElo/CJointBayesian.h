// Copyright (c) 2015 MIT License by 6.172 Staff

/////////////////////////////////////////////////////////////////////////////
//
// Rémi Coulom
//
// December, 2004
//
/////////////////////////////////////////////////////////////////////////////
#ifndef CJointBayesian_Declared
#define CJointBayesian_Declared

class CResultSet;
class CDistributionCollection;
class CBradleyTerry;

class CJointBayesian {
private:  //////////////////////////////////////////////////////////////////
  const CResultSet &rs;
  CDistributionCollection &dc;
  const CBradleyTerry &bt;
  int *pindex;
  int indexMax;
  double *pProbabilityCache;

  void RecursiveJointBayesian(int player, int indexTotal);
  double GetProbability() const;

public:  ///////////////////////////////////////////////////////////////////
  CJointBayesian(const CResultSet &rsInit,
                 CDistributionCollection &dcInit,
                 const CBradleyTerry &btInit);

  void RunComputation();

  ~CJointBayesian();
};

#endif  // CJointBayesian_Declared
