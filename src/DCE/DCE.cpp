#include <ostream>

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static bool isCritical(Instruction *I) {
    return I->mayHaveSideEffects() || I->mayWriteToMemory();
}

static void Mark(BasicBlock &BB, MapVector<Instruction *, bool> &mark) {
    SetVector<Instruction *> WorkList;
    for (auto &I: BB) {
        mark.erase(&I);
        if (isCritical(&I)) {
            mark[&I] = true;
            WorkList.insert(&I);
        } else {
            dbgs() << "Instr is not critical: " << I << '\n';
        }
    }
    while (!WorkList.empty()) {
        Instruction *I = WorkList.pop_back_val();
        for (auto &OpV: I->operands()) {
            if (auto *op_def = dyn_cast<Instruction>(OpV)) {
                mark[op_def] = true;
                WorkList.insert(op_def);
            }
        }
    }
    // get RDF here
}

static void Sweep(Function &F) {
}

struct DeadCodeElimination : llvm::PassInfoMixin<DeadCodeElimination> {
    llvm::PreservedAnalyses run(llvm::Function &F,
                                llvm::FunctionAnalysisManager &FAM) {
        auto mark = MapVector<Instruction *, bool>();
        for (auto &BB: F) {
            Mark(BB, mark);
        }
        return PreservedAnalyses::all();
    }
};


extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "Dead Code Elimination", "0.1", [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                    if (name == "b-dce") {
                        FPM.addPass(DeadCodeElimination{});
                        return true;
                    }
                    return false;
                });
        }
    };
}
