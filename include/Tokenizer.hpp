#ifndef YEPLANG_TOKENIZER_HPP
#define YEPLANG_TOKENIZER_HPP

#include <istream>
#include <vector>
#include <optional>
#include <unordered_map>

#include "Token.hpp"

namespace YepLang
{
	class Tokenizer
	{
	public:
		Tokenizer();
		std::vector<YepLang::Token> Tokenize(std::istream &input, const std::string &filename);

	private:
		std::string NextLine(std::istream &input);
		std::size_t CountTabs();
		std::vector<Token> TokenizeLine();

		static bool IsSpecialCharacter(char c);
		std::optional<std::string> IsKeyword(const std::string &line);
		Token SlurpKeyword(const std::string &keyword);
		Token SlurpStringLiteral();
		Token SlurpCharacterLiteral();
		Token SlurpSpecial();
		Token SlurpIdentifier();
		Token SlurpNumeric();
		void Advance(std::size_t count);

		[[noreturn]] void UnknownToken(const std::string &token);

	private:
		std::size_t _currentLine{0};
		std::string _currentFile;
		std::string _line;
		std::unordered_map<std::string, TokenType> _keywords;
	};

}


#endif //YEPLANG_TOKENIZER_HPP
