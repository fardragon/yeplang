#ifndef YEPLANG_FUNCTION_HPP
#define YEPLANG_FUNCTION_HPP

#include <string>
#include <vector>
#include "Types.hpp"
#include "Expression.hpp"

namespace YepLang
{

	struct FunctionArgument
	{
		std::string name;
		YepLang::Type type;
	};

	struct FunctionPrototype
	{
		std::string name;
		std::vector<FunctionArgument> arguments;
		YepLang::Type returnType;
	};

	struct Function
	{
		FunctionPrototype prototype;
		Expression body;
	};

	std::string PrintFunction(const Function &function);
	std::string PrintFunctionPrototype(const FunctionPrototype &prototype);


}

#endif //YEPLANG_FUNCTION_HPP
