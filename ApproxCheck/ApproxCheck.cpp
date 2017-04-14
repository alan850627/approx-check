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
		std::vector<Instruction*> worklist;
		std::vector<Instruction*> addrList;
		std::map<std::string, std::pair<int, int>> opCounter; // <Opcode <total count, allow approx count>>
		
		/*
		* mark this instruction is non-approximate-able
		*/
		void markInstruction(Instruction* vi) {
			LLVMContext& C = vi->getContext();
			MDNode* N = MDNode::get(C, MDString::get(C, "no"));
			vi->setMetadata("approx", N);
		};
		
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
		};
		
		/*
		* find and returns the instruction in the use-def chain that corresponds to the
		* address of a load or store instruction.
		*/
		Instruction* findAddressDependency(Instruction* vi) {
			std::string opcode = vi->getOpcodeName();
			if (opcode == "load") {
				User::op_iterator defI = vi->op_begin();
				Instruction *parentVi = dyn_cast<Instruction>(*defI);
				return parentVi;
			}
			else if (opcode == "store") {
				User::op_iterator defI = vi->op_begin();
				defI++;
				if (isa<Instruction>(*defI)) {
					Instruction *parentVi = dyn_cast<Instruction>(*defI);
					return parentVi;
				}
			}
		};
		
		/*
		* A recursive function that looks for use-chains recursively.
		* vector history is used for checking if the use-def chain loops forever.
		*/
		void checkUseChain(Instruction* instr, int level, std::vector<Instruction*> history) {
			std::string opcode = instr->getOpcodeName();
			bool skipFirst = (opcode == "store");

			for (User::op_iterator i = instr->op_begin(); i != instr->op_end(); i++) {
				if (skipFirst) {
					skipFirst = false;
				} else if (isa<Instruction>(*i)) {
					Instruction *vi = dyn_cast<Instruction>(*i);
					markInstruction(vi);

					// Print
					for (int j = 0; j < level; j++) { errs() << "\t"; }
					errs() << "(" << level << ")" << *vi << "\n";

					std::string newopcode = vi->getOpcodeName();
					if (newopcode != "load" && !vectorContains(history, vi)) {
						history.push_back(vi);
						ApproxCheck::checkUseChain(vi, level + 1, history);
					}
					
					if (newopcode == "load") {
						// Found some "address" stored in memory. 
						Instruction* evalAddrInst = findAddressDependency(vi);
						if(!vectorContains(addrList, evalAddrInst)) {
							addrList.push_back(evalAddrInst);
						}
					}
				}
			}
		};
		
		/*
		* compares the instruction to the list. If the instruction has all the same
		* operands as any of the addrList elements' operands, then return true.
		*/
		bool isInAddrList(Instruction* I) {
			for (std::vector<Instruction*>::iterator it = addrList.begin(); it < addrList.end(); it++) {
				Instruction* exactPt = *it;
				if (hasSameOperands(exactPt, I)) {
					return true;
				}
			}
			return false;
		};
		
		/*
		* Returns true if the the use of that instruction is used as data of
		* a store instruction.
		*/
		bool useAsData(Instruction* instr, bool loadFlag, int level) {
			bool asData = false;
			for (Value::user_iterator useI = instr->user_begin(); useI != instr->user_end(); useI++) {
				bool foundload = loadFlag;
				Instruction *vi = dyn_cast<Instruction>(*useI);

				for (int j = 0; j < level; j++) { errs() << "\t"; }
				errs() << "(" << level << ")" << *vi << "\n";

				std::string opcode = vi->getOpcodeName();
				if (loadFlag) {
					if (opcode == "store") {
						User::op_iterator defI = vi->op_begin();
						if (isa<Instruction>(*defI)) {
							Instruction *parentVi = dyn_cast<Instruction>(*defI);
							if (parentVi->isIdenticalTo(instr)) {
								Instruction* addressVi = findAddressDependency(vi);
								// if this addressVi is in the addrList, then we're using
								// pointer as data. Therefore everything here should not
								// be approximated.

								if (isInAddrList(addressVi)) {
									asData = true;
									errs() << "HITT ";
								}
							}
						}
					}
				}
				else {
					if (opcode == "load") {
						foundload = true;
					}
				}
				asData = useAsData(vi, foundload, level + 1) || asData;
				if (asData) {
					markInstruction(vi);
				}
			}
			return asData;
		}

		/*
		* Counts how many instructions are marked in the function F
		*/
		void countOpcodes(Function &F) {
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
		}
		
		/*
		* The actual function pass being run. It calls the functions above.
		*/
		virtual bool runOnFunction(Function &F) {
			errs() << "\n===================" << "Function " << F.getName() << "===================\n\n";

			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
				worklist.push_back(&*I);
			}
			
			// Step 1) Find all places where an address is being used.
			// Step 2) If the address is stored in memory, locate the addresses that point to those memory locations.
			for(std::vector<Instruction*>::iterator i = worklist.begin(); i != worklist.end(); i++) {
				Instruction* instr = *i;
				std::string opcode = instr->getOpcodeName();
				if (instr->mayReadOrWriteMemory() || opcode == "br" || opcode == "ret") {
					errs() << "(0)" << *instr << "\n";
					std::vector<Instruction*> history;
					checkUseChain(instr, 1, history);
				}				
			}
			
			// Step 3) Find all places where the address is being operated on.
			for (std::vector<Instruction*>::iterator i = addrList.begin(); i < addrList.end(); i++) {
				Instruction* inst = *i;
				errs() << "(0)" << *inst << "\n";
				useAsData(inst, false, 1);
			}
			
			countOpcodes(F);

			// Print approx counts
			std::map <std::string, std::pair<int, int>>::iterator i = opCounter.begin();
			while (i != opCounter.end()) {
				errs() << i->first << ": " << i->second.second << "/" << i->second.first << " can be approximated\n";
				i++;
			}
			errs() << "\n";
			
			
			worklist.clear();
			opCounter.clear();
			addrList.clear();
			return false;
		};
		
	};
}

char ApproxCheck::ID = 0;
static RegisterPass<ApproxCheck> X("ApproxCheck", "Looks for dependencies in functions");