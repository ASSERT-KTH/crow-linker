
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
#include "llvm/IR/Verifier.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <sys/stat.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>

using namespace llvm;

namespace {


    static cl::list<std::string> BCFiles("crow-merge-bitcodes",
                                                cl::desc("<variant file>")
                                                , cl::OneOrMore, cl::CommaSeparated);


    static cl::list<std::string> FunctionNames("crow-merge-functions",
                                         cl::desc("<function name>")
            , cl::OneOrMore, cl::CommaSeparated);

    static cl::opt<std::string> FuncSufix("crow-merge-func-sufix",
                                               cl::desc("Name sufix in the added functions")
            , cl::init("_"));

    static cl::opt<bool> SkipOnError(
            "crow-merge-skip-on-error",
            cl::desc("Skip the merge of a module if error"),
            cl::init(false));


    static cl::opt<unsigned> DebugLevel(
            "crow-merge-debug-level",
            cl::desc("Pass devbug level, 0 for none"),
            cl::init(0));

    struct CROWMergePass : public ModulePass {

        static char ID;
        unsigned funcCount;

        inline bool exists (const std::string& name) {
            struct stat buffer;
            return (stat (name.c_str(), &buffer) == 0);
        }

        void mergeFunction(Function &F, Module& toM){
            // first change the name of the function to prevent recursion troubles
            if (DebugLevel > 1)
                errs() << "Changing function name \n";
            F.setName(F.getName() + FuncSufix);

            // Now migrate every BB inside the function to the toM
            if (DebugLevel > 1)
                errs() << "Creating the new function \n";

            
            auto f = toM.getOrInsertFunction("tt", Type::getVoidTy(toM.getContext())).getCallee();

            Function* fNew = cast<Function>(f);

            BasicBlock* block = BasicBlock::Create(toM.getContext(), "entry", fNew);
            IRBuilder<> builder(block);

            builder.CreateRetVoid();

            if (DebugLevel > 1)
                errs() << "Finishing \n";
            //for(auto &BB: F){
            //    errs() << BB << "\n";
            //}
        }

        public:
            CROWMergePass() : ModulePass(ID) {
                funcCount = 0;
            }

            bool runOnModule(Module &M) {
                bool changed = false;
                errs() << "modules count " << BCFiles.size() << "\n";
                errs() << "function count " << FunctionNames.size() << "\n";


                for(auto module: BCFiles){

                    if (!exists(module)){
                        errs() << "Module " << module << " does not exist\n";

                        if(!SkipOnError){
                            llvm::report_fatal_error("Module " + module + " not found !");
                        }

                        continue; // continue since the module does not exist
                    }

                    for(auto fname : FunctionNames){


                        errs() << "Parsing " << module << "\n";

                        LLVMContext context;
                        SMDiagnostic error;
                        auto bitcode = parseIRFile(module, error, context);

                        auto fObject = bitcode->getFunction(fname);

                        if(!fObject){
                            errs() << "Function " << fname << " does not exist\n";

                            if(!SkipOnError){
                                llvm::report_fatal_error("Function " + fname + " not found !");
                            }

                            continue; // continue since the function does not exist
                        }

                        errs() << "Merging function " << fname << "\n";

                        mergeFunction(*fObject, M);
                        changed = true;


                        // Load module to be merged
                        
                        // Skip on module cannot be parsed if options is set

                        // Skip on function not found if option is set
                    }

                    funcCount++;
                }
                /*

                 for each function in FunctionNames

                    for each module:

                        load module

                        f = look for function in module
                        // check for option to report original in current module if exist
                        f.rename(f + "pr1")
                        report(f + "pr1")

                        add f to M
                        change_dependencies of f for current M ones, even globals

                */

                return changed;
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
