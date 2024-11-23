#ifndef MIDEND_ADD_TCOUNT_HEADER_H_
#define MIDEND_ADD_TCOUNT_HEADER_H_

#include "ir/visitor.h"
#include "ir/ir.h"
#include "midend/sharedData.h"

namespace P4 {

    class AddTcountHeader : public Transform {
    public:
        const IR::Node *preorder(IR::P4Program *program) override {
            auto fields = new IR::IndexedVector<IR::StructField>();
            for (int i = 0; i < sharedData.tableCount; ++i) {
                // Couldn't find a better name for a field
                cstring fieldName = "field" + std::to_string(i + 1);
                fields->push_back(new IR::StructField(fieldName, IR::Type_Bits::get(8)));
            }
            auto tcountHeader = new IR::Type_Header("tcount_t", *fields);

            bool inserted = false;
            for (auto it = program->objects.begin(); it != program->objects.end(); ++it) {
                if (auto headerType = (*it)->to<IR::Type_Header>()) {
                    program->objects.insert(it + 1, tcountHeader);
                    inserted = true;
                    break;
                }
            }

            // If no header type was found, add the new header type at the beginning
            if (!inserted) {
                program->objects.insert(program->objects.begin(), tcountHeader);
            }

            return program;
        }

        const IR::Node *postorder(IR::Type_Struct *headersStruct) override {
            if (headersStruct->name == "headers") {
                auto fields = headersStruct->fields;
                fields.push_back(new IR::StructField("tcount", new IR::Type_Name("tcount_t")));
                headersStruct->fields = fields;
            }
            return headersStruct;
        }

        const IR::Node *postorder(IR::P4Control *control) override {

            // The most inefficient way to find the deparser control block lol
            bool isDeparser = false;
            bool isIngress = false;

            if (control->body != nullptr) {
                for (const auto &component : control->body->components) {
                    if (auto methodCallStmt = component->to<IR::MethodCallStatement>()) {
                        if (auto methodCallExpr = methodCallStmt->methodCall) {
                            if (auto pathExpr = methodCallExpr->method->to<IR::Member>()) {
                                if (pathExpr->member == "emit" && pathExpr->expr->to<IR::PathExpression>()->path->name == "packet") {
                                    isDeparser = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // Find a better way to grab the ingress control block
            if (control->name == "MyIngress") {
                isIngress = true;
            }
            
            // std::cout << "isDeparser: " << isDeparser << std::endl;
            if (isDeparser) {
                auto body = control->body->components;
                auto emitStmt = new IR::MethodCallStatement(
                    new IR::MethodCallExpression(
                        new IR::PathExpression("packet.emit"),
                        new IR::Vector<IR::Type>({ new IR::Type_Name("tcount_t") }),
                        new IR::Vector<IR::Argument>({ new IR::Argument(new IR::Member(new IR::PathExpression("hdr"), "tcount")) })
                    )
                );
                body.insert(body.begin(), emitStmt);
                control->body = new IR::BlockStatement(body);
            }

            if (isIngress) {
                auto body = control->body->components;
                auto setValidStmt = new IR::MethodCallStatement(
                    new IR::MethodCallExpression(
                        new IR::Member(new IR::Member(new IR::PathExpression("hdr"), "tcount"), "setValid")
                    )
                );
                body.insert(body.begin(), setValidStmt);
                control->body = new IR::BlockStatement(body);
            }
            
            return control;
        }
    };

}

#endif