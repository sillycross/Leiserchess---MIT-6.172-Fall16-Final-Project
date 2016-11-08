// Copyright (c) 2015 MIT License by 6.172 Staff

////////////////////////////////////////////////////////////////////////////
//
// chtime.cpp
//
// CTime class
//
// Remi Coulom
//
// August 1996
//
////////////////////////////////////////////////////////////////////////////
#include "./chtime.h"
#include <cstdint>
#include <sstream>

////////////////////////////////////////////////////////////////////////////
// Conversion from a string
// Format : HH:MM:SS,HH (filled with zeroes)
////////////////////////////////////////////////////////////////////////////
void CTime::Set(const char *psz) {
  uint64_t ulHours = 0;
  uint64_t ulMinutes = 0;
  uint64_t ulSeconds = 0;
  uint64_t ulHundredths = 0;

  char szBuffer[3];
  szBuffer[2] = 0;

  szBuffer[0] = psz[0];
  if (szBuffer[0])
    szBuffer[1] = psz[1];
  std::istringstream(szBuffer) >> std::dec >> ulHours;
  if (psz[0] && psz[1] && psz[2]) {
    szBuffer[0] = psz[3];
    if (szBuffer[0])
      szBuffer[1] = psz[4];
    std::istringstream(szBuffer) >> std::dec >> ulMinutes;
    if (psz[3] && psz[4] && psz[5]) {
      szBuffer[0] = psz[6];
      if (szBuffer[0])
        szBuffer[1] = psz[7];
      std::istringstream(szBuffer) >> std::dec >> ulSeconds;
      if (psz[6] && psz[7] && psz[8]) {
        szBuffer[0] = psz[9];
        if (szBuffer[0])
          szBuffer[1] = psz[10];
        std::istringstream(szBuffer) >> std::dec >> ulHundredths;
      }
    }
  }

  Set(ulHours, ulMinutes, ulSeconds, ulHundredths);
}
