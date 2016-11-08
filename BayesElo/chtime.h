// Copyright (c) 2015 MIT License by 6.172 Staff

////////////////////////////////////////////////////////////////////////////
//
// chtime.h
//
// CTime class declaration
//
// Remi Coulom
//
// August 1996
//
////////////////////////////////////////////////////////////////////////////
#ifndef CTime_Declared
#define CTime_Declared

#include <cstdint>

class CTime {  // time
private:  //////////////////////////////////////////////////////////////////
  int64_t lTime;  // time in hundredths of a second

public:  ///////////////////////////////////////////////////////////////////
  //
  // Compatibility with the int64_t type
  //
  operator int64_t () const {return lTime;}
  CTime(int64_t l) {lTime = l;}
  CTime operator += (int64_t l) {return lTime += l;}
  CTime operator -= (int64_t l) {return lTime -= l;}
  CTime operator *= (int64_t l) {return lTime *= l;}
  CTime operator /= (int64_t l) {return lTime /= l;}

  //
  // Default constructor
  //
  CTime() {lTime = 0;}

  //
  // Gets for HH:MM:SS,HH
  //
  int64_t GetHours() const {return (lTime / 360000L);}
  int64_t GetMinutes() const {return (lTime / 6000) % 60;}
  int64_t GetSeconds() const {return (lTime / 100) % 60;}
  int64_t GetHundredths() const {return lTime % 100;}

  //
  // Sets
  //
  void Reset() {lTime = 0;}
  void Set(int64_t lHours,
           int64_t lMinutes,
           int64_t lSeconds,
           int64_t lHundredths) {
    lTime = lHundredths +
        lSeconds * 100 +
        lMinutes * 6000 +
        lHours * 360000L;
  }
  void Set(const char *psz);
};

#endif  // CTime_Declared
