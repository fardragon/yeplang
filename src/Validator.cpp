#include "Validator.hpp"

#include <stdexcept>
#include <sstream>
#include <algorithm>

#define VERIFY(condition, message, expression) if (!(condition)) ValidatorError(message, expression)

YepLang::Validator::Validator()
{
}

void YepLang::Validator::ValidatorError(std::string_view message, const YepLang::Expression &)
{
	std::ostringstream oss;
	oss << _currentFunction.name << ": " << message;
	throw std::invalid_argument(oss.str());
}

std::optional<YepLang::Type> YepLang::Validator::FindVariable(const std::string &name)
{
	const auto it = std::find_if(_scopes.rbegin(), _scopes.rend(), [&name](const ValidatorScope &scope)
	{
		return scope.variables.contains(name);
	});

	if (it == _scopes.rend())
	{
		return std::nullopt;
	}
	else
	{
		return it->variables.at(name);
	}
}


void YepLang::Validator::ValidateFunction(YepLang::Function &function)
{
	_currentFunction = function.prototype;

	ValidatorScope functionScope{};
	for (const auto &argument: function.prototype.arguments)
	{
		functionScope.variables.emplace(argument.name, argument.type);
	}
	_scopes.push_back(std::move(functionScope));

	//todo check duplicates
	_definedFunctions[function.prototype.name] = function.prototype;

	VERIFY(function.body.kind == ExpressionKind::SCOPE, "Function body is not a scope", function.body);
	ValidateExpression(function.body);
}


void YepLang::Validator::ValidateExpression(YepLang::Expression &expression)
{
	switch (expression.kind)
	{
		case ExpressionKind::SCOPE:
			return ValidateScope(expression);
		case ExpressionKind::LITERAL:
			return ValidateLiteral(expression);
		case ExpressionKind::VARIABLE:
			return ValidateVariable(expression);
		case ExpressionKind::VARIABLE_ASSIGNMENT:
			return ValidateVariableAssignment(expression);
		case ExpressionKind::VARIABLE_DECLARATION:
			return ValidateVariableDeclaration(expression);
		case ExpressionKind::RETURN:
			return ValidateReturn(expression);
		case ExpressionKind::CONDITIONAL:
			return ValidateConditional(expression);
		case ExpressionKind::PLUS_OP:
		case ExpressionKind::MINUS_OP:
		case ExpressionKind::DIVIDE_OP:
		case ExpressionKind::MULTIPLY_OP:
			return ValidateArithmetic(expression);
		case ExpressionKind::GREATER_THAN_OP:
		case ExpressionKind::LESS_THAN_OP:
		case ExpressionKind::EQUAL_OP:
		case ExpressionKind::NOT_EQUAL_OP:
			return ValidateComparison(expression);
		case ExpressionKind::LOGICAL_AND:
		case ExpressionKind::LOGICAL_OR:
			return ValidateLogical(expression);
		case ExpressionKind::POST_INC_OP:
			return ValidatePostInc(expression);
		case ExpressionKind::POINTER_DEREFERENCE:
			return ValidatePointerDereference(expression);
		case ExpressionKind::ARRAY_SUBSCRIPT:
			return ValidateArraySubscript(expression);
		case ExpressionKind::FUNCTION_CALL:
			return ValidateFunctionCall(expression);
		case ExpressionKind::FOR_LOOP:
			return ValidateForLoop(expression);
		case ExpressionKind::NEGATE:
			return ValidateNegate(expression);
		case ExpressionKind::ADDRESS_OF:
			return ValidateAddressOf(expression);
		case ExpressionKind::CONTINUE:
			break;
		case ExpressionKind::BREAK:
			break;
		case ExpressionKind::CALLEE:
			break;
		case ExpressionKind::MEMBER_ACCESS:
			return ValidateMemberAccess(expression);
	}
}

void YepLang::Validator::ValidateScope(YepLang::Expression &expression)
{
	VERIFY(!expression.type.has_value(), "Scope should not have a type", expression);
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Scope body is not a list of expressions", expression);

	_scopes.emplace_back();

	for (auto &expr: ExpressionGet<ExpressionChildren>(expression))
	{
		ValidateExpression(expr);
	}

	_scopes.pop_back();
}

void YepLang::Validator::ValidateLiteral(YepLang::Expression &expression)
{
	VERIFY(expression.type.has_value(), "Literal has no type", expression);
}

void YepLang::Validator::ValidateVariable(YepLang::Expression &expression)
{
	VERIFY(!expression.type.has_value(), "Aw snap!", expression);
	VERIFY(ExpressionHolds<std::string>(expression), "Variable is not a string", expression);

	const auto &variableName = ExpressionGet<std::string>(expression);
	VERIFY(!variableName.empty(), "Variable is an empty string", expression);

	const auto &variable = FindVariable(variableName);

	VERIFY(variable.has_value(), "Variable not found", expression);
	expression.type = variable.value();
}

void YepLang::Validator::ValidateVariableAssignment(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Assignment does not contain expressions", expression);
	auto &expressions = ExpressionGet<ExpressionChildren>(expression);

	VERIFY(expressions.size() == 2, "Assignment is not a pair of expressions", expression);

	auto &LHS = expressions[0];
	auto &RHS = expressions[1];

	VERIFY(KindIsIn(LHS.kind, ExpressionKind::VARIABLE, ExpressionKind::POINTER_DEREFERENCE, ExpressionKind::MEMBER_ACCESS),
	       "Assignment LHS is not a variable or a pointer dereference", expression);
	ValidateExpression(LHS);

	ValidateExpression(RHS);
	VERIFY(RHS.type.has_value(), "Assignment RHS cannot be assigned", expression);

	//TODO print actual types
	VERIFY(TypeIsSame(LHS.type.value(), RHS.type.value()), "Assignment types mismatch", expression);

	expression.type = LHS.type.value();
}

void YepLang::Validator::ValidateVariableDeclaration(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Variable declaration does not contain expressions",
	       expression);
	auto &expressions = ExpressionGet<ExpressionChildren>(expression);

	VERIFY(expressions.size() == 2, "Variable declaration is not a pair of expressions", expression);

	auto &LHS = expressions[0];
	auto &RHS = expressions[1];

	VERIFY(LHS.kind == ExpressionKind::VARIABLE, "Variable declaration LHS is not a variable", expression);
	VERIFY(LHS.type.has_value(), "Variable declaration has no type", expression);
	VERIFY(!TypeIsBuiltin(LHS.type.value(), BuiltinTypeEnum::void_t), "Cannot create void variable", expression);

	ValidateExpression(RHS);
	VERIFY(RHS.type.has_value(), "Variable initial value has no type", expression);
	//TODO print actual types
	VERIFY(TypeIsSame(LHS.type.value(), RHS.type.value()), "Variable declaration types mismatch", expression);

	expression.type = LHS.type.value();

	const auto &variableName = ExpressionGet<std::string>(LHS);
	_scopes.back().variables[variableName] = expression.type.value();
}

void YepLang::Validator::ValidateReturn(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Return does not contain an expression", expression);
	auto &expressions = ExpressionGet<ExpressionChildren>(expression);

	VERIFY(expressions.size() <= 1, "Return contains more than 1 expression", expression);
	if (expressions.empty())
	{
		//TODO print actual types
		VERIFY(TypeIsBuiltin(_currentFunction.returnType, BuiltinTypeEnum::void_t), "Return type does not match function type",
		       expression);
		expression.type = _currentFunction.returnType;
	}
	else
	{
		auto &expr = expressions[0];

		ValidateExpression(expr);

		VERIFY(expr.type.has_value(), "Return does not return a value", expression);
		VERIFY(TypeIsSame(expr.type.value(), _currentFunction.returnType), "Return type does not match function type",
		       expression);
		expression.type = expr.type;
	}
}

void YepLang::Validator::ValidateConditional(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Conditional does not contain expressions", expression);
	auto &expressions = ExpressionGet<ExpressionChildren>(expression);

	VERIFY(expressions.size() >= 2, "Conditional requires at least 2 expressions",
	       expression);

	auto &conditionalExpression = expressions[0];
	ValidateExpression(conditionalExpression);
	VERIFY(conditionalExpression.type.has_value(), "Condition is not a boolean", expression);
	VERIFY((TypeIsBuiltin(conditionalExpression.type.value(), BuiltinTypeEnum::boolean)), "Condition is not a boolean", expression);

	auto &trueExpression = expressions[1];
	VERIFY(trueExpression.kind == ExpressionKind::SCOPE, "True branch is not a scope", expression);
	ValidateScope(trueExpression);

	auto ix = 2U;

	for (; ix < expressions.size() - 1; ix += 2)
	{
		auto &elifCondition = expressions[ix];
		ValidateExpression(elifCondition);
		VERIFY(elifCondition.type.has_value(), "Elif condition is not a boolean", expression);
		VERIFY((TypeIsBuiltin(elifCondition.type.value(), BuiltinTypeEnum::boolean)), "Elif condition is not a boolean", expression);

		auto &elifBody = expressions[ix + 1];
		VERIFY(elifBody.kind == ExpressionKind::SCOPE, "Elif branch is not a scope", expression);
		ValidateScope(elifBody);
	}

	if (ix == expressions.size() - 1)
	{
		auto &elseBody = expressions[ix];
		VERIFY(elseBody.kind == ExpressionKind::SCOPE, "Else branch is not a scope", expression);
		ValidateScope(elseBody);
	}
}

void YepLang::Validator::ValidateArithmetic(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Arithmetic operation does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 2, "Arithmetic operation is not a pair of expressions", expression);

	auto &LHS = expressions[0];
	auto &RHS = expressions[1];

	ValidateExpression(LHS);
	VERIFY(LHS.type.has_value(), "Arithmetic operation LHS has no type", expression);
	VERIFY(TypeIsInteger(LHS.type.value()) || TypeIsPointer(LHS.type.value()),
	       "Arithmetic operation LHS is not an integer type or a pointer", expression);


	ValidateExpression(RHS);
	VERIFY(RHS.type.has_value(), "Arithmetic operation RHS has no type", expression);
	VERIFY(TypeIsInteger(RHS.type.value()), "Arithmetic operation RHS is not an integer type", expression);

	if (TypeIsPointer(LHS.type.value()))
	{
		VERIFY(KindIsIn(expression.kind, ExpressionKind::MINUS_OP, ExpressionKind::PLUS_OP),
		       "Only + and - operations are allowed on pointers", expression);
		expression.type = LHS.type;
	}
	else
	{
		//TODO print actual types
		VERIFY(TypeIsSame(LHS.type.value(), RHS.type.value()), "Arithmetic operation types mismatch", expression);
		expression.type = LHS.type;
	}

}

void YepLang::Validator::ValidateComparison(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Comparison operation does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 2, "Comparison operation is not a pair of expressions", expression);

	auto &LHS = expressions[0];
	auto &RHS = expressions[1];

	ValidateExpression(LHS);
	ValidateExpression(RHS);

	//TODO print actual types
	VERIFY(TypeIsSame(LHS.type.value(), RHS.type.value()), "Comparison operation types mismatch", expression);
	VERIFY(TypeIsComparable(LHS.type.value()), "Comparison operand type is not comparable", expression);

	expression.type = BuiltinType{BuiltinTypeEnum::boolean};
}

void YepLang::Validator::ValidateLogical(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Logical operation does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 2, "Logical operation is not a pair of expressions", expression);

	auto &LHS = expressions[0];
	auto &RHS = expressions[1];

	ValidateExpression(LHS);
	ValidateExpression(RHS);

	//TODO print actual types
	VERIFY(TypeIsSame(LHS.type.value(), RHS.type.value()), "Logical operation types mismatch", expression);
	VERIFY(TypeIsBuiltin(LHS.type.value(), BuiltinTypeEnum::boolean), "Logical operation operand type is not a boolean", expression);

	expression.type = LHS.type;
}

void YepLang::Validator::ValidatePostInc(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Post increment does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 1, "Post increment contains more than one expression", expression);

	auto &operand = expressions[0];
	VERIFY(KindIsIn(operand.kind, ExpressionKind::VARIABLE), "Post increment operand is not a variable", expression);

	ValidateExpression(operand);
	VERIFY(operand.type.has_value(), "Post increment operand has no type", expression);
	VERIFY(TypeIsInteger(operand.type.value()) || TypeIsPointer(operand.type.value()),
	       "Post increment operand is not an integer type or pointer", expression);

	expression.type = operand.type;
}

void YepLang::Validator::ValidatePointerDereference(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Pointer dereference does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 1, "Pointer dereference contains more than one expression", expression);

	auto &operand = expressions[0];

	ValidateExpression(operand);
	VERIFY(operand.type.has_value(), "Pointer dereference operand has no type", expression);
	VERIFY(TypeIsPointer(operand.type.value()), "Pointer dereference operand is not a pointer", expression);

	expression.type = *std::get<Pointer>(operand.type.value()).pointedType;
}

void YepLang::Validator::ValidateArraySubscript(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Array subscript does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 2, "Array subscript is not a pair of expressions", expression);

	auto &target = expressions[0];
	auto &index = expressions[1];

	ValidateExpression(target);
	VERIFY(target.type.has_value(), "Array subscript target has no type", expression);
	VERIFY(TypeIsPointer(target.type.value()) || TypeIsArray(target.type.value()),
	       "Array subscript target is not an array or a pointer", expression);

	ValidateExpression(index);
	VERIFY(index.type.has_value(), "Array subscript index has no type", expression);
	VERIFY(TypeIsInteger(index.type.value()), "Array subscript target is not an integer", expression);

	if (TypeIsArray(target.type.value()))
	{
		expression.type = *std::get<YepLang::Array>(target.type.value()).elementType;
	}
	else
	{
		expression.type = *std::get<YepLang::Pointer>(target.type.value()).pointedType;
	}
}

void YepLang::Validator::ValidateFunctionCall(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Function call does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(!expressions.empty(), "Call expression is empty", expression);

	auto &calleeExpression = expressions[0];
	ValidateExpression(calleeExpression);
	VERIFY(KindIsIn(calleeExpression.kind, ExpressionKind::CALLEE), "Function call has no callee", expression);

	const auto &callee = ExpressionGet<std::string>(calleeExpression);

	const auto &prototype = FindFunction(callee);
	VERIFY(prototype.has_value(), "Unknown function", expression);

	VERIFY(prototype->arguments.size() == (expressions.size() - 1), "Mismatched argument count", expression);
	for (auto i = 0U; i < prototype->arguments.size(); ++i)
	{
		auto &argExpression = expressions[i + 1];
		ValidateExpression(argExpression);

		VERIFY(argExpression.type.has_value(), "Function call argument has no type", expression);
		//TODO print actual types
		VERIFY(TypeIsSame(argExpression.type.value(), prototype->arguments[i].type),
		       "Function call argument types mismatch", expression);
	}
	expression.type = prototype.value().returnType;
}

std::optional<YepLang::FunctionPrototype> YepLang::Validator::FindFunction(const std::string &name)
{
	auto it = _definedFunctions.find(name);
	if (it != _definedFunctions.end())
	{
		return it->second;
	}
	else
	{
		return std::nullopt;
	}
}

void YepLang::Validator::ValidateExternalFunction(const FunctionPrototype &prototype)
{
	//todo check for duplicates
	_definedFunctions[prototype.name] = prototype;
}

void YepLang::Validator::ValidateForLoop(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "For loop does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 4, "For loop requires 4 expressions", expression);

	auto &initExpression = expressions[0];
	ValidateExpression(initExpression);

	auto &conditionExpression = expressions[1];
	ValidateExpression(conditionExpression);
	VERIFY(conditionExpression.type.has_value(), "For loop condition expression has no type", expression);
	VERIFY(TypeIsBuiltin(conditionExpression.type.value(), BuiltinTypeEnum::boolean), "For loop condition expression is not a boolean",
	       expression);

	auto &postExpression = expressions[2];
	ValidateExpression(postExpression);

	auto &bodyExpression = expressions[3];
	ValidateExpression(bodyExpression);
	VERIFY(KindIsIn(bodyExpression.kind, ExpressionKind::SCOPE), "For loop body is not a scope", expression);
}

void YepLang::Validator::ValidateNegate(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Negate does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 1, "Negate contains more than one expression", expression);

	auto &operand = expressions[0];
	VERIFY(KindIsIn(operand.kind, ExpressionKind::LITERAL), "Negate operand is not a variable", expression);

	ValidateExpression(operand);
	VERIFY(operand.type.has_value(), "Negate operand has no type", expression);
	VERIFY(TypeIsInteger(operand.type.value()) && TypeIsSigned(operand.type.value()),
	       "Negate operand is not an signed integer type", expression);

	expression.type = operand.type;
	expression.kind = ExpressionKind::LITERAL;
	expression.value = -(ExpressionGet<std::int64_t>(operand));
}

void YepLang::Validator::ValidateAddressOf(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Address of does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 1, "Address of contains more than one expression", expression);

	auto &operand = expressions[0];
	VERIFY(KindIsIn(operand.kind, ExpressionKind::VARIABLE), "Address of operand is not a variable", expression);

	ValidateExpression(operand);
	VERIFY(operand.type.has_value(), "Address of operand has no type", expression);

	expression.type = YepLang::Pointer {std::make_shared<YepLang::Type>(operand.type.value())};
}

void YepLang::Validator::ValidateMemberAccess(YepLang::Expression &expression)
{
	VERIFY(ExpressionHolds<ExpressionChildren>(expression), "Member access does not contain expressions",
	       expression);

	auto &expressions = ExpressionGet<ExpressionChildren>(expression);
	VERIFY(expressions.size() == 2, "Member access is not a pair of expressions", expression);

	auto &target = expressions[0];
	auto &field = expressions[1];

	ValidateExpression(target);
	VERIFY(target.type.has_value(), "Member access target has no type", expression);

	VERIFY(TypeIsStruct(target.type.value()), "Member access target is not a struct", expression);

	const auto &structDef = std::get<YepLang::Struct>(target.type.value());

	VERIFY(((field.kind == ExpressionKind::VARIABLE) && ExpressionHolds<std::string>(field)), "Member access index is not a string", expression);

	auto fieldName = ExpressionGet<std::string>(field);

	for (const auto &targetField: structDef.fields)
	{
		if (targetField.name == fieldName)
		{
			expression.type = *targetField.fieldType;
			field.type = *targetField.fieldType;
			return;
		}
	}

	VERIFY(false, "Unknown struct field", expression);
}

