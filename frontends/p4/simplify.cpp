/*
Copyright 2013-present Barefoot Networks, Inc. 

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "simplify.h"

namespace P4 {

const IR::Node* SimplifyControlFlow::postorder(IR::BlockStatement* statement) {
    LOG1("Visiting " << statement);
    auto parent = getContext()->node;
    auto statancestor = findContext<IR::Statement>();
    if (parent->is<IR::SwitchCase>() ||
        parent->is<IR::P4Control>() ||
        parent->is<IR::Function>() ||
        parent->is<IR::P4Action>()) {
        // Cannot remove block from switch or toplevel control block
        return statement;
    }
    if ((statancestor && statancestor->is<IR::BlockStatement>()) ||
        findContext<IR::P4Action>() != nullptr) {  // directly within an action
        // if there are no local declarations we can remove this block
        bool hasDeclarations = false;
        for (auto c : *statement->components)
            if (!c->is<IR::Statement>()) {
                hasDeclarations = true;
                break;
            }
        if (!hasDeclarations)
            return statement->components;
    }
    if (statement->components->empty())
        return new IR::EmptyStatement(statement->srcInfo);
    if (statement->components->size() == 1) {
        auto first = statement->components->at(0);
        if (first->is<IR::Statement>())
            return first;
    }
    return statement;
}

const IR::Node* SimplifyControlFlow::postorder(IR::IfStatement* statement)  {
    if (SideEffects::check(statement->condition, refMap, typeMap))
        return statement;
    if (statement->ifTrue->is<IR::EmptyStatement>() &&
        (statement->ifFalse == nullptr || statement->ifFalse->is<IR::EmptyStatement>()))
        return new IR::EmptyStatement(statement->srcInfo);
    return statement;
}

const IR::Node* SimplifyControlFlow::postorder(IR::EmptyStatement* statement)  {
    auto parent = findContext<IR::Statement>();
    if (parent == nullptr ||  // in a ParserState or P4Action
        parent->is<IR::BlockStatement>())
        // remove it from blocks
        return nullptr;
    return statement;
}

const IR::Node* SimplifyControlFlow::postorder(IR::SwitchStatement* statement)  {
    if (statement->cases->empty()) {
        BUG_CHECK(statement->expression->is<IR::Member>(),
                  "%1%: expected a Member", statement->expression);
        auto expr = statement->expression->to<IR::Member>();
        BUG_CHECK(expr->expr->is<IR::MethodCallExpression>(),
                  "%1%: expected a table invocation", expr->expr);
        auto mce = expr->expr->to<IR::MethodCallExpression>();
        return new IR::MethodCallStatement(mce->srcInfo, mce);
    }
    auto last = statement->cases->back();
    if (last->statement == nullptr) {
        auto cases = new IR::Vector<IR::SwitchCase>();
        auto it = statement->cases->rbegin();
        for (; it != statement->cases->rend(); ++it) {
            if ((*it)->statement != nullptr)
                break;
            else
                ::warning("%1%: fallthrough with no statement", last);
        }
        for (auto i = statement->cases->begin(); i != it.base(); ++i)
            cases->push_back(*i);
        return new IR::SwitchStatement(statement->srcInfo, statement-> expression, cases);
    }
    return statement;
}

}  // namespace P4
