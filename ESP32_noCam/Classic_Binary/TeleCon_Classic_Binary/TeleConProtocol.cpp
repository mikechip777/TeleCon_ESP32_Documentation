#include "TeleConProtocol.h"

bool parseLine(const String& line, String& app, String& type) {
  int colon = line.indexOf(':');
  if (colon <= 0) return false;

  app = line.substring(0, colon);
  String rest = line.substring(colon + 1);
  if (rest.length() == 0) return false;

  int comma = rest.indexOf(',');
  type = (comma < 0) ? rest : rest.substring(0, comma);
  return true;
}

String getValue(const String& line, const char* key) {
  int colon = line.indexOf(':');
  if (colon < 0) return "";

  int pos = line.indexOf(',', colon);
  if (pos < 0) return "";
  pos++;

  while (pos < (int)line.length()) {
    int keyEnd = line.indexOf(',', pos);
    if (keyEnd < 0) break;

    String k = line.substring(pos, keyEnd);

    int valEnd = line.indexOf(',', keyEnd + 1);
    String v = (valEnd < 0)
      ? line.substring(keyEnd + 1)
      : line.substring(keyEnd + 1, valEnd);

    if (k == key) return v;

    pos = (valEnd < 0) ? line.length() : valEnd + 1;
  }
  return "";
}