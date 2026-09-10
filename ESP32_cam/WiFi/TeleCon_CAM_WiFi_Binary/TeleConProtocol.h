#pragma once

#include <Arduino.h>

bool parseLine(const String& line, String& app, String& type);
String getValue(const String& line, const char* key);