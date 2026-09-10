#pragma once

/*
 * Line protocol parsing — APP:TYPE,key1,val1,...
 */

#include <Arduino.h>

bool parseLine(const String& line, String& app, String& type);
String getValue(const String& line, const char* key);
