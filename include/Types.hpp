#ifndef YEPLANG_TYPES_HPP
#define YEPLANG_TYPES_HPP

#include <string>
#include <variant>
#include <memory>
#include <vector>

namespace YepLang
{
	struct BuiltinType;
	struct Array;
	struct Struct;
	struct Pointer;

	using Type = std::variant<BuiltinType, Array, Struct, Pointer>;

	enum class BuiltinTypeEnum
	{
		i32,
		i64,
		u64,
		boolean,
		character,
		void_t
	};

	struct BuiltinType
	{
		BuiltinTypeEnum type;
	};

	struct Array
	{
		std::shared_ptr<Type> elementType;
		std::uint64_t size;
	};

	struct StructField
	{
		std::string name;
		std::shared_ptr<Type> fieldType;
	};

	struct Struct
	{
		std::vector<StructField> fields;
	};

	struct Pointer
	{
		std::shared_ptr<Type> pointedType;
	};


	std::string TypeToStr(const Type &type);
	bool TypeIsSigned(const Type &type);
	bool TypeIsInteger(const Type &type);
	bool TypeIsSame(const Type &a, const Type &b);
	bool TypeIsArray(const Type &type);
	bool TypeIsStruct(const Type &type);
	bool TypeIsPointer(const Type &type);
	bool TypeIsBuiltin(const Type &type);
	bool TypeIsBuiltin(const Type &type, const BuiltinTypeEnum &builtinType);
	bool TypeIsComparable(const Type &type);
}

#endif //YEPLANG_TYPES_HPP
