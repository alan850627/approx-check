#define DEBUG_TYPE "ApproxCheck"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include <vector>
#include <map>
#include <utility>
#include <string>
using namespace llvm;

namespace {
  struct ApproxCheck : public FunctionPass {
    static char ID;
    ApproxCheck() : FunctionPass(ID) {}

    std::vector<std::string> specialI = {"store"};
    bool isSpecialInstruction (Instruction* instr) {
      std::string op = instr->getOpcodeName();
      for(std::vector<std::string>::iterator it = specialI.begin(); it < specialI.end(); it++) {
        if (op == *it) {
          return true;
        }
      }
      return false;
    }

    // <Opcode <total count, allow approx count>>
    std::map<std::string, std::pair<int, int>> opCounter;

    // Function for recursive searches for usechain.
    void checkUseChain(Instruction* instr, int level, bool skipFirst) {
      for (User::op_iterator i = instr -> op_begin(), e = instr -> op_end(); i != e; ++i) {
        Value *v = *i;
        if (isa<Instruction>(*i)) {
          if (skipFirst) {
            skipFirst = false;
          } else {
            Instruction *vi = dyn_cast<Instruction>(*i);
            // Set Metadata
            LLVMContext& C = vi->getContext();
            MDNode* N = MDNode::get(C, MDString::get(C, "no"));
            vi->setMetadata("approx", N);

            for (int j = 0; j < level; j++) {
              errs() << "\t";
            }
            errs() << "(" << level << ")" << *vi << "\n";

            if (!(vi->mayReadFromMemory())) {
              ApproxCheck::checkUseChain(vi, level + 1, isSpecialInstruction(vi));
            }
          }
        }
      }
    };

    virtual bool runOnFunction(Function &F) {
      errs() << "\n===================" << "Function " << F.getName() << "===================\n\n";

  		std::vector<Instruction*> worklist;
  		for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
        worklist.push_back(&*I);
  		}

  		// use-def chain for Instruction
  		for(std::vector<Instruction*>::iterator iter = worklist.begin(); iter != worklist.end(); ++iter){
  			Instruction* instr = *iter;

        // Identify and store instructions that may read or write to memory.
        // These the operands of these instructions cannot be approximated.
        if (instr->mayReadOrWriteMemory()) {
    			errs() << "(0)" <<*instr << "\n";
          ApproxCheck::checkUseChain(instr, 1, isSpecialInstruction(instr));
        }
  		}

      errs() << "\n";

      // Count Opcodes
      for (Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb) {
        for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
          if(opCounter.find(i->getOpcodeName()) == opCounter.end()) {
            opCounter[i->getOpcodeName()].first = 1;
            opCounter[i->getOpcodeName()].second = 0;
          } else {
            opCounter[i->getOpcodeName()].first += 1;
          }

          StringRef s;
          MDNode* mdn = i->getMetadata("approx");

          if (mdn) {
            s = cast<MDString>(mdn->getOperand(0))->getString();
          } else {
            s = "";
          }
          if(!s.equals("no")) {
            opCounter[i->getOpcodeName()].second += 1;
          }
        }
      }

      // Print approx counts
      std::map <std::string, std::pair<int, int>>::iterator i = opCounter.begin();
      std::map <std::string, std::pair<int, int>>::iterator e = opCounter.end();
      while (i != e) {
        errs() << i->first << ": " << i->second.second << "/" << i->second.first << " can be approximated\n";
        i++;
      }
      errs() << "\n";
      opCounter.clear();

      // def-use chain for Instruction
      // for(std::vector<Instruction*>::iterator iter = worklist.begin(); iter != worklist.end(); ++iter){
      // 	Instruction* instr = *iter;
      // 	errs() << "def: " <<*instr << "\n";
      // 	for(Value::use_iterator i = instr->use_begin(), ie = instr->use_end(); i!=ie; ++i){
      // 		Value *v = *i;
      // 		Instruction *vi = dyn_cast<Instruction>(*i);
      // 		errs() << "\t\t" << *vi << "\n";
      // 	}
      // }
      //
      // errs() << "\n\n";

  		return false;
  	}
  };
}

// char ApproxCheck::ID = 0;
// INITIALIZE_PASS(ApproxCheck, "ApproxCheck", "Looks for dependencies in functions", false, false)

// FunctionPass *llvm::ApproxCheckPass() {
//   return new ApproxCheck();
// }

char ApproxCheck::ID = 0;
static RegisterPass<ApproxCheck> X("ApproxCheck", "Looks for dependencies in functions");
