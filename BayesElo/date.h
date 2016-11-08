// Copyright (c) 2015 MIT License by 6.172 Staff

////////////////////////////////////////////////////////////////////////////
//
// date.h
//
// CDate class definition
//
// Remi Coulom
//
// June 1996
//
////////////////////////////////////////////////////////////////////////////
#ifndef DATE_H
#define DATE_H

#include <iostream>  // NOLINT(readability/streams)

class CDate;

std::ostream &operator<<(std::ostream &ostr, const CDate &date);

class CDate {  // date
 private:  //////////////////////////////////////////////////////////////////
  int Day;
  int Month;
  int Year;

 public:  ///////////////////////////////////////////////////////////////////
  CDate() : Day(0), Month(0), Year(0) {}
  CDate(int y, int m, int d) : Day(d), Month(m), Year(y) {}
  CDate(const char *psz);

  void SetDay(int NewDay) {Day = NewDay;}
  void SetMonth(int NewMonth) {Month = NewMonth;}
  void SetYear(int NewYear) {Year = NewYear;}

  int GetDay() const {return Day;}
  int GetMonth() const {return Month;}
  int GetYear() const {return Year;}

  static CDate Today();
};

#endif  // DATE_H
