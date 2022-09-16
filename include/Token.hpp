#ifndef YEPLANG_TOKEN_HPP
#define YEPLANG_TOKEN_HPP

#include <string>
#include <optional>

namespace YepLang
{
	enum class TokenType
	{
		Function,
		VariableDeclaration,
		Identifier,
		LeftParen,
		RightParen,
		RightArrow,
		LeftChevron,
		RightChevron,
		LeftBracket,
		RightBracket,
		Equal,
		NotEqual,
		Colon,
		Comma,
		Return,
		I64Literal,
		U64Literal,
		StringLiteral,
		CharacterLiteral,
		PlusOp,
		PlusPlusOp,
		Minus,
		Asterisk,
		DivideOp,
		IndentPlus,
		IndentMinus,
		EndOfFile,
		EndOfLine,
		If,
		Else,
		Elif,
		Assignment,
		For,
		Continue,
		Break,
		LogicalAnd,
		LogicalOr,
		Ampersand,
		Struct,
		LeftBrace,
		RightBrace,
		Dot
	};

	struct Token
	{
		std::string file;
		std::size_t line;
		TokenType type;
		std::optional <std::string> value;
	};
}

#endif //YEPLANG_TOKEN_HPP
