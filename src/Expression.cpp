#include "Expression.hpp"
#include <sstream>

std::string YepLang::ExpressionToString(const Expression &expression, const std::string &prefix, bool isLeft)
{
	std::ostringstream oss;
	oss << prefix;

	oss << (isLeft ? "├──" : "└──");

	// print the value of the node
	oss << ExpressionKindToStr(expression.kind);
	switch (expression.kind)
	{
		case ExpressionKind::LITERAL:
		{
			if (!expression.type.has_value())
			{
				throw std::invalid_argument(__PRETTY_FUNCTION__);
			}
			else
			{
				oss << ": " << ExpressionValueToString(expression.value) << " : " << TypeToStr(expression.type.value());
			}
			break;
		}
		case ExpressionKind::VARIABLE:
			oss << ": " << std::get<std::string>(expression.value) << " : " << TypeToStr(expression.type.value());
			break;
		case ExpressionKind::CALLEE:
			oss << ": " << std::get<std::string>(expression.value);
			break;

		default:
		{
			if (expression.type.has_value())
			{
				oss << " : " << TypeToStr(expression.type.value());
			}
		}
	}
	oss << "\r\n";

	if (std::holds_alternative<ExpressionChildren>(expression.value))
	{
		const auto &children = std::get<ExpressionChildren>(expression.value);
		if (!children.empty())
		{
			for (auto it = children.begin(); it != std::prev(children.end()); ++it)
			{
				oss << ExpressionToString(*it, prefix + (isLeft ? "│   " : "    "), true);
			}
			oss << ExpressionToString(children.back(), prefix + (isLeft ? "│   " : "    "), false);
		}
	}
	return oss.str();
}

std::string YepLang::ExpressionKindToStr(const YepLang::ExpressionKind &type)
{
	switch (type)
	{
		case ExpressionKind::LITERAL:
			return "literal";
		case ExpressionKind::VARIABLE:
			return "variable";
		case ExpressionKind::RETURN:
			return "return";
		case ExpressionKind::PLUS_OP:
			return "plus";
		case ExpressionKind::MINUS_OP:
			return "minus";
		case ExpressionKind::DIVIDE_OP:
			return "divide";
		case ExpressionKind::MULTIPLY_OP:
			return "star";
		case ExpressionKind::SCOPE:
			return "scope";
		case ExpressionKind::LESS_THAN_OP:
			return "less than";
		case ExpressionKind::GREATER_THAN_OP:
			return "greater than";
		case ExpressionKind::EQUAL_OP:
			return "equal";
		case ExpressionKind::NOT_EQUAL_OP:
			return "not equal";
		case ExpressionKind::CONDITIONAL:
			return "if";
		case ExpressionKind::VARIABLE_DECLARATION:
			return "var";
		case ExpressionKind::VARIABLE_ASSIGNMENT:
			return "assignment";
		case ExpressionKind::POST_INC_OP:
			return "post increment";
		case ExpressionKind::FOR_LOOP:
			return "for";
		case ExpressionKind::CONTINUE:
			return "continue";
		case ExpressionKind::BREAK:
			return "break";
		case ExpressionKind::FUNCTION_CALL:
			return "call";
		case ExpressionKind::CALLEE:
			return "callee";
		case ExpressionKind::LOGICAL_AND:
			return "and";
		case ExpressionKind::LOGICAL_OR:
			return "or";
		case ExpressionKind::POINTER_DEREFERENCE:
			return "pointer dereference";
		case ExpressionKind::ARRAY_SUBSCRIPT:
			return "array subscript";
		case ExpressionKind::NEGATE:
			return "negate";
		case ExpressionKind::ADDRESS_OF:
			return "address of";
		case ExpressionKind::MEMBER_ACCESS:
			return "member access";
	}
	return "unreachable";
}

std::string YepLang::ExpressionValueToString(const YepLang::ExpressionValue &value)
{
	if (std::holds_alternative<std::int64_t>(value))
	{
		return std::to_string(std::get<std::int64_t>(value));
	}
	else if (std::holds_alternative<std::uint64_t>(value))
	{
		return std::to_string(std::get<std::uint64_t>(value));
	}
	else if (std::holds_alternative<std::string>(value))
	{
		std::ostringstream oss;
		oss << '"';
		for (const auto &c: std::get<std::string>(value))
		{
			switch (c)
			{
				case '\r':
					oss << "\\r";
					break;
				case '\n':
					oss << "\\n";
					break;
				case '\"':
					oss << "\\\"";
					break;
				case '\0':
					oss << "\\0";
					break;
				default:
					oss << c;
					break;
			}
		}
		oss << '"';
		return oss.str();
	}
	else if (std::holds_alternative<char>(value))
	{
		std::ostringstream oss;
		oss << '\'' << std::get<char>(value) << '\'';
		return oss.str();
	}
	else if (std::holds_alternative<bool>(value))
	{
		if (std::get<bool>(value))
		{
			return "true";
		}
		else
		{
			return "false";
		}
	}
	else if (std::holds_alternative<ExpressionChildren>(value))
	{
		const auto &children = std::get<ExpressionChildren>(value);
		std::ostringstream oss;
		oss << "[ ";
		for (const auto &child:children)
		{
			oss << ExpressionValueToString(child.value) << ", ";
		}
		oss << "]";
		return oss.str();
	}
	else
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
}
