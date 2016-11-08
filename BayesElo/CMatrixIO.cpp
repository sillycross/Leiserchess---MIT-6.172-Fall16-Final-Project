// Copyright (c) 2015 MIT License by 6.172 Staff

/////////////////////////////////////////////////////////////////////////////
//
// Rémi Coulom
//
// February, 2005
//
/////////////////////////////////////////////////////////////////////////////
#include "./CMatrixIO.h"

#include <iostream>  // NOLINT(readability/streams)
#include <iomanip>

#include "./CMatrix.h"

/////////////////////////////////////////////////////////////////////////////
// Output operator for matrices
/////////////////////////////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &out, const CMatrix &m) {
  for (int i = 0; i < m.GetRows(); i++) {
    for (int j = 0; j < m.GetColumns(); j++)
      out << std::setw(12) << m.GetElement(i, j) << ' ';
    out << '\n';
  }

  return out;
}
