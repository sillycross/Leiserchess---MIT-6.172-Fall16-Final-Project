// Copyright (c) 2015 MIT License by 6.172 Staff

////////////////////////////////////////////////////////////////////////////
//
// readstr.h
//
// ReadString function
//
// Remi Coulom
//
// june, 1998
//
////////////////////////////////////////////////////////////////////////////
#ifndef READSTR_H
#define READSTR_H

#include <iostream>  // NOLINT(readability/streams)

int ReadString(std::istream *is, char *pszBuffer, int Size);

#endif  // READSTR_H
