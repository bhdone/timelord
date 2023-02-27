#include "test_utils.h"

#include <cstring>

bool IsFlag(char const* sz_argv, char const* flag_name)
{
    int p { 0 };
    int n = strlen(sz_argv);
    if (n == 0 || sz_argv[0] != '-') {
        return false;
    }
    for (; p < n; ++p) {
        if (sz_argv[p] != '-') {
            break;
        }
    }
    return strcmp(sz_argv + p, flag_name) == 0;
}

void ParseCommandLineParams(int argc, char* argv[], bool& verbose)
{
    verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (IsFlag(argv[i], "verbose")) {
            verbose = true;
            break;
        }
    }
}

