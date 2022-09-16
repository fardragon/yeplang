#include "Tokenizer.hpp"

#include <iostream>
#include <sstream>
#include <cctype>

YepLang::Tokenizer::Tokenizer()
{
	_keywords =
			{
					{"function", TokenType::Function},
					{"return",   TokenType::Return},
					{"var",      TokenType::VariableDeclaration},
					{"if",       TokenType::If},
					{"else",     TokenType::Else},
					{"elif",     TokenType::Elif},
					{"for",      TokenType::For},
					{"continue", TokenType::Continue},
					{"break",    TokenType::Break},
					{"and",      TokenType::LogicalAnd},
					{"or",       TokenType::LogicalOr},
					{"struct",   TokenType::Struct}
			};
}

std::vector<YepLang::Token> YepLang::Tokenizer::Tokenize(std::istream &input, const std::string &filename)
{
	_currentLine = 0;
	_currentFile = filename;
	std::size_t indentation = 0U;
	std::vector<YepLang::Token> tokens{};

	while (true)
	{
		_line = NextLine(input);

		if (_line.empty())
		{
			tokens.insert(tokens.end(), indentation,
			              {_currentFile, _currentLine, TokenType::IndentMinus, std::nullopt});
			tokens.push_back(
					{
							_currentFile,
							_currentLine,
							TokenType::EndOfFile,
							std::nullopt
					});
			break;
		}
		const auto lineIndentation = CountTabs();
		const auto indentChange = static_cast<std::int64_t>(lineIndentation) - static_cast<std::int64_t>(indentation);

		tokens.insert(tokens.end(), std::abs(indentChange),
		              {
				              _currentFile,
				              _currentLine,
				              indentChange > 0 ? TokenType::IndentPlus : TokenType::IndentMinus,
				              std::nullopt
		              });

		indentation = lineIndentation;
		auto lineTokens = TokenizeLine();
		tokens.insert(tokens.end(), lineTokens.begin(), lineTokens.end());
		tokens.push_back({_currentFile, _currentLine, TokenType::EndOfLine, std::nullopt});
	}

	return tokens;
}

std::string YepLang::Tokenizer::NextLine(std::istream &input)
{
	std::string line{};
	while (std::getline(input, line))
	{
		++_currentLine;
		if (line.empty() || line.starts_with('#'))
		{
			continue;
		}
		else
		{
			return line;
		}
	}

	if (input.eof())
	{
		return "";
	}
	else
	{
		throw std::runtime_error("Tokenizer error in NextLine");
	}
}

std::size_t YepLang::Tokenizer::CountTabs()
{
	std::size_t lineIndentation{0};
	while (_line.front() == '\t')
	{
		++lineIndentation;
		Advance(1);
	}
	return lineIndentation;
}

std::vector<YepLang::Token> YepLang::Tokenizer::TokenizeLine()
{
	std::vector<YepLang::Token> tokens;

	while (!_line.empty())
	{
		if (auto keyword = IsKeyword(_line); keyword.has_value())
		{
			tokens.push_back(SlurpKeyword(keyword.value()));
		}
		else if (_line.starts_with('#'))
		{
			Advance(_line.size());
		}
		else if (_line.starts_with('\"'))
		{
			tokens.push_back(Tokenizer::SlurpStringLiteral());
		}
		else if (_line.starts_with('\''))
		{
			tokens.push_back(Tokenizer::SlurpCharacterLiteral());
		}
		else if (Tokenizer::IsSpecialCharacter(_line.front()))
		{
			tokens.push_back(Tokenizer::SlurpSpecial());
		}
		else if (std::isdigit(_line.front()))
		{
			tokens.push_back(Tokenizer::SlurpNumeric());
		}
		else if (std::isalpha(_line.front()))
		{
			tokens.push_back(Tokenizer::SlurpIdentifier());
		}
		else if (std::isspace(_line.front()))
		{
			Advance(1);
		}
		else
		{
			UnknownToken(_line);
		}
	}

	return tokens;
}

bool YepLang::Tokenizer::IsSpecialCharacter(char c)
{
	return c == '(' || c == ')' || c == '-' || c == '>' || c == ':' || c == '+' || c == '*' || c == '/' || c == '<' ||
	       c == '=' || c == '!' || c == ',' || c == '[' || c == ']' || c == '&' || c == '{' || c == '}' || c == '.';
}

YepLang::Token YepLang::Tokenizer::SlurpSpecial()
{
	auto firstChar = _line.front();
	switch (firstChar)
	{
		case '(':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::LeftParen,
					std::nullopt
			};
		}
		case '*':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::Asterisk,
					std::nullopt
			};
		}
		case '.':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::Dot,
					std::nullopt
			};
		}
		case '&':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::Ampersand,
					std::nullopt
			};
		}
		case '/':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::DivideOp,
					std::nullopt
			};
		}
		case ')':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::RightParen,
					std::nullopt
			};
		}
		case '[':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::LeftBracket,
					std::nullopt
			};
		}
		case ']':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::RightBracket,
					std::nullopt
			};
		}
		case '{':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::LeftBrace,
					std::nullopt
			};
		}
		case '}':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::RightBrace,
					std::nullopt
			};
		}
		case '-':
		{
			if (_line.starts_with("->"))
			{
				Advance(2);
				return {
						_currentFile,
						_currentLine,
						TokenType::RightArrow,
						std::nullopt
				};
			}
			else
			{
				Advance(1);
				return {
						_currentFile,
						_currentLine,
						TokenType::Minus,
						std::nullopt
				};
			}
		}
		case ':':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::Colon,
					std::nullopt
			};
		}
		case ',':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::Comma,
					std::nullopt
			};
		}
		case '+':
		{
			if (_line.starts_with("++"))
			{
				Advance(2);
				return {
						_currentFile,
						_currentLine,
						TokenType::PlusPlusOp,
						std::nullopt
				};
			}
			else
			{
				Advance(1);
				return {
						_currentFile,
						_currentLine,
						TokenType::PlusOp,
						std::nullopt
				};
			}
		}
		case '<':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::LeftChevron,
					std::nullopt
			};
		}
		case '>':
		{
			Advance(1);
			return {
					_currentFile,
					_currentLine,
					TokenType::RightChevron,
					std::nullopt
			};
		}
		case '=':
		{
			if (_line.starts_with("=="))
			{
				Advance(2);
				return {
						_currentFile,
						_currentLine,
						TokenType::Equal,
						std::nullopt
				};
			}
			else
			{
				Advance(1);
				return {
						_currentFile,
						_currentLine,
						TokenType::Assignment,
						std::nullopt
				};
			}
		}
		case '!':
		{
			if (_line.starts_with("!="))
			{
				Advance(2);
				return {
						_currentFile,
						_currentLine,
						TokenType::NotEqual,
						std::nullopt
				};
			}
			else
			{
				UnknownToken(_line);
			}
		}
		default:
			UnknownToken(_line);
	}
}

YepLang::Token YepLang::Tokenizer::SlurpIdentifier()
{
	std::size_t index = 0;

	for (const auto &c: _line)
	{
		if (Tokenizer::IsSpecialCharacter(c) || std::isspace(c))
		{
			break;
		}
		++index;
	}

	auto identifier = _line.substr(0, index);
	Advance(index);

	return {
			_currentFile,
			_currentLine,
			TokenType::Identifier,
			std::move(identifier)
	};
}

YepLang::Token YepLang::Tokenizer::SlurpNumeric()
{
	std::size_t index = 0;

	for (const auto &c: _line)
	{
		if (!std::isdigit(c))
		{
			break;
		}
		++index;
	}
	auto value = _line.substr(0, index);
	Advance(index);

	auto type = TokenType::I64Literal;
	if (_line.starts_with("u64"))
	{
		Advance(3);
		type = TokenType::U64Literal;
	}
	else if (_line.starts_with("i64"))
	{
		Advance(3);
		type = TokenType::I64Literal;
	}

	return {
			_currentFile,
			_currentLine,
			type,
			std::move(value)
	};
}

void YepLang::Tokenizer::UnknownToken(const std::string &token)
{
	std::ostringstream oss;
	oss << _currentFile << ":" << _currentLine << " Invalid token: " << token;
	throw std::invalid_argument(oss.str());
}

std::optional<std::string> YepLang::Tokenizer::IsKeyword(const std::string &line)
{
	for (const auto &[keyword, _]: _keywords)
	{
		if (line.starts_with(keyword))
		{
			return keyword;
		}
	}
	return std::nullopt;
}

YepLang::Token YepLang::Tokenizer::SlurpKeyword(const std::string &keyword)
{
	auto tokenType = _keywords.at(keyword);
	Advance(keyword.length());
	return {
			_currentFile,
			_currentLine,
			tokenType,
			std::nullopt
	};
}

YepLang::Token YepLang::Tokenizer::SlurpStringLiteral()
{
	Advance(1);
	std::string literal{};
	bool finished = false;

	while (!_line.empty())
	{
		auto c = _line.front();

		if (c == '"')
		{
			finished = true;
			Advance(1);
			break;
		}
		else if (c == '\\')
		{
			Advance(1);
			if (_line.empty())
			{
				break;
			}
			else
			{
				auto nextC = _line.front();
				switch (nextC)
				{
					case 'r':
						literal.push_back('\r');
						break;
					case 'n':
						literal.push_back('\n');
						break;
					case '"':
						literal.push_back('"');
						break;
					default:
						literal.push_back(nextC);
						break;
				}
				Advance(1);
			}
		}
		else
		{
			literal.push_back(c);
			Advance(1);
		}
	}

	if (!finished)
	{
		std::ostringstream oss;
		oss << _currentFile << ":" << _currentLine << " Uncapped string";
		throw std::invalid_argument(oss.str());
	}
	return {
			_currentFile,
			_currentLine,
			TokenType::StringLiteral,
			std::move(literal)
	};
}

YepLang::Token YepLang::Tokenizer::SlurpCharacterLiteral()
{
	Advance(1);
	std::string literal{};

	if (auto c = _line.front(); c == '\\')
	{
		Advance(1);
		auto nextC = _line.front();
		switch (nextC)
		{
			case 'r':
				literal = '\r';
				break;
			case 'n':
				literal = '\n';
				break;
			case '\'':
				literal = '\'';
				break;
			case '0':
				literal = '\0';
				break;
			default:
				literal.push_back(nextC);
				break;
		}
		Advance(1);

	}
	else
	{
		Advance(1);
		literal = c;
	}

	//TODO check for closing quote
	Advance(1);

	return {
			_currentFile,
			_currentLine,
			TokenType::CharacterLiteral,
			std::move(literal)
	};
}

void YepLang::Tokenizer::Advance(std::size_t count)
{
	_line.erase(0, count);
}


