#include "printTableOrder.h"
#include "ir/ir.h"
#include "ir/visitor.h"
#include "frontends/p4/methodInstance.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <sstream>

namespace P4 {

void PrintTableOrder::printDependencies() const {
    for (const auto& entry : tableDependencies) {
        std::cout << "Table " << entry.first << " depends on: ";
        for (const auto& dep : entry.second) {
            std::cout << dep << " ";
        }
        std::cout << "\n";
    }
}

void PrintTableOrder::printInterferences() const {
    for (const auto& entry : tableInterferences) {
        std::cout << "Table " << entry.first << " interferes with: ";
        for (const auto& interference : entry.second) {
            std::cout << interference << " ";
        }
        std::cout << "\n";
    }
}

void PrintTableOrder::readInterferenceData(const std::string &filename) {
    std::ifstream infile(filename);
    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string word, table;
        iss >> word >> table >> word >> word;
        cstring tableName = cstring(table.c_str());
        while (iss >> word) {
            tableInterferences[tableName].insert(cstring(word.c_str()));
        }
    }
}

bool PrintTableOrder::preorder(const IR::P4Control *control) {
    if (control->name == "ACLIngress") {
        std::cout << "Ingress control tables order:\n";
        readInterferenceData("interf_data.txt");
        visit(control->body);
        printDependencies();
        printInterferences();
        transformDependencies();
    }
    return false;
}

bool PrintTableOrder::preorder(const IR::BlockStatement *block) {
    cstring previousTable;
    for (auto statement : block->components) {
        std::cout << "Statement: " << statement << "\n";
        visit(statement);
        if (!currentTable.isNullOrEmpty() && !previousTable.isNullOrEmpty() && currentTable != previousTable) {
            tableDependencies[currentTable].insert(previousTable);
        }
        previousTable = currentTable;
    }
    return false;
}

bool PrintTableOrder::preorder(const IR::IfStatement *ifStmt) {
    std::cout << "If statement with condition: " << ifStmt->condition << "\n";
    cstring outerTable = currentTable;
    visit(ifStmt->ifTrue);
    if (ifStmt->ifFalse) {
        std::cout << "Else statement\n";
        visit(ifStmt->ifFalse);
    }
    if (!currentTable.isNullOrEmpty() && !outerTable.isNullOrEmpty() && currentTable != outerTable) {
        std::cout << "Adding dependency from " << outerTable << " to " << currentTable << "\n";
        tableDependencies[currentTable].insert(outerTable);
    }
    return false;
}

bool PrintTableOrder::preorder(const IR::MethodCallStatement *mcs) {
    auto mi = P4::MethodInstance::resolve(mcs->methodCall, &refMap, &typeMap);
    if (mi->isApply()) {
        if (auto method = mcs->methodCall->method->to<IR::Member>()) {
            if (auto path = method->expr->to<IR::PathExpression>()) {
                std::cout << "Apply statement: " << path->path->name.name << "\n";
                if (!currentTable.isNullOrEmpty() && currentTable != path->path->name.name) {
                    tableDependencies[path->path->name.name].insert(currentTable);
                }
                currentTable = path->path->name.name;
            }
        }
    } else {
        std::cout << "Not an apply statement: " << mcs->methodCall << "\n";
    }
    return false;
}

void PrintTableOrder::transformDependencies() {
    for (const auto& entry : tableDependencies) {
        const cstring& tableA = entry.first;
        for (const auto& tableB : entry.second) {
            if (tableInterferences[tableA].find(tableB) == tableInterferences[tableA].end()) {
                std::cout << "Transforming: " << tableA << " depends on " << tableB << " but does not interfere\n";
            }
        }
    }
}

}