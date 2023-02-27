#ifndef TL_TEST_UTILS_H
#define TL_TEST_UTILS_H

bool IsFlag(char const* sz_argv, char const* flag_name);

void ParseCommandLineParams(int argc, char* argv[], bool& verbose);

#endif
