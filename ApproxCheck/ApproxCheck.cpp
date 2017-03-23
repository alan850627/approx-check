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
		std::vector<Instruction*> allocaList;
		std::map<std::string, std::pair<int, int>> opCounter; // <Opcode <total count, allow approx count>>


		/*
		* Checks whether the instruction vector contains the same instruction as the
		* parameter passed. This is used to check if the use-def chain loops forever.
		*/
		bool vectorContains(std::vector<Instruction*> v, Instruction* instr) {
			for (std::vector<Instruction*>::iterator it = v.begin(); it < v.end(); it++) {
				if ((*it)->isIdenticalTo(instr)) {
					return true;
				}
			}
			return false;
		}


		/*
		* A recursive function that looks for use-chains recursively.
		* vector history is used for checking of the use-def chain loops forever.
		*/
		void checkUseChain(Instruction* instr, int level, std::vector<Instruction*> history) {
			std::string opcode = instr->getOpcodeName();
			bool skipFirst = opcode == "store";
			if (opcode == "alloca" && level > 2 && !vectorContains(allocaList, instr)) {;
				allocaList.push_back(instr);
			}

			for (User::op_iterator i = instr->op_begin(), e = instr->op_end(); i != e; ++i) {
				Value *v = *i;
				if (skipFirst) {
					skipFirst = false;
				}
				else if (isa<Instruction>(*i)) {
					Instruction *vi = dyn_cast<Instruction>(*i);

					// Set Metadata
					LLVMContext& C = vi->getContext();
					MDNode* N = MDNode::get(C, MDString::get(C, "no"));
					vi->setMetadata("approx", N);

					// Print
					for (int j = 0; j < level; j++) { errs() << "\t"; }
					errs() << "(" << level << ")" << *vi << "\n";

					if (!vectorContains(history, vi)) {
						history.push_back(vi);
						ApproxCheck::checkUseChain(vi, level + 1, history);
					}
				}
			}
		};

		/*
		* The actual function pass being run. It calls the functions above.
		*/
		virtual bool runOnFunction(Function &F) {
			errs() << "\n===================" << "Function " << F.getName() << "===================\n\n";

			std::vector<Instruction*> worklist;
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
				worklist.push_back(&*I);
			}

			// Simple use-def chain for Instruction
      errs() << "===ROUND ONE===\n";
			for (std::vector<Instruction*>::iterator iter = worklist.begin(); iter != worklist.end(); ++iter) {
				Instruction* instr = *iter;
				// Identify and store instructions that may read or write to memory.
				// These the operands of these instructions cannot be approximated.
				if (instr->mayReadOrWriteMemory()) {
					errs() << "(0)" << *instr << "\n";
					std::vector<Instruction*> history;
					ApproxCheck::checkUseChain(instr, 1, history);
				}
			}
			errs() << "\n";

      // def-use chain for Instruction
      errs() << "===ROUND TWO===\n";
			for(std::vector<Instruction*>::iterator iter = allocaList.begin(); iter != allocaList.end(); ++iter){
				Instruction* instr = *iter;
				errs() << "def: " <<*instr << "\n";
				for(Value::user_iterator i = instr->user_begin(), ie = instr->user_end(); i!=ie; ++i){
					Value *v = *i;
					Instruction *vi = dyn_cast<Instruction>(*i);
					errs() << "\t\t" << *vi << "\n";
				}
			}


			// // Catching "load-store" dependencies
      // errs() << "===ROUND TWO===\n";
			// for (std::vector<Instruction*>::iterator iter = worklist.begin(); iter != worklist.end(); ++iter) {
			// 	Instruction* instr = *iter;
			// 	if (instr->mayReadOrWriteMemory()) {
      //
			// 		// Loop through first level use-def to find if we find the alloca.
			// 		bool hit = false;
			// 		for (User::op_iterator i = instr->op_begin(), e = instr->op_end(); i != e; ++i) {
			// 			Value *v = *i;
			// 			if (isa<Instruction>(*i)) {
			// 				Instruction *vi = dyn_cast<Instruction>(*i);
			// 				if (vectorContains(allocaList, vi)) {
			// 					hit = true;
			// 				}
			// 			}
			// 		}
      //
			// 		if (hit) {
			// 			// we found the alloca, we need to look at all the use-def chains thoroughly
			// 			errs() << "(0)" << *instr << "\n";
			// 			std::vector<Instruction*> history;
			// 			for (User::op_iterator i = instr->op_begin(), e = instr->op_end(); i != e; ++i) {
			// 				Value *v = *i;
			// 				if (isa<Instruction>(*i)) {
			// 					Instruction *vi = dyn_cast<Instruction>(*i);
      //
			// 					// Set Metadata
			// 					LLVMContext& C = vi->getContext();
			// 					MDNode* N = MDNode::get(C, MDString::get(C, "no"));
			// 					vi->setMetadata("approx", N);
      //
			// 					// Print
			// 					errs() << "\t(" << 1 << ")" << *vi << "\n";
      //
			// 					history.push_back(vi);
			// 					ApproxCheck::checkUseChain(vi, 2, history);
			// 				}
			// 			}
			// 		}
			// 	}
			// }
			errs() << "\n";

			// Count Opcodes
			for (Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb) {
				for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
					if (opCounter.find(i->getOpcodeName()) == opCounter.end()) {
						opCounter[i->getOpcodeName()].first = 1;
						opCounter[i->getOpcodeName()].second = 0;
					}
					else {
						opCounter[i->getOpcodeName()].first += 1;
					}
					StringRef s;
					MDNode* mdn = i->getMetadata("approx");
					if (mdn) {
						s = cast<MDString>(mdn->getOperand(0))->getString();
					}
					else {
						s = "";
					}
					if (!s.equals("no")) {
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
      allocaList.clear();

			return false;
		}
	};
}

char ApproxCheck::ID = 0;
static RegisterPass<ApproxCheck> X("ApproxCheck", "Looks for dependencies in functions");
