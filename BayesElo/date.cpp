// Copyright (c) 2015 MIT License by 6.172 Staff

////////////////////////////////////////////////////////////////////////////
//
// date.cpp
//
// CDate class
//
// Remi Coulom
//
// june 1996
//
////////////////////////////////////////////////////////////////////////////
#include "./date.h"

#include <ctime>
#include <iostream>  // NOLINT(readability/streams)
#include <sstream>
#include <iomanip>
#include <cstring>

using ::std::istringstream;
using ::std::ostream;
using ::std::dec;
using ::std::setw;

////////////////////////////////////////////////////////////////////////////
// Function to retrieve the current date
////////////////////////////////////////////////////////////////////////////
CDate CDate::Today() {
  time_t now;
  struct tm vtm;
  time(&now);
  struct tm *ptm = localtime_r(&now, &vtm);
  return CDate(ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);
}


////////////////////////////////////////////////////////////////////////////
// Constructor to convert a string to a date
// Format : YYYY?MM?DD (filled with zeroes)
////////////////////////////////////////////////////////////////////////////
CDate::CDate(const char *psz) {
  int Length = strlen(psz);

  Year = 0;
  Month = 0;
  Day = 0;

  char szBuffer[5];

  szBuffer[0] = psz[0];
  szBuffer[1] = psz[1];
  szBuffer[2] = psz[2];
  szBuffer[3] = psz[3];
  szBuffer[4] = 0;
  istringstream(szBuffer) >> dec >> Year;

  if (Length > 5) {
    szBuffer[0] = psz[5];
    szBuffer[1] = psz[6];
    szBuffer[2] = 0;
    istringstream(szBuffer) >> dec >> Month;

    if (Length > 8) {
      szBuffer[0] = psz[8];
      szBuffer[1] = psz[9];
      szBuffer[2] = 0;
      istringstream(szBuffer) >> dec >> Day;
    }
  }
}

////////////////////////////////////////////////////////////////////////////
// output operator
////////////////////////////////////////////////////////////////////////////
ostream &operator<<(ostream &ostr, const CDate &date) {
  char cOldFill = ostr.fill('0');

  if (date.GetYear())
    ostr << setw(4) << date.GetYear() << '.';
  else
    ostr << "??" << "??.";

  if (date.GetMonth())
    ostr << setw(2) << date.GetMonth() << '.';
  else
    ostr << "??.";

  if (date.GetDay())
    ostr << setw(2) << date.GetDay();
  else
    ostr << "??";

  ostr.fill(cOldFill);
  return ostr;
}
