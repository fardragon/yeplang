
#ifndef YEPLANG_VALIDATOR_HPP
#define YEPLANG_VALIDATOR_HPP

#include "Function.hpp"
#include <unordered_map>
#include <vector>

namespace YepLang
{

	struct ValidatorScope
	{
		std::unordered_map<std::string, YepLang::Type> variables;
	};

	class Validator
	{
	public:
		Validator();
		void ValidateExternalFunction(const FunctionPrototype &prototype);
		void ValidateFunction(YepLang::Function &function);




	private:
		void ValidateExpression(YepLang::Expression &expression);
		void ValidateScope(YepLang::Expression &expression);
		void ValidateLiteral(YepLang::Expression &expression);
		void ValidateVariable(Expression &expression);
		void ValidateVariableAssignment(Expression &expression);
		void ValidateVariableDeclaration(Expression &expression);
		void ValidateReturn(Expression &expression);
		void ValidateConditional(Expression &expression);
		void ValidateArithmetic(Expression &expression);
		void ValidateComparison(Expression &expression);
		void ValidateLogical(Expression &expression);
		void ValidatePostInc(Expression &expression);
		void ValidatePointerDereference(Expression &expression);
		void ValidateArraySubscript(Expression &expression);
		void ValidateFunctionCall(Expression &expression);
		void ValidateMemberAccess(Expression &expression);


		std::optional<YepLang::Type> FindVariable(const std::string &name);
		std::optional<YepLang::FunctionPrototype> FindFunction(const std::string &name);
		[[noreturn]] void ValidatorError(std::string_view message, const YepLang::Expression &);

	private:
		FunctionPrototype _currentFunction;
		std::vector<ValidatorScope> _scopes;
		std::unordered_map<std::string, FunctionPrototype> _definedFunctions;


		void ValidateForLoop(Expression &expression);

		void ValidateNegate(Expression &expression);

		void ValidateAddressOf(Expression &expression);
	};



}

#endif //YEPLANG_VALIDATOR_HPP
