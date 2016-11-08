// Copyright (c) 2015 MIT License by 6.172 Staff

/////////////////////////////////////////////////////////////////////////////
//
// Rémi Coulom
//
// December, 2004
//
/////////////////////////////////////////////////////////////////////////////
#ifndef CDistributionCollection_Declared
#define CDistributionCollection_Declared

#include "./CDiscretization.h"

class CCDistribution;

class CDistributionCollection: public CDiscretization {  // dc
private:  //////////////////////////////////////////////////////////////////
  CCDistribution **ppDistribution;
  int Players;

public:  ///////////////////////////////////////////////////////////////////
  CDistributionCollection(int PlayersInit, int Size, double Min, double Max);

  CCDistribution &GetDistribution(int i) {return *ppDistribution[i];}

  ~CDistributionCollection();
};

#endif  // CDistributionCollection_Declared
