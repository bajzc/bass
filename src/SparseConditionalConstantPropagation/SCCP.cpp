// Sparse Conditional Constant Propagation
#include <ostream>

#include "llvm/IR/ConstantFolder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

struct SCCP : PassInfoMixin<SCCP> {
private:
    enum lattice_t {
        UNDEFINED, // Top
        CONSTANT,
        VARIABLE, // Bottom
    };

    struct ValueLattice {
        lattice_t Tag = UNDEFINED;
        std::optional<Constant*> ConstValue;


        ValueLattice() = default;

        explicit ValueLattice(lattice_t Tag) : Tag(Tag) {}

        explicit ValueLattice(Constant* ConstValue) : Tag(CONSTANT), ConstValue(ConstValue) {}
    };

    DenseMap<Value*, ValueLattice> ValueState;
    DenseMap<std::pair<BasicBlock*, BasicBlock*>, bool> ExecFlag;
    SetVector<BasicBlock*> BlockList;
    SetVector<Instruction*> WorkList;
    SetVector<BasicBlock*> Visited;

    bool SCCP_Evaluate(Instruction* I) {
        dbgs() << "Try to evaluate: " << *I << "\n";
        if (auto* PN = dyn_cast<PHINode>(I)) {
            Constant* Common = nullptr;
            for (Value* Incoming : PN->incoming_values()) {
                Constant* C = ValueState.lookup(Incoming).ConstValue.value_or(nullptr);
                if (isa<Constant>(Incoming))
                    C = dyn_cast<Constant>(Incoming);
                if (!C || (Common && C != Common)) {
                    Common = nullptr;
                    break;
                }
                Common = C;
            }
            if (Common) {
                dbgs() << "Common for PHI = " << *Common << "\n";
                ValueState[I] = ValueLattice(Common);
                return true;
            }
            return false;
        }

        SmallVector<Constant*, 4> ConstOperands;
        for (Value* Op : I->operands()) {
            if (Constant* C = ValueState.lookup(Op).ConstValue.value_or(nullptr)) {
                ConstOperands.push_back(C);
            } else if (isa<Constant>(Op)) {
                ConstOperands.push_back(dyn_cast<Constant>(Op));
            } else {
                return false;
            }
        }

        if (auto* BO = dyn_cast<BinaryOperator>(I)) {
            Constant* C = ConstantFoldBinaryInstruction(BO->getOpcode(), ConstOperands[0], ConstOperands[1]);
            if (C) {
                dbgs() << "C = " << *C << "\n";
                ValueState[I] = ValueLattice(C);
                return true;
            }
        }
        return false;
    }

    void SCCP_Block(BasicBlock* BB) {
        for (auto& I : BB->phis()) {
            SCCP_Instruction(&I);
        }
        if (!Visited.contains(BB)) {
            Visited.insert(BB);
            for (auto& I : *BB) {
                SCCP_Instruction(&I);
            }
        }
    }

    void SCCP_Instruction(Instruction* I) {
        if (isa<BinaryOperator>(I) || isa<PHINode>(I)) {
            if (SCCP_Evaluate(I)) {
                I->replaceAllUsesWith(ValueState[I].ConstValue.value());
                for (User* U : I->users()) {
                    if (auto* UserInst = dyn_cast<Instruction>(U)) {
                        dbgs() << "Use of " << *I << "is now" << *UserInst << "\n";
                        if (!is_contained(WorkList, UserInst)) {
                            WorkList.insert(UserInst);
                        }
                    }
                }
            }
        } else if (isa<BranchInst>(I)) {
            BasicBlock* BB = I->getParent();
            for (auto S : successors(BB)) {
                if (!ExecFlag[std::make_pair(BB, S)]) {
                    ExecFlag[std::make_pair(BB, S)] = true;
                    BlockList.insert(BB);
                    BlockList.insert(S);
                }
            }
        }
    }


    void SCCP_Initialize(Function& F) {
        for (auto& BB : F)
            for (auto& I : BB)
                if (!I.getType()->isVoidTy())
                    ValueState[&I] = ValueLattice(UNDEFINED);

        for (auto& arg : F.args()) {
            if (auto arg_constant = dyn_cast<Constant>(&arg)) // barely impossible
                ValueState[&arg] = ValueLattice(arg_constant);
            else
                ValueState[&arg] = ValueLattice(VARIABLE);
        }

        for (auto& BB : F) {
            for (auto Successor_BB : successors(&BB)) {
                ExecFlag[std::pair(&BB, Successor_BB)] = false;
            }
        }
        Visited.clear();
        WorkList.clear();
        BlockList = SetVector<BasicBlock*>();
        BlockList.insert(&F.getEntryBlock());
    }

public:
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        dbgs() << "Running SCCP on: " << F.getName() << '\n';
        SCCP_Initialize(F);
        while (!WorkList.empty() || !BlockList.empty()) {
            while (!WorkList.empty()) {
                auto* I = WorkList.pop_back_val();
                SCCP_Instruction(I);
            }
            while (!BlockList.empty()) {
                auto* B = BlockList.pop_back_val();
                SCCP_Block(B);
            }
        }

        for (auto& BB : F) {
            for (auto& I : make_early_inc_range(BB)) {
                if (ValueState[&I].Tag == CONSTANT) {
                    dbgs() << "Deleting constexpr: " << I << "\n";
                    I.eraseFromParent();
                }
            }
        }

        return PreservedAnalyses::none();
    }
};


extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "bajzc: Sparse Conditional Constant Propagation", "0.1",
            [](llvm::PassBuilder& PB) {
                PB.registerPipelineParsingCallback([](llvm::StringRef name, llvm::FunctionPassManager& FPM,
                                                      llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                    if (name == "b-sccp") {
                        FPM.addPass(SCCP{});
                        return true;
                    }
                    return false;
                });
            }};
}
