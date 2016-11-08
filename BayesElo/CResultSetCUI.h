// Copyright (c) 2015 MIT License by 6.172 Staff

////////////////////////////////////////////////////////////////////////////
//
// CResultSetCUI.h
//
// Remi Coulom
//
// December, 2004
//
////////////////////////////////////////////////////////////////////////////
#ifndef CResultSetCUI_Declared
#define CResultSetCUI_Declared

#include <vector>
#include <string>

#include "./consolui.h"  // CConsoleUI

class CResultSet;

class CResultSetCUI : public CConsoleUI {  // rscui
private:  //////////////////////////////////////////////////////////////////
  static const char * const tszCommands[];

  CResultSet &rs;
  std::vector<std::string> &vecName;

  unsigned ComputePlayerWidth() const;

protected:  ////////////////////////////////////////////////////////////////
  virtual int ProcessCommand(const char *pszCommand,
                             const char *pszParameters,
                             std::istream &in,
                             std::ostream &out);

  virtual void PrintLocalPrompt(std::ostream &out);

public:  ///////////////////////////////////////////////////////////////////
  CResultSetCUI(CResultSet &rsInit,
                std::vector<std::string> &vecNameInit,
                CConsoleUI *pcui = 0,
                int openmode = OpenModal);
};

#endif  // CResultSetCUI_Declared
