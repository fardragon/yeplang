#ifndef YEPLANG_GENERATOR_HPP
#define YEPLANG_GENERATOR_HPP

#include "Function.hpp"

#include <memory>
#include <optional>
#include <map>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/NoFolder.h>

namespace YepLang
{
	struct GeneratorVariable
	{
		llvm::Type *type;
		llvm::Value *address;
	};

	struct GeneratorScope
	{
		std::map<std::string, GeneratorVariable> variables;
	};

	enum class GeneratorConstructType
	{
		ForLoop
	};

	struct GeneratorConstruct
	{
		struct ForConstruct
		{
			llvm::BasicBlock *changeBlock;
			llvm::BasicBlock *afterBlock;
		};

		GeneratorConstructType type;
		union
		{
			ForConstruct forConstruct;
		};
	};



	class Generator
	{
	public:
		Generator();
		void GenerateFunction(const Function &function);
		void GenerateExternFunction(const FunctionPrototype &function);

		void Dump() const;

	private:
		bool CodegenScope(const Expression &scope);

		llvm::Value *CodegenExpression(const Expression &expression);
		llvm::Value *CodegenLiteral(const Expression &expression);
		llvm::Value *CodegenVariable(const Expression &expression);
		llvm::Value *CodegenPostIncrement(const Expression &expression);
		llvm::Value *CodegenReturn(const Expression &expression);
		llvm::Value *CodegenBinaryArithmeticOp(const Expression &expression);
		llvm::Value *CodegenBinaryRelationalOp(const Expression &expression);
		llvm::Value *CodegenBinaryLogicalOp(const Expression &expression);
		llvm::Value *CodegenPointerDereference(const Expression &expression);
		llvm::Value *CodegenArraySubscript(const Expression &expression);
		llvm::Value *CodegenMemberAccess(const Expression &expression);
		void CodegenConditional(const Expression &expression);
		void CodegenForLoop(const Expression &expression);
		void CodegenContinue();
		void CodegenBreak();
		void CodegenVariableDeclaration(const Expression &expression);
		llvm::Value * CodegenFunctionCall(const Expression &expression);
		llvm::Value *CodegenAssignment(const Expression &expression);

		llvm::Type *CodegenType(const Type &type);
		llvm::AllocaInst *AllocaVariable(const std::string &name, llvm::Type *type);

		std::pair<std::uint64_t, std::string> NextLabel();
		std::optional<GeneratorVariable> FindGeneratorVariable(const std::string &variable);

	private:
		std::unique_ptr<llvm::LLVMContext> _context;
		std::unique_ptr<llvm::Module> _module;
		std::unique_ptr<llvm::IRBuilder<llvm::NoFolder>> _builder;

		llvm::Function *_currentFunction = nullptr;

		std::uint64_t _labelCounter = 0;
		std::vector<GeneratorScope> _scopes = {};
		std::vector<GeneratorConstruct> _constructs = {};

		llvm::Value *CodegenAddressOf(const Expression &expression);
	};
}


#endif //YEPLANG_GENERATOR_HPP
