#include "Parser.hpp"

#include <stdexcept>
#include <charconv>
#include <sstream>
#include <ranges>
#include <algorithm>
#include <deque>


YepLang::Parser::Parser()
		: _keywordParsers
				  {
						  {TokenType::Return,              std::bind(&Parser::ParseReturn, this)},
						  {TokenType::If,                  std::bind(&Parser::ParseConditional, this)},
						  {TokenType::For,                 std::bind(&Parser::ParseForLoop, this)},
						  {TokenType::Continue,            std::bind(&Parser::ParseContinue, this)},
						  {TokenType::Break,               std::bind(&Parser::ParseBreak, this)},
						  {TokenType::VariableDeclaration, std::bind(&Parser::ParseVariableDeclaration, this)},
						  {TokenType::IndentPlus,          std::bind(&Parser::ParseIndent, this)}
				  }, _types
		{
				{"i32",  YepLang::BuiltinType{YepLang::BuiltinTypeEnum::i32}},
				{"i64",  YepLang::BuiltinType{YepLang::BuiltinTypeEnum::i64}},
				{"u64",  YepLang::BuiltinType{YepLang::BuiltinTypeEnum::u64}},
				{"char", YepLang::BuiltinType{YepLang::BuiltinTypeEnum::character}},
				{"bool", YepLang::BuiltinType{YepLang::BuiltinTypeEnum::boolean}},
				{"void", YepLang::BuiltinType{YepLang::BuiltinTypeEnum::void_t}},
		}
{
}

std::vector<YepLang::Function> YepLang::Parser::Parse(const std::vector<Token> &tokens)
{
	_currentToken = tokens.begin();
	_endToken = tokens.end();
	std::vector<YepLang::Function> result{};

	while (true)
	{
		if (_currentToken->type == YepLang::TokenType::Function)
		{
			Advance();
			result.push_back(this->ParseFunction());
		}
		else if (_currentToken->type == TokenType::Struct)
		{
			auto [name, type] = this->ParseStruct();
			_types.emplace(name, YepLang::Struct(type));
		}
		else
		{
			ExpectTokenType(TokenType::EndOfFile);
			break;
		}
	}

	return result;
}

YepLang::Function YepLang::Parser::ParseFunction()
{
	FunctionPrototype prototype{};

	prototype.name = ExpectTokenType(TokenType::Identifier).value.value();
	prototype.arguments = this->ParseFunctionArguments();

	ExpectTokenType(TokenType::RightArrow);

	prototype.returnType = this->ParseType();

	_declaredFunctions.emplace(prototype.name);

	ExpectTokenType(TokenType::Colon);
	ExpectTokenType(TokenType::EndOfLine);

	auto body = this->ParseScope();

	return {
			prototype,
			std::move(body)
	};
}

std::vector<YepLang::FunctionArgument> YepLang::Parser::ParseFunctionArguments()
{
	std::vector<YepLang::FunctionArgument> result;
	ExpectTokenType(TokenType::LeftParen);

	while (_currentToken->type != YepLang::TokenType::RightParen)
	{
		result.push_back(ParseFunctionArgument());
		if (_currentToken->type == YepLang::TokenType::Comma)
		{
			Advance();
		}
		else if (_currentToken->type != TokenType::RightParen)
		{
			ParserError(*_currentToken, "Unexpected token, ')' expected");
		}
	}

	ExpectTokenType(TokenType::RightParen);
	return result;
}

YepLang::Type YepLang::Parser::ParseType()
{
	const auto type = ExpectTokenType(YepLang::TokenType::Identifier).value.value();

	YepLang::Type returnType;


	if (auto it = _types.find(type); it != _types.end())
	{
		returnType = it->second;
	}
	else
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}

	while (_currentToken->type == TokenType::Asterisk || _currentToken->type == TokenType::LeftBracket)
	{
		switch (_currentToken->type)
		{
			case TokenType::Asterisk:
			{
				ExpectTokenType(TokenType::Asterisk);
				returnType = YepLang::Pointer{std::make_shared<YepLang::Type>(returnType)};
				break;
			}
			case TokenType::LeftBracket:
			{
				ExpectTokenType(TokenType::LeftBracket);
				if (_currentToken->type != TokenType::I64Literal)
				{
					throw std::invalid_argument(__PRETTY_FUNCTION__);
				}
				auto size = ParseIntegerLiteral();
				ExpectTokenType(TokenType::RightBracket);
				returnType = YepLang::Array{std::make_shared<YepLang::Type>(returnType),
				                            static_cast<std::uint64_t>(std::get<std::int64_t>(size.value))};
				break;
			}
			default:
				throw std::invalid_argument(__PRETTY_FUNCTION__);
		}

	}

	return returnType;
}

YepLang::Expression YepLang::Parser::ParseScope()
{
	Expression result{ExpressionKind::SCOPE, {}, ExpressionChildren{}};
	auto &body = std::get<ExpressionChildren>(result.value);

	ExpectTokenType(YepLang::TokenType::IndentPlus);

	while (_currentToken->type != YepLang::TokenType::IndentMinus)
	{
		if (_currentToken->type == TokenType::EndOfLine)
		{
			Advance();
		}
		else if (_keywordParsers.contains(_currentToken->type))
		{
			body.push_back(_keywordParsers[_currentToken->type]());
		}
		else
		{
			auto expression = this->ParseExpression();
			body.push_back(std::move(expression));
		}
	}

	ExpectTokenType(YepLang::TokenType::IndentMinus);
	return result;
}

YepLang::Expression YepLang::Parser::ParseReturn()
{
	Advance();
	ExpressionChildren children;
	if (_currentToken->type != TokenType::EndOfLine)
	{
		children = {this->ParseExpression()};
	}
	return {
			ExpressionKind::RETURN,
			std::nullopt,
			std::move(children)
	};
}

YepLang::Expression YepLang::Parser::ParseConditional()
{
	Advance();
	auto conditionalExpression = this->ParseExpression();
	ExpectTokenType(TokenType::Colon);
	ExpectTokenType(TokenType::EndOfLine);

	auto trueBody = this->ParseScope();

	ExpressionChildren children{std::move(conditionalExpression), std::move(trueBody)};

	while (_currentToken->type == TokenType::Elif)
	{
		Advance();
		auto elifCondition = this->ParseExpression();
		children.push_back(std::move(elifCondition));
		ExpectTokenType(TokenType::Colon);
		ExpectTokenType(TokenType::EndOfLine);

		auto elifBody = this->ParseScope();
		children.push_back(std::move(elifBody));
	}

	if (_currentToken->type == TokenType::Else)
	{
		Advance();
		ExpectTokenType(TokenType::Colon);
		ExpectTokenType(TokenType::EndOfLine);

		auto elseBody = this->ParseScope();
		children.push_back(std::move(elseBody));
	}

	return {
			ExpressionKind::CONDITIONAL,
			std::nullopt,
			std::move(children)
	};
}

YepLang::Expression YepLang::Parser::ParseForLoop()
{
	Advance();
	auto initBlock =
			_currentToken->type == TokenType::VariableDeclaration ? ParseVariableDeclaration() : ParseExpression();
	ExpectTokenType(TokenType::Comma);
	auto conditionBlock = this->ParseExpression();
	ExpectTokenType(TokenType::Comma);
	auto postBlock = this->ParseExpression();
	ExpectTokenType(TokenType::Colon);
	ExpectTokenType(TokenType::EndOfLine);
	auto forBody = ParseScope();
	return {
			ExpressionKind::FOR_LOOP,
			std::nullopt,
			ExpressionChildren{
					std::move(initBlock),
					std::move(conditionBlock),
					std::move(postBlock),
					std::move(forBody),
			}
	};
}

YepLang::Expression YepLang::Parser::ParseVariableDeclaration()
{
	Advance();
	auto identifier = ExpectTokenType(TokenType::Identifier);
	ExpectTokenType(TokenType::Colon);
	auto type = ParseType();
	ExpectTokenType(TokenType::Assignment);
	auto expression = this->ParseExpression();

	Expression variableExpression = {
			ExpressionKind::VARIABLE,
			type,
			identifier.value.value()
	};
	return {
			ExpressionKind::VARIABLE_DECLARATION,
			std::nullopt,
			ExpressionChildren{std::move(variableExpression), std::move(expression)}
	};
}

YepLang::Expression YepLang::Parser::ParseContinue()
{
	Advance();
	return {
			ExpressionKind::CONTINUE,
			{},
			{}
	};
}

YepLang::Expression YepLang::Parser::ParseBreak()
{
	Advance();
	return {
			ExpressionKind::BREAK,
			{},
			{}
	};
}

YepLang::Expression YepLang::Parser::ParseIndent()
{
	ExpressionChildren children{this->ParseScope()};
	return {
			ExpressionKind::SCOPE,
			children.front().type,
			std::move(children)
	};
}

YepLang::Expression YepLang::Parser::ParseExpression()
{
	auto LHS = ParseUnary();
	return ParseExpressionRHS(std::move(LHS), 0);
}

YepLang::Expression YepLang::Parser::ParseExpressionRHS(YepLang::Expression &&LHS, int currentPrecedence)
{
	while (true)
	{
		const auto tokenPrecedence = GetBinaryOperatorPrecedence();
		if (tokenPrecedence < currentPrecedence)
		{
			return LHS;
		}

		const auto op = _currentToken->type;
		Advance();

		auto RHS = ParseUnary();

		const auto nextTokenPrecedence = GetBinaryOperatorPrecedence();
		if (tokenPrecedence < nextTokenPrecedence)
		{
			RHS = ParseExpressionRHS(std::move(RHS), tokenPrecedence + 1);
		}

		switch (op)
		{
			case TokenType::Asterisk:
			case TokenType::DivideOp:
			case TokenType::Minus:
			case TokenType::PlusOp:
				LHS = BuildArithmeticExpression(op, std::move(LHS), std::move(RHS));
				break;
			case TokenType::LeftChevron:
			case TokenType::RightChevron:
			case TokenType::Equal:
			case TokenType::NotEqual:
				LHS = BuildComparisonExpression(op, std::move(LHS), std::move(RHS));
				break;
			case TokenType::LogicalAnd:
			case TokenType::LogicalOr:
				LHS = BuildLogicalExpression(op, std::move(LHS), std::move(RHS));
				break;
			case TokenType::Assignment:
				LHS = BuildAssignmentExpression(std::move(LHS), std::move(RHS));
				break;
			default:
				throw std::invalid_argument(__PRETTY_FUNCTION__);
		}

	}
}

YepLang::Expression YepLang::Parser::ParsePrimary()
{
	if (_currentToken->type == TokenType::I64Literal)
	{
		return ParseIntegerLiteral();
	}
	else if (_currentToken->type == TokenType::U64Literal)
	{
		return ParseUnsignedIntegerLiteral();
	}
	else if (_currentToken->type == TokenType::LeftParen)
	{
		return ParseParenExpression();
	}
	else if (_currentToken->type == TokenType::Identifier)
	{
		return ParseIdentifier();
	}
	else if (_currentToken->type == TokenType::CharacterLiteral)
	{
		return ParseCharacterLiteral();
	}
	else if (_currentToken->type == TokenType::StringLiteral)
	{
		return ParseStringLiteral();
	}
	else if (_currentToken->type == TokenType::LeftBracket)
	{
		return ParseArrayLiteral();
	}
	else if (_currentToken->type == TokenType::LeftBrace)
	{
		return ParseStructLiteral();
	}
	else
	{
		ParserError(*_currentToken, "Invalid primary expression");
	}
}

YepLang::FunctionArgument YepLang::Parser::ParseFunctionArgument()
{
	auto argName = ExpectTokenType(TokenType::Identifier).value.value();

	ExpectTokenType(TokenType::Colon);
	auto argType = ParseType();
	return {
			std::move(argName),
			argType
	};
}

const YepLang::Token &YepLang::Parser::ExpectTokenType(const YepLang::TokenType &type)
{
	if (_currentToken->type == type)
	{
		const auto &result = *_currentToken;
		Advance();
		return result;
	}
	else
	{
		//todo token to str
		std::ostringstream oss;
		oss << _currentToken->file << ":" << _currentToken->line << " Unexpected token: expected '"
		    << static_cast<int>(type) << "', but got '" << static_cast<int>(_currentToken->type) << "'\r\n";
		throw std::runtime_error(oss.str());
	}
}

void YepLang::Parser::ParserError(const Token &identifier, const std::string &message)
{
	std::ostringstream oss;
	oss << identifier.file << ":" << identifier.line << " " << message;
	if (identifier.value.has_value())
	{
		oss << ": " << identifier.value.value();
	}
	throw std::runtime_error(oss.str());
}

void YepLang::Parser::Advance()
{
	if (_currentToken == _endToken)
	{
		throw std::invalid_argument("Parser ran out of tokens");
	}
	std::advance(_currentToken, 1);
}

YepLang::Expression YepLang::Parser::ParseIntegerLiteral()
{
	auto &tokenValue = _currentToken->value.value();
	std::int64_t result{};
	auto convResult = std::from_chars(tokenValue.data(), tokenValue.data() + tokenValue.size(), result);
	if (convResult.ec != std::errc())
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}

	BuiltinType type;
	if (_currentToken->type == TokenType::I64Literal)
	{
		type = YepLang::BuiltinType{BuiltinTypeEnum::i64};
	}
	else
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
	Advance();

	return {
			ExpressionKind::LITERAL,
			type,
			result
	};
}

YepLang::Expression YepLang::Parser::ParseUnsignedIntegerLiteral()
{
	auto &tokenValue = _currentToken->value.value();
	std::uint64_t result{};
	auto convResult = std::from_chars(tokenValue.data(), tokenValue.data() + tokenValue.size(), result);
	if (convResult.ec != std::errc())
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}

	BuiltinType type;
	if (_currentToken->type == TokenType::U64Literal)
	{
		type = YepLang::BuiltinType{BuiltinTypeEnum::u64};
	}
	else
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
	Advance();


	return {
			ExpressionKind::LITERAL,
			type,
			result
	};
}

YepLang::Expression YepLang::Parser::ParseParenExpression()
{
	Advance();
	auto expression = this->ParseExpression();
	if (_currentToken->type != TokenType::RightParen)
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
	Advance();
	return expression;
}

YepLang::Expression YepLang::Parser::ParseCharacterLiteral()
{
	const auto &value = _currentToken->value.value();
	Advance();
	return {
			ExpressionKind::LITERAL,
			YepLang::BuiltinType{YepLang::BuiltinTypeEnum::character},
			value.front()
	};
}

YepLang::Expression YepLang::Parser::ParseStringLiteral()
{
	const auto &value = _currentToken->value.value();
	Advance();
	return {
			ExpressionKind::LITERAL,
			YepLang::Pointer{
					std::make_shared<YepLang::Type>(YepLang::BuiltinType{YepLang::BuiltinTypeEnum::character})},
			value
	};
}

YepLang::Expression YepLang::Parser::ParseIdentifier()
{
	const auto identifier = _currentToken->value.value();
	if (identifier == "true" || identifier == "false")
	{
		auto value = (identifier == "true");
		Advance();
		return {
				ExpressionKind::LITERAL,
				YepLang::BuiltinType{YepLang::BuiltinTypeEnum::boolean},
				value
		};
	}
	else
	{
		if (_declaredFunctions.contains(identifier))
		{
			return ParseCallExpression(identifier);
		}
		else
		{
			Expression variable{ExpressionKind::VARIABLE, std::nullopt, identifier};
			Advance();
			return variable;
		}
	}
}

YepLang::Expression YepLang::Parser::ParseCallExpression(const std::string &callee)
{
	Advance();

	ExpressionChildren params;
	params.push_back(
			Expression{
					ExpressionKind::CALLEE,
					std::nullopt,
					callee
			}
	);
	ExpectTokenType(TokenType::LeftParen);
	if (_currentToken->type != TokenType::RightParen)
	{
		while (true)
		{
			auto argExpression = this->ParseExpression();
			params.push_back(std::move(argExpression));

			if (_currentToken->type == TokenType::Comma)
			{
				Advance();
			}
			else
			{
				break;
			}
		}
	}
	ExpectTokenType(TokenType::RightParen);

	return {
			ExpressionKind::FUNCTION_CALL,
			std::nullopt,
			std::move(params)
	};
}

int YepLang::Parser::GetBinaryOperatorPrecedence()
{
	switch (_currentToken->type)
	{
		case TokenType::Asterisk:
		case TokenType::DivideOp:
			return 120;
		case TokenType::PlusOp:
		case TokenType::Minus:
			return 110;
		case TokenType::LeftChevron:
		case TokenType::RightChevron:
			return 90;
		case TokenType::Equal:
		case TokenType::NotEqual:
			return 80;
		case TokenType::LogicalAnd:
			return 40;
		case TokenType::LogicalOr:
			return 30;
		case TokenType::Assignment:
			return 10;
		default:
			return -1;
	}
}

bool YepLang::Parser::IsUnaryPrefix()
{
	switch (_currentToken->type)
	{
		case TokenType::Asterisk:
		case TokenType::Minus:
		case TokenType::Ampersand:
			return true;
		default:
			return false;
	}
}

bool YepLang::Parser::IsUnarySuffix()
{
	switch (_currentToken->type)
	{
		case TokenType::PlusPlusOp:
		case TokenType::LeftBracket:
		case TokenType::Dot:
			return true;
		default:
			return false;
	}
}

YepLang::Expression
YepLang::Parser::BuildArithmeticExpression(YepLang::TokenType op, YepLang::Expression &&LHS, YepLang::Expression &&RHS)
{
	switch (op)
	{
		case TokenType::Asterisk:
			return {
					ExpressionKind::MULTIPLY_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		case TokenType::Minus:
			return {
					ExpressionKind::MINUS_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		case TokenType::PlusOp:
			return {
					ExpressionKind::PLUS_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		case TokenType::DivideOp:
			return {
					ExpressionKind::DIVIDE_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		default:
			throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
}

YepLang::Expression
YepLang::Parser::BuildComparisonExpression(YepLang::TokenType op, YepLang::Expression &&LHS, YepLang::Expression &&RHS)
{
	switch (op)
	{
		case TokenType::LeftChevron:
			return {
					ExpressionKind::LESS_THAN_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		case TokenType::RightChevron:
			return {
					ExpressionKind::GREATER_THAN_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		case TokenType::Equal:
			return {
					ExpressionKind::EQUAL_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		case TokenType::NotEqual:
			return {
					ExpressionKind::NOT_EQUAL_OP,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		default:
			throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
}

YepLang::Expression
YepLang::Parser::BuildAssignmentExpression(YepLang::Expression &&LHS, YepLang::Expression &&RHS)
{
	return {
			ExpressionKind::VARIABLE_ASSIGNMENT,
			std::nullopt,
			ExpressionChildren{std::move(LHS), std::move(RHS)}
	};
}

YepLang::Expression
YepLang::Parser::BuildLogicalExpression(YepLang::TokenType op, YepLang::Expression &&LHS, YepLang::Expression &&RHS)
{
	switch (op)
	{
		case TokenType::LogicalAnd:
			return {
					ExpressionKind::LOGICAL_AND,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		case TokenType::LogicalOr:
			return {
					ExpressionKind::LOGICAL_OR,
					std::nullopt,
					ExpressionChildren{
							std::move(LHS), std::move(RHS)
					}
			};
		default:
			throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
}

YepLang::Expression YepLang::Parser::ParseUnary()
{
	std::vector<TokenType> unaryPrefixes;
	while (IsUnaryPrefix())
	{
		unaryPrefixes.push_back(_currentToken->type);
		Advance();
	}

	auto operand = ParsePrimary();

	std::deque<std::variant<TokenType, Expression>> unarySuffixes;
	while (IsUnarySuffix())
	{
		unarySuffixes.emplace_back(_currentToken->type);
		if (_currentToken->type == TokenType::LeftBracket)
		{
			Advance();
			unarySuffixes.emplace_back(this->ParseExpression());
			if (_currentToken->type != TokenType::RightBracket)
			{
				throw std::invalid_argument(__PRETTY_FUNCTION__);
			}
			unarySuffixes.emplace_back(_currentToken->type);
			Advance();
		}
		else if (_currentToken->type == TokenType::Dot)
		{
			Advance();
			unarySuffixes.emplace_back(this->ParseUnary());
		}
		else
		{
			Advance();
		}

	}

	while (!unarySuffixes.empty())
	{
		const auto &opVar = unarySuffixes.front();
		const auto &op = std::get<TokenType>(opVar);

		switch (op)
		{
			case TokenType::PlusPlusOp:
			{
				unarySuffixes.pop_front();
				operand = {
						ExpressionKind::POST_INC_OP,
						std::nullopt,
						ExpressionChildren{std::move(operand)}
				};
				break;
			}
			case TokenType::LeftBracket:
			{
				unarySuffixes.pop_front();
				if (!std::holds_alternative<Expression>(unarySuffixes.front()))
				{
					throw std::invalid_argument(__PRETTY_FUNCTION__);
				}
				auto indexExpression = std::get<Expression>(unarySuffixes.front());
				unarySuffixes.pop_front();

				if (!std::holds_alternative<TokenType>(unarySuffixes.front()))
				{
					throw std::invalid_argument(__PRETTY_FUNCTION__);
				}
				const auto &rightBracket = std::get<TokenType>(unarySuffixes.front());
				if (rightBracket != TokenType::RightBracket)
				{
					throw std::invalid_argument(__PRETTY_FUNCTION__);
				}
				unarySuffixes.pop_front();

				operand = {
						ExpressionKind::ARRAY_SUBSCRIPT,
						std::nullopt,
						ExpressionChildren{std::move(operand), std::move(indexExpression)}
				};
				break;
			}
			case TokenType::Dot:
			{
				unarySuffixes.pop_front();
				if (!std::holds_alternative<Expression>(unarySuffixes.front()))
				{
					throw std::invalid_argument(__PRETTY_FUNCTION__);
				}
				auto fieldName = std::get<Expression>(unarySuffixes.front());
				unarySuffixes.pop_front();

				operand = {
						ExpressionKind::MEMBER_ACCESS,
						std::nullopt,
						ExpressionChildren{std::move(operand), std::move(fieldName)}
				};
				break;
			}


			default:
				throw std::invalid_argument(__PRETTY_FUNCTION__);

		}
	}

	std::ranges::reverse(unaryPrefixes);
	for (const auto &op: unaryPrefixes)
	{
		if (op == TokenType::Asterisk)
		{
			operand = {
					ExpressionKind::POINTER_DEREFERENCE,
					std::nullopt,
					ExpressionChildren{std::move(operand)}
			};
		}
		else if (op == TokenType::Minus)
		{
			operand = {
					ExpressionKind::NEGATE,
					std::nullopt,
					ExpressionChildren{std::move(operand)}
			};
		}
		else if (op == TokenType::Ampersand)
		{
			operand = {
					ExpressionKind::ADDRESS_OF,
					std::nullopt,
					ExpressionChildren{std::move(operand)}
			};
		}
		else
		{
			throw std::invalid_argument(__PRETTY_FUNCTION__);
		}
	}


	return operand;
}

YepLang::Expression YepLang::Parser::ParseArrayLiteral()
{
	YepLang::ExpressionChildren elements{};

	do
	{
		Advance();
		elements.push_back(ParseExpression());
	} while (_currentToken->type == TokenType::Comma);

	ExpectTokenType(TokenType::RightBracket);

	return {
			ExpressionKind::LITERAL,
			YepLang::Array{std::make_shared<YepLang::Type>(elements.front().type.value()), elements.size()},
			std::move(elements)
	};
}

std::pair<std::string, YepLang::Struct> YepLang::Parser::ParseStruct()
{
	ExpectTokenType(TokenType::Struct);


	if (_currentToken->type != TokenType::Identifier)
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
	auto name = _currentToken->value.value();
	Advance();

	ExpectTokenType(TokenType::Colon);
	ExpectTokenType(TokenType::EndOfLine);
	ExpectTokenType(TokenType::IndentPlus);

	YepLang::Struct s;
	while (_currentToken->type != TokenType::IndentMinus)
	{
		if (_currentToken->type != TokenType::Identifier)
		{
			throw std::invalid_argument(__PRETTY_FUNCTION__);
		}
		auto memberName = _currentToken->value.value();
		Advance();

		ExpectTokenType(TokenType::Colon);
		auto memberType = this->ParseType();
		ExpectTokenType(TokenType::EndOfLine);
		s.fields.push_back(StructField{std::move(memberName), std::make_shared<YepLang::Type>(memberType)});
	}
	ExpectTokenType(TokenType::IndentMinus);
	return {std::move(name), std::move(s)};
}

YepLang::Expression YepLang::Parser::ParseStructLiteral()
{
	YepLang::ExpressionChildren elements{};
	std::vector<YepLang::StructField> fields;

	do
	{
		Advance();
		elements.push_back(ParseExpression());
		fields.push_back(YepLang::StructField{"", std::make_shared<YepLang::Type>(elements.back().type.value())});
	} while (_currentToken->type == TokenType::Comma);

	ExpectTokenType(TokenType::RightBrace);

	return {
			ExpressionKind::LITERAL,
			YepLang::Struct{std::move(fields)},
			std::move(elements)
	};
}
















