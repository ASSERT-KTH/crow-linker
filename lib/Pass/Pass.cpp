
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/DemandedBits.h"
#include "llvm/Analysis/LazyValueInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"


using namespace llvm;

namespace {

    struct CROWMergePass : public ModulePass {

        static char ID;


        public:
            CROWMergePass() : ModulePass(ID) {
        
            }

            bool runOnModule(Module &M) {
                return true;
            }


    };
    char CROWMergePass::ID = 0;
}


namespace llvm {
    void initializeCROWMergePassPass(llvm::PassRegistry &);
}

INITIALIZE_PASS_BEGIN(CROWMergePass, "crowmerge", "Crow merge function variants",
                    false, false)

INITIALIZE_PASS_END(CROWMergePass, "crowmerge", "Crow merge function variants", false,
                    false)
static struct Register {
    Register() {
        initializeCROWMergePassPass(*llvm::PassRegistry::getPassRegistry());
    }
} X;

static void registerCROWMergePass(
        const llvm::PassManagerBuilder &Builder, llvm::legacy::PassManagerBase &PM) {
    PM.add(new CROWMergePass);
}

static llvm::RegisterStandardPasses
#ifdef DYNAMIC_PROFILE_ALL
RegisterSouperOptimizer(llvm::PassManagerBuilder::EP_OptimizerLast,
                        registerCROWMergePass);
#else
RegisterSouperOptimizer(llvm::PassManagerBuilder::EP_Peephole,
                        registerCROWMergePass);
#endif
