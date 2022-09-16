#ifndef YEPLANG_EXPRESSION_HPP
#define YEPLANG_EXPRESSION_HPP

#include <string>
#include <variant>
#include <optional>
#include <vector>
#include "Types.hpp"

namespace YepLang
{
	enum class ExpressionKind
	{
		LITERAL,
		VARIABLE,
		VARIABLE_DECLARATION,
		VARIABLE_ASSIGNMENT,
		RETURN,
		CONDITIONAL,
		PLUS_OP,
		MINUS_OP,
		DIVIDE_OP,
		MULTIPLY_OP,
		LESS_THAN_OP,
		GREATER_THAN_OP,
		EQUAL_OP,
		NOT_EQUAL_OP,
		LOGICAL_AND,
		LOGICAL_OR,
		POST_INC_OP,
		FOR_LOOP,
		CONTINUE,
		BREAK,
		SCOPE,
		FUNCTION_CALL,
		CALLEE,
		POINTER_DEREFERENCE,
		ARRAY_SUBSCRIPT,
		NEGATE,
		ADDRESS_OF,
		MEMBER_ACCESS
	};

	struct Expression;
	using ExpressionChildren = std::vector<Expression>;
	using ExpressionValue = std::variant<std::monostate, ExpressionChildren, std::int64_t, std::uint64_t, std::string, char, bool>;
	struct Expression
	{
		ExpressionKind kind;
		std::optional<YepLang::Type> type;
		ExpressionValue value;
	};

	inline bool KindIsIn(ExpressionKind in, ExpressionKind last)
	{
		return in == last;
	}

	template<typename... Ts>
	inline bool KindIsIn(ExpressionKind in, ExpressionKind first, Ts... rest)
	{
		return (in == first) || KindIsIn(in, rest...);
	}

	template <class T>
	bool ExpressionHolds(const Expression &expression)
	{
		return std::holds_alternative<T>(expression.value);
	}

	template <class T>
	T& ExpressionGet(Expression &expression)
	{
		return std::get<T>(expression.value);
	}

	std::string ExpressionKindToStr(const ExpressionKind &type);
	std::string ExpressionToString(const Expression &expression, const std::string& prefix = "", bool isLeft = false);
	std::string ExpressionValueToString(const ExpressionValue &value);
}


#endif //YEPLANG_EXPRESSION_HPP
