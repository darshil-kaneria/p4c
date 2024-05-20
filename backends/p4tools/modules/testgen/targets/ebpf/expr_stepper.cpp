#include "backends/p4tools/modules/testgen/targets/ebpf/expr_stepper.h"

#include <functional>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

#include "backends/p4tools/common/lib/taint.h"
#include "backends/p4tools/common/lib/variables.h"
#include "ir/irutils.h"
#include "ir/solver.h"
#include "lib/cstring.h"
#include "lib/error.h"
#include "lib/log.h"

#include "backends/p4tools/modules/testgen/core/externs.h"
#include "backends/p4tools/modules/testgen/core/program_info.h"
#include "backends/p4tools/modules/testgen/core/small_step/expr_stepper.h"
#include "backends/p4tools/modules/testgen/core/small_step/small_step.h"
#include "backends/p4tools/modules/testgen/lib/continuation.h"
#include "backends/p4tools/modules/testgen/lib/exceptions.h"
#include "backends/p4tools/modules/testgen/lib/execution_state.h"
#include "backends/p4tools/modules/testgen/targets/ebpf/table_stepper.h"

namespace P4Tools::P4Testgen::EBPF {

EBPFExprStepper::EBPFExprStepper(ExecutionState &state, AbstractSolver &solver,
                                 const ProgramInfo &programInfo)
    : ExprStepper(state, solver, programInfo) {}

void EBPFExprStepper::evalExternMethodCall(const IR::MethodCallExpression *call,
                                           const IR::Expression *receiver, IR::ID name,
                                           const IR::Vector<IR::Argument> *args,
                                           ExecutionState &state) {
    // Provides implementations of eBPF externs.
    static const ExternMethodImpls EXTERN_METHOD_IMPLS({
        /* ======================================================================================
         *  CounterArray.add
         *  A counter array is a dense or sparse array of unsigned 32-bit values, visible to the
         *  control-plane as an EBPF map (array or hash).
         *  Each counter is addressed by a 32-bit index.
         *  Counters can only be incremented by the data-plane, but they can be read or
         *  reset by the control-plane.
         *
         *  CounterArray.add: Add value to counter with specified index.
         * ====================================================================================== */
        // TODO: Count currently has no effect in the symbolic interpreter.
        {"CounterArray.add",
         {"index", "value"},
         [](const IR::MethodCallExpression * /*call*/, const IR::Expression * /*receiver*/,
            IR::ID & /*methodName*/, const IR::Vector<IR::Argument> * /*args*/,
            const ExecutionState &state, SmallStepEvaluator::Result &result) {
             ::warning("CounterArray.add not fully implemented.");
             auto &nextState = state.clone();
             nextState.popBody();
             result->emplace_back(nextState);
         }},
        /* ======================================================================================
         *  CounterArray.increment
         *  A counter array is a dense or sparse array of unsigned 32-bit values, visible to the
         *  control-plane as an EBPF map (array or hash).
         *  Each counter is addressed by a 32-bit index.
         *  Counters can only be incremented by the data-plane, but they can be read or
         *  reset by the control-plane.
         *
         *  CounterArray.increment: Add value to counter with specified index.
         * ====================================================================================== */
        // TODO: Count currently has no effect in the symbolic interpreter.
        {"CounterArray.increment",
         {"index"},
         [](const IR::MethodCallExpression * /*call*/, const IR::Expression * /*receiver*/,
            IR::ID & /*methodName*/, const IR::Vector<IR::Argument> * /*args*/,
            const ExecutionState &state, SmallStepEvaluator::Result &result) {
             ::warning("CounterArray.increment not fully implemented.");
             auto &nextState = state.clone();
             nextState.popBody();
             result->emplace_back(nextState);
         }},
        /* ======================================================================================
         *  verify_ipv4_checksum
         *  This is a user-defined function.
         *  This function implements method to verify IP checksum.
         *  @param iphdr Structure representing IP header. The IP header is generated by the P4
         *  compiler and defined in test.h.
         *  @return True if checksum is correct.
         * Implemented in p4c/testdata/extern_modules/extern-checksum-ebpf.c
         * ====================================================================================== */
        {"*method.verify_ipv4_checksum",
         {"iphdr"},
         [](const IR::MethodCallExpression * /*call*/, const IR::Expression * /*receiver*/,
            IR::ID & /*methodName*/, const IR::Vector<IR::Argument> *args,
            const ExecutionState &state, SmallStepEvaluator::Result &result) {
             const auto *ipHdrRef = args->at(0)->expression;
             if (!(ipHdrRef->is<IR::Member>() || ipHdrRef->is<IR::PathExpression>())) {
                 TESTGEN_UNIMPLEMENTED("IP header input %1% of type %2% not supported", ipHdrRef,
                                       ipHdrRef->type);
             }
             // Input must be an IPv4 header.
             ipHdrRef->type->checkedTo<IR::Type_Header>();

             const auto &validVar = state.get(ToolsVariables::getHeaderValidity(ipHdrRef));
             // Check whether the validity bit of the header is false.
             // If yes, do not bother evaluating the checksum.
             auto emitIsTainted = Taint::hasTaint(validVar);
             if (emitIsTainted || !validVar->checkedTo<IR::BoolLiteral>()->value) {
                 auto &nextState = state.clone();
                 nextState.replaceTopBody(Continuation::Return(IR::BoolLiteral::get(false)));
                 result->emplace_back(nextState);
                 return;
             }
             // Define a series of short-hand variables. These are hardcoded just like the extern.
             const auto *version = state.get(new IR::Member(ipHdrRef, "version"));
             const auto *ihl = state.get(new IR::Member(ipHdrRef, "ihl"));
             const auto *diffserv = state.get(new IR::Member(ipHdrRef, "diffserv"));
             const auto *totalLen = state.get(new IR::Member(ipHdrRef, "totalLen"));
             const auto *identification = state.get(new IR::Member(ipHdrRef, "identification"));
             const auto *flags = state.get(new IR::Member(ipHdrRef, "flags"));
             const auto *fragOffset = state.get(new IR::Member(ipHdrRef, "fragOffset"));
             const auto *ttl = state.get(new IR::Member(ipHdrRef, "ttl"));
             const auto *protocol = state.get(new IR::Member(ipHdrRef, "protocol"));
             const auto *hdrChecksum = state.get(new IR::Member(ipHdrRef, "hdrChecksum"));
             const auto *srcAddr = state.get(new IR::Member(ipHdrRef, "srcAddr"));
             const auto *dstAddr = state.get(new IR::Member(ipHdrRef, "dstAddr"));
             const auto *bt8 = IR::Type_Bits::get(8);
             const auto *bt16 = IR::Type_Bits::get(16);
             const auto *bt32 = IR::Type_Bits::get(32);

             // The checksum is computed as a series of 16-bit additions.
             // We need to widen to 32 bits to handle overflows.
             // These overflows are added to the checksum at the end.
             const IR::Expression *checksum = new IR::Cast(
                 bt32, new IR::Concat(bt16, new IR::Concat(bt8, version, ihl), diffserv));
             checksum = new IR::Add(bt32, checksum, new IR::Cast(bt32, totalLen));
             checksum = new IR::Add(bt32, checksum, new IR::Cast(bt32, identification));
             checksum = new IR::Add(bt32, checksum,
                                    new IR::Cast(bt32, new IR::Concat(bt16, flags, fragOffset)));
             checksum = new IR::Add(bt32, checksum,
                                    new IR::Cast(bt32, new IR::Concat(bt16, ttl, protocol)));
             checksum = new IR::Add(bt32, checksum, new IR::Cast(bt32, hdrChecksum));
             checksum =
                 new IR::Add(bt32, checksum, new IR::Cast(bt32, new IR::Slice(srcAddr, 31, 16)));
             checksum =
                 new IR::Add(bt32, checksum, new IR::Cast(bt32, new IR::Slice(srcAddr, 15, 0)));
             checksum =
                 new IR::Add(bt32, checksum, new IR::Cast(bt32, new IR::Slice(dstAddr, 31, 16)));
             checksum =
                 new IR::Add(bt32, checksum, new IR::Cast(bt32, new IR::Slice(dstAddr, 15, 0)));
             checksum =
                 new IR::Add(bt16, new IR::Slice(checksum, 31, 16), new IR::Slice(checksum, 15, 0));
             const auto *calcResult = new IR::Cmpl(bt16, checksum);
             const auto *comparison = new IR::Equ(calcResult, IR::Constant::get(bt16, 0));
             auto &nextState = state.clone();
             nextState.replaceTopBody(Continuation::Return(comparison));
             result->emplace_back(nextState);
         }},
        /* ======================================================================================
         *  tcp_conntrack
         *  This file implements sample C extern function. It contains definition of the following C
         *  extern function:
         *  bool tcp_conntrack(in Headers_t hdrs)
         *  The implementation shows how to use BPF maps in the C extern function to perform
         * stateful packet processing.
         * Implemented in p4c/testdata/extern_modules/extern-conntrack-ebpf.c
         * ====================================================================================== */
        {"*method.tcp_conntrack",
         {"hdrs"},
         [](const IR::MethodCallExpression * /*call*/, const IR::Expression * /*receiver*/,
            IR::ID & /*methodName*/, const IR::Vector<IR::Argument> *args,
            const ExecutionState &state, SmallStepEvaluator::Result &result) {
             // Input must be the headers struct.
             const auto *headers = args->at(0)->expression->checkedTo<IR::StructExpression>();
             const auto *tcpRef = headers->getField("tcp");
             CHECK_NULL(tcpRef);
             const auto *tcpHeader = tcpRef->expression->checkedTo<IR::HeaderExpression>();
             const auto *syn = tcpHeader->getField("syn");
             CHECK_NULL(syn);
             const auto *ack = tcpHeader->getField("ack");
             CHECK_NULL(ack);
             const auto *synExpr = syn->expression;
             const auto *ackExpr = ack->expression;

             // Implement the simple conntrack case since we do not support multiple packets here
             // yet.
             // TODO: We need custom test objects to implement richer, stateful testing here.
             auto &nextState = state.clone();
             const auto *cond =
                 new IR::LAnd(new IR::Equ(synExpr, IR::Constant::get(synExpr->type, 1)),
                              new IR::Equ(ackExpr, IR::Constant::get(ackExpr->type, 0)));
             nextState.replaceTopBody(Continuation::Return(cond));
             result->emplace_back(nextState);
         }},
    });

    if (!EXTERN_METHOD_IMPLS.exec(call, receiver, name, args, state, result)) {
        ExprStepper::evalExternMethodCall(call, receiver, name, args, state);
    }
}  // NOLINT

bool EBPFExprStepper::preorder(const IR::P4Table *table) {
    // Delegate to the tableStepper.
    EBPFTableStepper tableStepper(this, table);

    return tableStepper.eval();
}

}  // namespace P4Tools::P4Testgen::EBPF
