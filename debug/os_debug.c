#include "os_print_external.h"
void OsDebugPanicSpin(char *filename, int line, const char *func,
                      const char *cond) {
  OsPrintStr("filename: ");
  OsPrintStr(filename);
  OsPrintStr("\n");
  OsPrintStr("line: 0x");
  OsPrintHex(line);
  OsPrintStr("\n");
  OsPrintStr("function: ");
  OsPrintStr((char *)func);
  OsPrintStr("\n");
  OsPrintStr("condition: ");
  OsPrintStr((char *)cond);
  OsPrintStr("\n");
  while (1) {
  }
}