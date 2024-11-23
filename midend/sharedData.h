#ifndef MIDEND_SHAREDDATA_H_
#define MIDEND_SHAREDDATA_H_

#include <vector>
#include <string>
#include "ir/ir.h"

namespace P4 {
    // Simplistic shared data structure for Tcount information
    struct SharedData {
        int actionCount = 0;
        int tableCount = 0;
        std::vector<const IR::P4Table *> tables;
    };

    extern SharedData sharedData;

}

#endif