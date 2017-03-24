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
		std::map<std::string, std::pair<int, int>> opCounter; // <Opcode <total count, allow approx count>>
		std::vector<Instruction*> allocaList;
		std::vector<Instruction*> exactPtList;

		/*
		* Find all alloca opcodes and store those instructions in
		* allocaList.
		*/
		void findAlloca() {
			for (std::vector<Instruction*>::iterator iter = worklist.begin(); iter != worklist.end(); iter++) {
				Instruction* instr = *iter;
				std::string opcode = instr->getOpcodeName();
				if (opcode == "alloca") {
					allocaList.push_back(instr);
				}
			}
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
		* check if the two instructions have the exact same operands
		*/
		bool hasSameOperands(Instruction* exactPt, Instruction* I) {
			if (exactPt->getOpcode() != I->getOpcode() || exactPt->getNumOperands() != I->getNumOperands() || exactPt->getType() != I->getType()) {
				return false;
			}
			// If both instructions have no operands, they are identical.
			if (exactPt->getNumOperands() == 0 && I->getNumOperands() == 0) {
				return true;
			}

			// We have two instructions of identical opcode and #operands.  Check to see
			// if all operands are the same.
			if (!std::equal(exactPt->op_begin(), exactPt->op_end(), I->op_begin())) {
				return false;
			}
			return true;
		}

		/*
		* compares the instruction to the list. If the instruction has all the same
		* operands as any of the exactptlist elements' operands, then return true.
		*/
		bool isInExactPtList(Instruction* I) {
			for (std::vector<Instruction*>::iterator it = exactPtList.begin(); it < exactPtList.end(); it++) {
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
								// if this addressVi is in the exactPtList, then we're using 
								// pointer as data. Therefore everything here should not
								// be approximated. 

								if (isInExactPtList(addressVi)) {
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
			}
			return asData;
		}

		/*
		* Returns true if the the use of that instruction is used as an address of
		* a load or store instruction.
		*/
		bool useAsAddress(Instruction* instr, bool loadFlag, int level) {
			bool asAddress = false;
			for (Value::user_iterator useI = instr->user_begin(); useI != instr->user_end(); useI++) {
				bool foundload = loadFlag;
				Instruction *vi = dyn_cast<Instruction>(*useI);

				std::string opcode = vi->getOpcodeName();
				if (loadFlag) {
					if (opcode == "load") {
						Instruction *parentVi = findAddressDependency(vi);
						if (parentVi->isIdenticalTo(instr)) {
							asAddress = true;
						}
					}
					else if (opcode == "store") {
						Instruction *parentVi = findAddressDependency(vi);
						if (parentVi->isIdenticalTo(instr)) {
							asAddress = true;
						}
					}
					else if (opcode == "call") {
						for (User::op_iterator i = vi->op_begin(), e = vi->op_end(); i != e; ++i) {
							if (isa<Instruction>(*i)) {
								Instruction *parentVi = dyn_cast<Instruction>(*i);
								if (parentVi->isIdenticalTo(instr)) {
									asAddress = true;
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
				asAddress = useAsAddress(vi, foundload, level + 1) || asAddress;

				// find the crucial load.
				if (foundload && !loadFlag && opcode == "load" && asAddress) {
					Instruction* ptI = findAddressDependency(vi);
					if (!vectorContains(exactPtList, ptI)) {
						exactPtList.push_back(ptI);
					}
				}
			}
			return asAddress;
		};

		/*
		* mark this instruction is non-approximate-able
		*/
		void markInstruction(Instruction* instr) {

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
		* A recursive function that looks for use-chains recursively.
		* vector history is used for checking if the use-def chain loops forever.
		*/
		void checkUseChain(Instruction* instr, int level, std::vector<Instruction*> history) {

		};

		/*
		* A recursive function that looks for def-use chains.
		* vector history is used for checking if the use-def chain loops forever
		*/
		void checkDefChain(Instruction* instr, int level, std::vector<Instruction*> history) {

		};

		/*
		* The actual function pass being run. It calls the functions above.
		*/
		virtual bool runOnFunction(Function &F) {
			errs() << "\n===================" << "Function " << F.getName() << "===================\n\n";

			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
				worklist.push_back(&*I);
			}

			ApproxCheck::findAlloca();
			for (std::vector<Instruction*>::iterator it = allocaList.begin(); it < allocaList.end(); it++) {
				Instruction* inst = *it;
				useAsAddress(inst, false, 1);
			}
			
			for (std::vector<Instruction*>::iterator it = allocaList.begin(); it < allocaList.end(); it++) {
				Instruction* inst = *it;
				useAsData(inst, false, 1);
			}

			worklist.clear();
			opCounter.clear();
			allocaList.clear();
			exactPtList.clear();
			return false;
		};
	};
}

char ApproxCheck::ID = 0;
static RegisterPass<ApproxCheck> X("ApproxCheck", "Looks for dependencies in functions");
