#ifndef MIDEND_COUNTACTIONSANDTABLES_H_
#define MIDEND_COUNTACTIONSANDTABLES_H_

#include "ir/visitor.h"
#include "midend/sharedData.h"

namespace P4 {

    class CountActionsAndTables : public Inspector {
    public:
        bool preorder(const IR::P4Action *action) override {
            sharedData.actionCount++;
            return true;
        }

        bool preorder(const IR::P4Table *table) override {
            sharedData.tableCount++;
            sharedData.tables.push_back(table);
            return true;
        }

        // Debug function
        void end_apply(const IR::Node *node) override {
            // std::cout << "Number of actions: " << sharedData.actionCount << std::endl;
            // std::cout << "Number of tables: " << sharedData.tableCount << std::endl;
        }
    };

}

#endif