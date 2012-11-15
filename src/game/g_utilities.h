#ifndef UTILITIES_H
#define UTILITIES_H

#include <vector>
#include <string>

using std::vector;
using std::string;

vector<string> GetArgs();
std::string G_SHA1(const std::string& str);
string int2string(int value);
bool string2int(const string& s, int& i);
void LogPrint(string msg);
void LogPrintln(string msg);

#endif