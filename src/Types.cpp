#include "Types.hpp"
#include <sstream>
#include <stdexcept>

std::string YepLang::TypeToStr(const Type &type)
{
	if (TypeIsBuiltin(type))
	{
		switch (std::get<BuiltinType>(type).type)
		{
			case BuiltinTypeEnum::i32:
				return "i32";
			case BuiltinTypeEnum::i64:
				return "i64";
			case BuiltinTypeEnum::boolean:
				return "bool";
			case BuiltinTypeEnum::character:
				return "char";
			case BuiltinTypeEnum::u64:
				return "u64";
			case BuiltinTypeEnum::void_t:
				return "void";
		}
	}
	else if (TypeIsArray(type))
	{
		const auto &array = std::get<Array>(type);
		std::ostringstream oss;
		oss << TypeToStr(*array.elementType) << '[' << array.size << ']';
		return oss.str();
	}
	else if (TypeIsStruct(type))
	{
		const auto &structType = std::get<Struct>(type);
		std::ostringstream oss;
		oss << "TODO: Struct" ;
		return oss.str();
	}
	else if (TypeIsPointer(type))
	{
		return "pointer";
	}

	throw std::runtime_error(__PRETTY_FUNCTION__ );
}

bool YepLang::TypeIsSigned(const YepLang::Type &type)
{
	if (std::holds_alternative<BuiltinType>(type))
	{
		switch (std::get<BuiltinType>(type).type)
		{
			case BuiltinTypeEnum::i32:
			case BuiltinTypeEnum::i64:
				return true;
			default:
				return false;
		}
	}
	else
	{
		return false;
	}
}

bool YepLang::TypeIsInteger(const YepLang::Type &type)
{
	if (std::holds_alternative<BuiltinType>(type))
	{
		switch (std::get<BuiltinType>(type).type)
		{
			case BuiltinTypeEnum::i32:
			case BuiltinTypeEnum::i64:
			case BuiltinTypeEnum::u64:
				return true;
			default:
				return false;
		}
	}
	else
	{
		return false;
	}
}

bool YepLang::TypeIsSame(const YepLang::Type &a, const YepLang::Type &b)
{
	if (std::holds_alternative<BuiltinType>(a))
	{
		if (!std::holds_alternative<BuiltinType>(b))
		{
			return false;
		}
		const auto &builtinA = std::get<BuiltinType>(a).type;
		const auto &builtinB = std::get<BuiltinType>(b).type;

		if (builtinA != builtinB)
		{
			return false;
		}
	}
	else if (std::holds_alternative<YepLang::Array>(a))
	{
		const auto &arrayA = std::get<YepLang::Array>(a);
		const auto &arrayB = std::get<YepLang::Array>(b);

		if (!TypeIsSame(*arrayA.elementType, *arrayB.elementType))
		{
			return false;
		}

		if (arrayA.size != arrayB.size)
		{
			return false;
		}
	}
	else if (std::holds_alternative<YepLang::Struct>(a))
	{
		const auto &structA = std::get<YepLang::Struct>(a);
		const auto &structB = std::get<YepLang::Struct>(b);

		if (structA.fields.size() != structB.fields.size())
		{
			return false;
		}

		for (auto i = 0U; i < structA.fields.size(); ++i)
		{
			if (!TypeIsSame(*structA.fields[i].fieldType, *structB.fields[i].fieldType))
			{
				return false;
			}
		}
	}
	else if (std::holds_alternative<Pointer>(a))
	{
		if (!std::holds_alternative<Pointer>(b))
		{
			return false;
		}
		const auto &builtinA = std::get<Pointer>(a).pointedType;
		const auto &builtinB = std::get<Pointer>(b).pointedType;

		if (!TypeIsSame(*builtinA, *builtinB))
		{
			return false;
		}
	}
	else
	{
		throw std::runtime_error(__PRETTY_FUNCTION__ );
	}

	return true;
}

bool YepLang::TypeIsArray(const YepLang::Type &type)
{
	return std::holds_alternative<YepLang::Array>(type);
}

bool YepLang::TypeIsStruct(const Type &type)
{
	return std::holds_alternative<YepLang::Struct>(type);
}

bool YepLang::TypeIsPointer(const YepLang::Type &type)
{
	return std::holds_alternative<YepLang::Pointer>(type);
}

bool YepLang::TypeIsComparable(const YepLang::Type &type)
{
	if (std::holds_alternative<BuiltinType>(type))
	{
		switch (std::get<BuiltinType>(type).type)
		{
			case BuiltinTypeEnum::i32:
			case BuiltinTypeEnum::i64:
			case BuiltinTypeEnum::u64:
			case BuiltinTypeEnum::character:
				return true;
			case BuiltinTypeEnum::boolean:
			case BuiltinTypeEnum::void_t:
				return false;
		}
	}
	else if (std::holds_alternative<Pointer>(type))
	{
		return true;
	}
	else
	{
		return false;
	}
	return false;
}

bool YepLang::TypeIsBuiltin(const YepLang::Type &type)
{
	return std::holds_alternative<YepLang::BuiltinType>(type);
}

bool YepLang::TypeIsBuiltin(const YepLang::Type &type, const YepLang::BuiltinTypeEnum &builtinType)
{
	if (!std::holds_alternative<YepLang::BuiltinType>(type))
	{
		return false;
	}
	const auto &ty = std::get<YepLang::BuiltinType>(type).type;
	return ty == builtinType;
}