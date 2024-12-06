#ifndef PRINTTABLEORDER_H
#define PRINTTABLEORDER_H

#include "ir/ir.h"
#include "ir/visitor.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/methodInstance.h"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

namespace P4 {

class PrintTableOrder : public Inspector {
    std::unordered_map<cstring, std::unordered_set<cstring>> tableDependencies;
    std::unordered_map<cstring, std::unordered_set<cstring>> tableInterferences;
    cstring currentTable;
    ReferenceMap &refMap;
    TypeMap &typeMap;
    std::ostream &outStream;
    std::vector<const IR::P4Table *> candidateTables;

public:
    PrintTableOrder(std::ostream &outStream, ReferenceMap &refMap, TypeMap &typeMap)
        : refMap(refMap), typeMap(typeMap), outStream(outStream) {}

    void printDependencies() const;
    void printInterferences() const;
    void readInterferenceData(const std::string &filename);
    void transformDependencies();
    const std::vector<const IR::P4Table *> &getCandidateTables() const;

    bool preorder(const IR::P4Control *control) override;
    bool preorder(const IR::BlockStatement *block) override;
    bool preorder(const IR::IfStatement *ifStmt) override;
    bool preorder(const IR::MethodCallStatement *mcs) override;
};

}

#endif