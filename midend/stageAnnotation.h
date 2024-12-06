#ifndef MIDEND_STAGE_ANNOTATION_H_
#define MIDEND_STAGE_ANNOTATION_H_

#include "ir/ir.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/methodInstance.h"
#include <vector>

namespace P4 {

class StageAnnotation : public Transform {
    int stage;
    const std::vector<const IR::P4Table *> &candidateTables;

public:
    StageAnnotation(int initialStage, const std::vector<const IR::P4Table *> &candidateTables)
        : stage(initialStage), candidateTables(candidateTables) {}

    ~StageAnnotation();

    const IR::Node *postorder(IR::P4Table *table) override;
    const IR::Node *apply(const IR::Node *node);
};

}

#endif