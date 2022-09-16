#include <iostream>
#include <fstream>
#include <vector>

#include "Tokenizer.hpp"
#include "Parser.hpp"
#include "Generator.hpp"
#include "Validator.hpp"

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		throw std::runtime_error("Not enough arguments");
	}

	std::ifstream input(argv[1], std::ios::binary);
	std::ofstream output("main.ll", std::ios::binary | std::ios::trunc);

	YepLang::Tokenizer tokenizer{};

	auto tokens = tokenizer.Tokenize(input, argv[1]);

	YepLang::Parser parser{};
	auto functions = parser.Parse(tokens);

	YepLang::Validator validator{};
	for (auto &f: functions)
	{
		validator.ValidateFunction(f);
	}


	YepLang::Generator generator{};
	for (const auto &f: functions)
	{
		std::cout << YepLang::PrintFunction(f);
		generator.GenerateFunction(f);
	}

	generator.Dump();
	return 0;
}
