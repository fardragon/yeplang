#include "Function.hpp"
#include <sstream>




std::string YepLang::PrintFunction(const YepLang::Function &function)
{
	std::ostringstream oss;
	oss << "Function: \r\n";
	oss << "-name: " << function.prototype.name << std::endl;

	oss << "-args: ";
	for(auto it = function.prototype.arguments.begin(); it != function.prototype.arguments.end(); std::advance(it, 1))
	{
		oss << it->name << ": " << TypeToStr(it->type);
		if (it != std::prev(function.prototype.arguments.end()))
		{
			oss << ", ";
		}
	}
	oss << "\r\n";

	oss << "-type: " << TypeToStr(function.prototype.returnType) << std::endl;
	oss << "-body: \r\n";

	oss << YepLang::ExpressionToString(function.body, "     ");
	return oss.str();
}

