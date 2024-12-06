#include "stageAnnotation.h"

namespace P4 {

StageAnnotation::~StageAnnotation() {}

const IR::Node *StageAnnotation::postorder(IR::P4Table *table) {
    auto stageAnnotation = new IR::Annotation(IR::ID("stage"), new IR::Constant(stage));
    auto annotationsClone = table->annotations.clone();
    annotationsClone->push_back(stageAnnotation);
    table->annotations = *annotationsClone;

    std::cout << "Adding @stage(" << stage << ") annotation to table " << table->name << std::endl;

    return table;
}

const IR::Node *StageAnnotation::apply(const IR::Node *node) {
    for (const auto *table : candidateTables) {
        visit(table);
    }
    return node;
}

} // namespace P4