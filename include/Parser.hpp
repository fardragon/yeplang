#ifndef YEPLANG_PARSER_HPP
#define YEPLANG_PARSER_HPP

#include <vector>
#include <map>
#include <unordered_set>
#include <functional>

#include "Function.hpp"
#include "Types.hpp"
#include "Token.hpp"
#include "Expression.hpp"

namespace YepLang
{
	class Parser
	{
	public:
		explicit Parser();
		std::vector<Function> Parse(const std::vector<Token> &tokens);

	private:
		Function ParseFunction();
		std::pair<std::string, YepLang::Struct> ParseStruct();
		std::vector<FunctionArgument> ParseFunctionArguments();
		YepLang::Type ParseType();
		Expression ParseScope();

		Expression ParseReturn();
		Expression ParseConditional();
		Expression ParseForLoop();
		Expression ParseContinue();
		Expression ParseBreak();
		Expression ParseVariableDeclaration();
		Expression ParseIndent();

		Expression ParseExpression();
		Expression ParseExpressionRHS(Expression &&LHS, int currentPrecedence);
		Expression ParseUnary();
		int GetBinaryOperatorPrecedence();
		bool IsUnaryPrefix();
		bool IsUnarySuffix();

		Expression ParsePrimary();
		Expression ParseIntegerLiteral();
		Expression ParseUnsignedIntegerLiteral();
		Expression ParseParenExpression();
		Expression ParseCharacterLiteral();
		Expression ParseStringLiteral();
		Expression ParseIdentifier();
		Expression ParseArrayLiteral();
		Expression ParseStructLiteral();
		YepLang::Expression ParseCallExpression(const std::string &callee);

		Expression BuildArithmeticExpression(YepLang::TokenType op, Expression &&LHS, Expression &&RHS);
		Expression BuildComparisonExpression(YepLang::TokenType op, Expression &&LHS, Expression &&RHS);
		Expression BuildLogicalExpression(YepLang::TokenType op, Expression &&LHS, Expression &&RHS);
		YepLang::Expression BuildAssignmentExpression(YepLang::Expression &&LHS, YepLang::Expression &&RHS);

		const Token &ExpectTokenType(const TokenType &type);

		[[noreturn]] void ParserError(const Token &identifier, const std::string &message);

		void Advance();

	private:
		std::unordered_map<YepLang::TokenType, std::function<YepLang::Expression()>> _keywordParsers;
		std::vector<Token>::const_iterator _currentToken;
		std::vector<Token>::const_iterator _endToken;
		std::unordered_set<std::string> _declaredFunctions;
		std::unordered_map<std::string, YepLang::Type> _types;


		FunctionArgument ParseFunctionArgument();
	};
}

#endif //YEPLANG_PARSER_HPP
