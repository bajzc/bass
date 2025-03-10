// Tail Recursive Elimination
#include <ostream>

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

struct TC : PassInfoMixin<TC> {
private:


    // from LLVM implementation, true if there is dynamic allocation
    /// Scan the specified function for alloca instructions.
    /// If it contains any dynamic allocas, returns false.
    static bool canTRE(Function &F) {
        // TODO: We don't do TRE if dynamic allocas are used.
        // Dynamic allocas allocate stack space which should be
        // deallocated before new iteration started. That is
        // currently not implemented.
        return llvm::all_of(instructions(F), [](Instruction &I) {
          auto *AI = dyn_cast<AllocaInst>(&I);
          return !AI || AI->isStaticAlloca();
        });
    }

    void Tail_Recur_Elim(Function& F) {
        if (F.getFunctionType()->isVarArg())
            return;
        if (canTRE(F))
            return;

        // scan for tail recursive call
        SmallVector<CallInst*, 4> recuCallInsts;
        for (auto& BB : F) {
            auto lastInst = BB.getTerminator();
            if (isa<ReturnInst>(lastInst)) {
                // call ..
                // ret
                auto oneBeforeLast = lastInst->getPrevNode();
                if (isa<CallInst>(oneBeforeLast)) {
                    recuCallInsts.push_back(dyn_cast<CallInst>(oneBeforeLast));
                }
            }
        }


        // transform all tail calls
        for (auto* CI : recuCallInsts) {
            BasicBlock* BB = CI->getParent();
        }

        /*         BasicBlock* entry_block = &F.getEntryBlock(); */
        /*         { */
        /*             BasicBlock* newEntry = BasicBlock::Create(F.getContext(), "TC_entry", &F, entry_block); */
        /*             IRBuilder<> builder(newEntry); */
        /*             builder.CreateBr(entry_block); */
        /*         } */
        /*         for (auto& BB : F) { */
        /*             if (!BB.getTerminator()) { */
        /*                 continue; // not a well-formed block */
        /*             } */
        /*             // TODO: last_instr can be a unconditional jump to return instr */
        /*             auto last_instr = dyn_cast<ReturnInst>(BB.getTerminator()); */

        /*             // return_call can be the return value or the call before return */
        /*             CallInst* return_call = nullptr; */
        /*             if (last_instr->getReturnValue()) */
        /*                 return_call = dyn_cast<CallInst>(last_instr->getReturnValue()); */
        /*             else if (last_instr->getPrevNode()) */
        /*                 return_call = dyn_cast<CallInst>(last_instr->getPrevNode()); */
        /*             if (!return_call) */
        /*                 continue; */

        /*             dbgs() << "Last is return: " << *last_instr << '\n'; */
        /*             dbgs() << "Call before returning: " << *return_call << '\n'; */
        /*             dbgs() << "function get called : " << return_call->getCalledFunction()->getName() << '\n'; */

        /*             // FIXME: pointer compare with `return_call->getCalledFunction()` doesn't work */
        /*             if (return_call->getCalledFunction()->getName() != F.getName()) { */
        /*                 dbgs() << "Not a recursive call\n"; */
        /*                 continue; */
        /*             } */

        /*             bool continueFlag = true; */
        /*             for (auto& arg : return_call->args()) { */
        /*                 if (!dyn_cast<Instruction>(&arg)) { */
        /*                     dbgs() << "Argument '" << *arg << "' of function call '" << *return_call */
        /*                            << "' cannot be cast to Instruction\n"; */
        /*                     continueFlag = false; */
        /*                 } */
        /*             } */
        /*             if (!continueFlag) */
        /*                 break; */
        /*             return_call->setTailCall(true); */
        /*             for (auto& arg : return_call->args()) { */
        /*                 dyn_cast<Instruction>(&arg)->insertBefore(return_call); */
        /*             } */
        /*             IRBuilder<> Builder(&BB); */
        /*             Builder.CreateBr(entry_block); */
        /*             last_instr->eraseFromParent(); */
        /*         } */
    }

public:
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        dbgs() << "Running on function: " << F.getName() << '\n';
        Tail_Recur_Elim(F);
        dbgs() << "\n";
        return PreservedAnalyses::none();
    }
};


extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "bajzc: Tail call elimination", "0.1", [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef name, FunctionPassManager& FPM, ArrayRef<PassBuilder::PipelineElement>) -> bool {
                        if (name == "b-tc") {
                            FPM.addPass(TC{});
                            return true;
                        }
                        return false;
                    });
            }};
}
