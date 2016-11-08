// Copyright (c) 2015 MIT License by 6.172 Staff

/////////////////////////////////////////////////////////////////////////////
//
// Rémi Coulom
//
// December, 2004
//
/////////////////////////////////////////////////////////////////////////////
#ifndef EloDataFromFile_Declared
#define EloDataFromFile_Declared

#include <vector>
#include <string>

class CPGNLex;
class CResultSet;

void EloDataFromFile(CPGNLex &pgnlex,
                     CResultSet &rs,
                     std::vector<std::string> &vNames);

#endif  // EloDataFromFile_Declared
