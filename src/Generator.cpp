#include "Generator.hpp"

YepLang::Generator::Generator()
		: _context(std::make_unique<llvm::LLVMContext>()), _module(std::make_unique<llvm::Module>("main.yl", *_context))
		  , _builder(std::make_unique<llvm::IRBuilder<llvm::NoFolder>>(*_context))
{
	// global scope
	_scopes.push_back(GeneratorScope{});
}

void YepLang::Generator::GenerateFunction(const YepLang::Function &function)
{
	std::vector<llvm::Type *> args{};
	args.reserve(function.prototype.arguments.size());
	for (const auto &arg: function.prototype.arguments)
	{
		args.push_back(CodegenType(arg.type));
	}

	const auto prototype = llvm::FunctionType::get(CodegenType(function.prototype.returnType), args, false);

	const auto func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage, function.prototype.name,
	                                         _module.get());
	_currentFunction = func;

	auto argsIt = function.prototype.arguments.begin();
	auto funcArgsIt = func->arg_begin();
	while (argsIt != function.prototype.arguments.end() && funcArgsIt != func->arg_end())
	{
		funcArgsIt->setName(argsIt->name);
		std::advance(argsIt, 1);
		std::advance(funcArgsIt, 1);
	}

	auto block = llvm::BasicBlock::Create(*_context, NextLabel().second, func);
	_builder->SetInsertPoint(block);

	// push function scope
	GeneratorScope functionScope{};
	for (auto &var: func->args())
	{
		const auto address = AllocaVariable(std::string(var.getName()), var.getType());
		_builder->CreateStore(&var, address);

		GeneratorVariable variable{
				var.getType(),
				address
		};
		functionScope.variables.emplace(var.getName(), variable);
	}
	_scopes.push_back(std::move(functionScope));

	CodegenScope(function.body);

	if (func->getBasicBlockList().back().empty())
	{
		func->getBasicBlockList().pop_back();
	}

	// pop function scope
	_scopes.pop_back();
}

bool YepLang::Generator::CodegenScope(const Expression &scope)
{
	if (scope.kind != ExpressionKind::SCOPE)
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}

	auto lastScopeTerminating = false;
	const auto &scopeExpressions = std::get<ExpressionChildren>(scope.value);
	for (const auto &expression: scopeExpressions)
	{
		switch (expression.kind)
		{
			case ExpressionKind::VARIABLE:
			case ExpressionKind::POST_INC_OP:
			case ExpressionKind::LITERAL:
			case ExpressionKind::RETURN:
			case ExpressionKind::PLUS_OP:
			case ExpressionKind::MINUS_OP:
			case ExpressionKind::DIVIDE_OP:
			case ExpressionKind::MULTIPLY_OP:
			case ExpressionKind::LESS_THAN_OP:
			case ExpressionKind::GREATER_THAN_OP:
			case ExpressionKind::EQUAL_OP:
			case ExpressionKind::VARIABLE_ASSIGNMENT:
			case ExpressionKind::FUNCTION_CALL:
				[[fallthrough]];
			case ExpressionKind::NOT_EQUAL_OP:
				CodegenExpression(expression);
				break;
			case ExpressionKind::CONDITIONAL:
				CodegenConditional(expression);
				break;
			case ExpressionKind::SCOPE:
				lastScopeTerminating = CodegenScope(expression);
				break;
			case ExpressionKind::VARIABLE_DECLARATION:
				CodegenVariableDeclaration(expression);
				break;
			case ExpressionKind::FOR_LOOP:
				CodegenForLoop(expression);
				break;
			case ExpressionKind::CONTINUE:
				CodegenContinue();
				break;
			case ExpressionKind::BREAK:
				CodegenBreak();
				break;
			default:
				throw std::runtime_error(__PRETTY_FUNCTION__);
		}
	}
	if ((scopeExpressions.back().kind == ExpressionKind::RETURN)
	    || (scopeExpressions.back().kind == ExpressionKind::CONTINUE)
	    || (scopeExpressions.back().kind == ExpressionKind::BREAK))
	{
		return true;
	}
	else if (scopeExpressions.back().kind == ExpressionKind::SCOPE)
	{
		return lastScopeTerminating;
	}
	else
	{
		return false;
	}
}

llvm::Value *YepLang::Generator::CodegenExpression(const Expression &expression)
{
	switch (expression.kind)
	{
		case ExpressionKind::LITERAL:
			return CodegenLiteral(expression);
		case ExpressionKind::VARIABLE:
			return CodegenVariable(expression);
		case ExpressionKind::POST_INC_OP:
			return CodegenPostIncrement(expression);
		case ExpressionKind::RETURN:
			return CodegenReturn(expression);
		case ExpressionKind::PLUS_OP:
		case ExpressionKind::MINUS_OP:
		case ExpressionKind::DIVIDE_OP:
			[[fallthrough]];
		case ExpressionKind::MULTIPLY_OP:
			return CodegenBinaryArithmeticOp(expression);
		case ExpressionKind::LESS_THAN_OP:
		case ExpressionKind::GREATER_THAN_OP:
		case ExpressionKind::EQUAL_OP:
			[[fallthrough]];
		case ExpressionKind::NOT_EQUAL_OP:
			return CodegenBinaryRelationalOp(expression);
		case ExpressionKind::LOGICAL_AND:
		case ExpressionKind::LOGICAL_OR:
			return CodegenBinaryLogicalOp(expression);
		case ExpressionKind::VARIABLE_ASSIGNMENT:
			return CodegenAssignment(expression);
		case ExpressionKind::FUNCTION_CALL:
			return CodegenFunctionCall(expression);
		case ExpressionKind::POINTER_DEREFERENCE:
			return CodegenPointerDereference(expression);
		case ExpressionKind::ARRAY_SUBSCRIPT:
			return CodegenArraySubscript(expression);
		case ExpressionKind::ADDRESS_OF:
			return CodegenAddressOf(expression);
		case ExpressionKind::MEMBER_ACCESS:
			return CodegenMemberAccess(expression);
		case ExpressionKind::CONDITIONAL:
		case ExpressionKind::SCOPE:
			[[fallthrough]];

		default:
			throw std::runtime_error(__PRETTY_FUNCTION__);
	}
}

llvm::Value *YepLang::Generator::CodegenLiteral(const YepLang::Expression &expression)
{
	if (TypeIsBuiltin(expression.type.value()))
	{
		switch (std::get<BuiltinType>(expression.type.value()).type)
		{
			case BuiltinTypeEnum::i32:
				return llvm::ConstantInt::get(*_context,
				                              llvm::APInt(32, std::get<std::int64_t>(expression.value), true));
			case BuiltinTypeEnum::i64:
				return llvm::ConstantInt::get(*_context,
				                              llvm::APInt(64, std::get<std::int64_t>(expression.value), true));
			case BuiltinTypeEnum::u64:
				return llvm::ConstantInt::get(*_context,
				                              llvm::APInt(64, std::get<std::uint64_t>(expression.value), false));
			case BuiltinTypeEnum::character:
			{
				const auto &c = std::get<char>(expression.value);
				return llvm::ConstantInt::get(*_context, llvm::APInt(8, c));
			}
			case BuiltinTypeEnum::boolean:
			{
				return llvm::ConstantInt::get(*_context, llvm::APInt(1, std::get<bool>(expression.value) ? 1 : 0));
			}
			default:
				throw std::runtime_error(__PRETTY_FUNCTION__);
		}
	}
	else if (TypeIsArray(expression.type.value()))
	{
		const auto &arrayType = std::get<YepLang::Array>(expression.type.value());

		auto arrayTypeCodegen = CodegenType(expression.type.value());
		auto arrayElementTypeCodegen = CodegenType(*arrayType.elementType);
		auto arrayLocation = AllocaVariable("", arrayTypeCodegen);

		auto zero = llvm::ConstantInt::get(*_context, llvm::APInt(64, 0, false));
		auto arrayBegin = _builder->CreateGEP(arrayTypeCodegen, arrayLocation, {zero, zero});


		auto initializers = std::get<ExpressionChildren>(expression.value);
		for (auto i = 0U; i < initializers.size(); ++i)
		{
			auto idx = llvm::ConstantInt::get(*_context, llvm::APInt(64, i, false));
			auto offsetPtr = _builder->CreateGEP(arrayElementTypeCodegen, arrayBegin, idx);

			auto initExpression = CodegenExpression(initializers[i]);
			_builder->CreateStore(initExpression, offsetPtr);
		}

		return arrayLocation;
	}
	else if (TypeIsStruct(expression.type.value()))
	{
		auto structCodegen = CodegenType(expression.type.value());
		auto structLocation = AllocaVariable("", structCodegen);

		auto initializers = std::get<ExpressionChildren>(expression.value);
		for (auto i = 0U; i < initializers.size(); ++i)
		{
			auto offsetPtr = _builder->CreateStructGEP(structCodegen, structLocation, i);
			auto initExpression = CodegenExpression(initializers[i]);
			_builder->CreateStore(initExpression, offsetPtr);
		}

		return structLocation;
	}
	else if (TypeIsPointer(expression.type.value()))
	{
		const auto &pointedType = std::get<YepLang::Pointer>(expression.type.value()).pointedType;

		if (TypeIsBuiltin(*pointedType))
		{
			if (TypeIsBuiltin(*pointedType, BuiltinTypeEnum::character))
			{
				const auto &str = std::get<std::string>(expression.value);
				return _builder->CreateGlobalStringPtr(str, NextLabel().second, 0, _module.get());
			}
			else
			{
				throw std::runtime_error(__PRETTY_FUNCTION__);
			}
		}
		else
		{
			throw std::runtime_error(__PRETTY_FUNCTION__);
		}
	}
	else
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}
}

llvm::Value *YepLang::Generator::CodegenReturn(const YepLang::Expression &expression)
{
	const auto &children = std::get<ExpressionChildren>(expression.value);

	if (children.empty())
	{
		return _builder->CreateRetVoid();
	}
	else
	{
		const auto returnValue = CodegenExpression(children.front());
		return _builder->CreateRet(returnValue);
	}
}

llvm::Value *YepLang::Generator::CodegenBinaryArithmeticOp(const YepLang::Expression &expression)
{
	const auto children = std::get<ExpressionChildren>(expression.value);
	const auto leftOperand = CodegenExpression(children[0]);
	auto rightOperand = CodegenExpression(children[1]);

	if (TypeIsPointer(children[0].type.value()))
	{
		switch (expression.kind)
		{
			case ExpressionKind::PLUS_OP:
				break;
			case ExpressionKind::MINUS_OP:
				rightOperand = _builder->CreateNeg(rightOperand);
				break;
			default:
				throw std::runtime_error(__PRETTY_FUNCTION__);
		}
		return _builder->CreateGEP(leftOperand->getType()->getPointerElementType(), leftOperand, rightOperand);
	}
	else
	{
		const auto type = children[0].type.value();
		switch (expression.kind)
		{
			case ExpressionKind::PLUS_OP:
				return _builder->CreateAdd(leftOperand, rightOperand);
			case ExpressionKind::MINUS_OP:
				return _builder->CreateSub(leftOperand, rightOperand);
			case ExpressionKind::DIVIDE_OP:
				if (YepLang::TypeIsSigned(type))
				{
					return _builder->CreateSDiv(leftOperand, rightOperand);
				}
				else
				{
					return _builder->CreateUDiv(leftOperand, rightOperand);
				}
			case ExpressionKind::MULTIPLY_OP:
				return _builder->CreateMul(leftOperand, rightOperand);
			default:
				throw std::runtime_error(__PRETTY_FUNCTION__);
		}
	}
}

void YepLang::Generator::Dump() const
{
//	_module->print(llvm::outs(), nullptr);
	std::error_code ec;
	auto stream = llvm::raw_fd_stream("main.ll", ec);
	_module->print(stream, nullptr);
}

llvm::Type *YepLang::Generator::CodegenType(const Type &type)
{
	if (TypeIsBuiltin(type))
	{
		switch (std::get<YepLang::BuiltinType>(type).type)
		{
			case BuiltinTypeEnum::i32:
				return llvm::Type::getInt32Ty(*_context);
			case BuiltinTypeEnum::i64:
			case BuiltinTypeEnum::u64:
				return llvm::Type::getInt64Ty(*_context);
			case BuiltinTypeEnum::character:
				return llvm::Type::getInt8Ty(*_context);
			case BuiltinTypeEnum::boolean:
				return llvm::Type::getInt1Ty(*_context);
			case BuiltinTypeEnum::void_t:
				return llvm::Type::getVoidTy(*_context);
			default:
				throw std::runtime_error(__PRETTY_FUNCTION__);
		}
	}
	else if (TypeIsArray(type))
	{
		const auto &array = std::get<YepLang::Array>(type);
		auto arrayType = CodegenType(*array.elementType);
		return llvm::ArrayType::get(arrayType, array.size);
	}
	else if (TypeIsStruct(type))
	{
		const auto &structDef = std::get<YepLang::Struct>(type);
		std::vector<llvm::Type *> fieldsTypes;
		for (const auto &field: structDef.fields)
		{
			fieldsTypes.emplace_back(CodegenType(*field.fieldType));
		}

		return llvm::StructType::get(*_context, fieldsTypes);
	}
	else if (TypeIsPointer(type))
	{
		auto pointeeType = CodegenType(*std::get<YepLang::Pointer>(type).pointedType);
		return llvm::PointerType::get(pointeeType, 0);
	}
	else
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}
}

llvm::Value *YepLang::Generator::CodegenBinaryRelationalOp(const YepLang::Expression &expression)
{
	const auto children = std::get<ExpressionChildren>(expression.value);
	const auto leftOperand = CodegenExpression(children[0]);
	const auto rightOperand = CodegenExpression(children[1]);

	switch (expression.kind)
	{
		case ExpressionKind::LESS_THAN_OP:
			return _builder->CreateICmpSLT(leftOperand, rightOperand);
		case ExpressionKind::GREATER_THAN_OP:
			return _builder->CreateICmpSGT(leftOperand, rightOperand);
		case ExpressionKind::EQUAL_OP:
			return _builder->CreateICmpEQ(leftOperand, rightOperand);
		case ExpressionKind::NOT_EQUAL_OP:
			return _builder->CreateICmpNE(leftOperand, rightOperand);
		default:
			throw std::runtime_error(__PRETTY_FUNCTION__);
	}
}

void YepLang::Generator::CodegenConditional(const YepLang::Expression &expression)
{
	const auto children = std::get<ExpressionChildren>(expression.value);


	auto function = _builder->GetInsertBlock()->getParent();
	const auto hasElse = ((children.size() % 2) == 1);
	const auto conditionalBranches = ((hasElse) ? children.size() - 1 : children.size()) / 2;

	std::vector<llvm::BasicBlock *> blocks;
	blocks.push_back(_builder->GetInsertBlock());
	for (auto i = 0U; i < children.size(); ++i)
	{
		blocks.push_back(llvm::BasicBlock::Create(*_context, NextLabel().second, function));
	}
	// Generate conditional branches
	auto ix = 0U;

	for (auto i = 0U; i < conditionalBranches; ++i)
	{
		_builder->SetInsertPoint(blocks[ix]);
		const auto conditionalExpression = CodegenExpression(children[ix]);
		_builder->CreateCondBr(conditionalExpression, blocks[ix + 1], blocks[ix + 2]);

		_builder->SetInsertPoint(blocks[ix + 1]);
		auto terminating = CodegenScope(children[ix + 1]);
		if (!terminating)
		{
			_builder->CreateBr(blocks.back());
		}

		ix += 2;
	}

	// generate else branch
	if (hasElse)
	{
		_builder->SetInsertPoint(blocks[ix]);
		auto terminating = CodegenScope(children.back());
		if (!terminating)
		{
			_builder->CreateBr(blocks.back());
		}
		++ix;
	}

	_builder->SetInsertPoint(blocks[ix]);
}

std::pair<std::uint64_t, std::string> YepLang::Generator::NextLabel()
{
	std::string label = ".L";
	label += std::to_string(_labelCounter);

	return {_labelCounter++, std::move(label)};
}

llvm::Value *YepLang::Generator::CodegenVariable(const YepLang::Expression &expression)
{
	const auto identifier = std::get<std::string>(expression.value);

	auto variable = FindGeneratorVariable(identifier);

	if (variable.has_value())
	{
		const auto &var = variable.value();
		return _builder->CreateLoad(var.type, var.address, identifier);
	}
	else
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
}

void YepLang::Generator::CodegenVariableDeclaration(const YepLang::Expression &expression)
{
	auto nameExpr = std::get<YepLang::ExpressionChildren>(expression.value).at(0);
	auto name = std::get<std::string>(nameExpr.value);
	auto valueExpression = std::get<YepLang::ExpressionChildren>(expression.value).at(1);
	auto type = CodegenType(valueExpression.type.value());


	GeneratorVariable variable
			{
					type,
					nullptr
			};
	auto value = CodegenExpression(valueExpression);

	if (YepLang::TypeIsArray(valueExpression.type.value()))
	{
		value->setName(name);
		variable.address = value;
	}
	else
	{
		auto address = AllocaVariable(name, type);
		if (type->isStructTy())
		{
			_builder->CreateMemCpy(address, llvm::MaybeAlign(), value, llvm::MaybeAlign(), _module->getDataLayout().getTypeAllocSize(type));
		}
		else
		{
			_builder->CreateStore(value, address);
		}
		variable.address = address;
	}
	_scopes.back().variables.emplace(name, variable);
}

llvm::AllocaInst *YepLang::Generator::AllocaVariable(const std::string &name, llvm::Type *type)
{
	llvm::IRBuilder<llvm::NoFolder> allocaBuilder(&_currentFunction->getEntryBlock(),
	                                              _currentFunction->getEntryBlock().begin());
	return allocaBuilder.CreateAlloca(type, nullptr, name);
}

llvm::Value *YepLang::Generator::CodegenAssignment(const YepLang::Expression &expression)
{
	auto children = std::get<YepLang::ExpressionChildren>(expression.value);

	auto &LHS = children.at(0);
	auto &RHS = children.at(1);

	auto value = CodegenExpression(RHS);

	if (LHS.kind == ExpressionKind::VARIABLE)
	{
		auto nameExpr = std::get<YepLang::ExpressionChildren>(expression.value).at(0);
		auto name = std::get<std::string>(nameExpr.value);

		auto variable = FindGeneratorVariable(name);;
		if (!variable.has_value())
		{
			throw std::invalid_argument(__PRETTY_FUNCTION__);
		}

		if (variable.value().type->isStructTy())
		{
			const auto address = variable.value().address;
			_builder->CreateMemCpy(value, llvm::MaybeAlign(), address, llvm::MaybeAlign(), _module->getDataLayout().getTypeAllocSize(variable->type));
		}
		else
		{
			const auto address = variable.value().address;
			_builder->CreateStore(value, address);
		}
	}
	else if (LHS.kind == ExpressionKind::MEMBER_ACCESS)
	{
		auto nameExpr = std::get<YepLang::ExpressionChildren>(LHS.value).at(0);
		auto name = std::get<std::string>(nameExpr.value);

		auto variable = FindGeneratorVariable(name);;
		if (!variable.has_value())
		{
			throw std::invalid_argument(__PRETTY_FUNCTION__);
		}

		auto fieldExpr = std::get<YepLang::ExpressionChildren>(LHS.value).at(1);
		auto field = std::get<std::string>(fieldExpr.value);

		// todo find actual index of the field
		const auto target = _builder->CreateStructGEP(variable.value().type, variable.value().address, 0);
		_builder->CreateStore(value, target);
	}
	else
	{
		// Pointer deref
		auto LHSChildren = std::get<YepLang::ExpressionChildren>(LHS.value);
		const auto address = CodegenExpression(LHSChildren.at(0));
		_builder->CreateStore(value, address);
	}
	return value;
}

std::optional<YepLang::GeneratorVariable> YepLang::Generator::FindGeneratorVariable(const std::string &variable)
{
	const auto it = std::find_if(_scopes.rbegin(), _scopes.rend(), [&variable](const GeneratorScope &scope)
	{
		return scope.variables.contains(variable);
	});

	if (it != _scopes.rend())
	{
		return it->variables.at(variable);
	}
	else
	{
		return std::nullopt;
	}
}

llvm::Value *YepLang::Generator::CodegenPostIncrement(const YepLang::Expression &expression)
{
	const auto &variableExpression = std::get<YepLang::ExpressionChildren>(expression.value).front();

	const auto identifier = std::get<std::string>(variableExpression.value);

	auto variable = FindGeneratorVariable(identifier);

	if (variable.has_value())
	{
		if (variable->type->isPointerTy())
		{
			const auto &var = variable.value();
			auto load = _builder->CreateLoad(var.type, var.address, identifier);

			llvm::Value *idx = llvm::ConstantInt::get(*_context, llvm::APInt(64, 1));
			auto offsetPtr = _builder->CreateGEP(var.type->getPointerElementType(), load, idx);
			_builder->CreateStore(offsetPtr, var.address);
			return load;
		}
		else
		{
			const auto &var = variable.value();
			auto load = _builder->CreateLoad(var.type, var.address, identifier);

			//todo correct type
			auto one = llvm::ConstantInt::get(*_context, llvm::APInt(64, 1));
			auto add = _builder->CreateAdd(load, one);
			_builder->CreateStore(add, var.address);
			return load;
		}
	}
	else
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}
}

void YepLang::Generator::CodegenForLoop(const YepLang::Expression &expression)
{
	_scopes.emplace_back();
	auto init = std::get<YepLang::ExpressionChildren>(expression.value).at(0);
	if (init.kind == ExpressionKind::VARIABLE_DECLARATION)
	{
		CodegenVariableDeclaration(init);
	}
	else
	{
		CodegenExpression(init);
	}

	auto function = _builder->GetInsertBlock()->getParent();

	auto bodyBlock = llvm::BasicBlock::Create(*_context, NextLabel().second, function);
	auto changeBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);
	auto conditionBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);
	auto afterBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);

	_constructs.push_back(
			GeneratorConstruct{
					GeneratorConstructType::ForLoop,
					{
							GeneratorConstruct::ForConstruct{
									changeBlock,
									afterBlock
							}
					}
			}
	);

	_builder->CreateBr(conditionBlock);
	_builder->SetInsertPoint(bodyBlock);
	auto body = std::get<YepLang::ExpressionChildren>(expression.value).at(3);
	CodegenScope(body);
	_builder->CreateBr(changeBlock);

	function->getBasicBlockList().push_back(changeBlock);
	_builder->SetInsertPoint(changeBlock);
	auto change = std::get<YepLang::ExpressionChildren>(expression.value).at(2);
	CodegenExpression(change);
	_builder->CreateBr(conditionBlock);

	function->getBasicBlockList().push_back(conditionBlock);
	_builder->SetInsertPoint(conditionBlock);
	auto condition = std::get<YepLang::ExpressionChildren>(expression.value).at(1);
	auto conditionExpression = CodegenExpression(condition);

	function->getBasicBlockList().push_back(afterBlock);
	_builder->CreateCondBr(conditionExpression, bodyBlock, afterBlock);

	_builder->SetInsertPoint(afterBlock);
	_scopes.pop_back();
	_constructs.pop_back();
}

void YepLang::Generator::CodegenContinue()
{
	if (_constructs.empty())
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}
	const auto &construct = _constructs.back();

	if (construct.type != GeneratorConstructType::ForLoop)
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}

	auto continueBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);

	_builder->CreateBr(continueBlock);

	auto function = _builder->GetInsertBlock()->getParent();

	function->getBasicBlockList().push_back(continueBlock);
	_builder->SetInsertPoint(continueBlock);
	_builder->CreateBr(construct.forConstruct.changeBlock);
}

void YepLang::Generator::CodegenBreak()
{
	if (_constructs.empty())
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}
	const auto &construct = _constructs.back();

	if (construct.type != GeneratorConstructType::ForLoop)
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}

	auto breakBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);
	_builder->CreateBr(breakBlock);

	auto function = _builder->GetInsertBlock()->getParent();

	function->getBasicBlockList().push_back(breakBlock);
	_builder->SetInsertPoint(breakBlock);
	_builder->CreateBr(construct.forConstruct.afterBlock);
}

llvm::Value *YepLang::Generator::CodegenFunctionCall(const YepLang::Expression &expression)
{
	const auto &children = std::get<YepLang::ExpressionChildren>(expression.value);
	const auto &name = std::get<std::string>(children.front().value);

	auto func = _module->getFunction(name);
	std::vector<llvm::Value *> args{};
	for (auto it = std::next(children.begin()); it != children.end(); ++it)
	{
		args.push_back(CodegenExpression(*it));
	}

	return _builder->CreateCall(func, args);
}

void YepLang::Generator::GenerateExternFunction(const YepLang::FunctionPrototype &function)
{
	std::vector<llvm::Type *> args{};
	args.reserve(function.arguments.size());
	for (const auto &arg: function.arguments)
	{
		args.push_back(CodegenType(arg.type));
	}

	const auto prototype = llvm::FunctionType::get(CodegenType(function.returnType), args, false);

	const auto func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage, function.name, _module.get());

	auto argsIt = function.arguments.begin();
	auto funcArgsIt = func->arg_begin();
	while (argsIt != function.arguments.end() && funcArgsIt != func->arg_end())
	{
		funcArgsIt->setName(argsIt->name);
		std::advance(argsIt, 1);
		std::advance(funcArgsIt, 1);
	}
}

llvm::Value *YepLang::Generator::CodegenBinaryLogicalOp(const YepLang::Expression &expression)
{
	auto function = _builder->GetInsertBlock()->getParent();

	auto resType = CodegenType(expression.type.value());
	auto res = AllocaVariable("", resType);

	if (expression.kind == ExpressionKind::LOGICAL_AND)
	{
		const auto children = std::get<ExpressionChildren>(expression.value);
		const auto leftOperand = CodegenExpression(children[0]);

		auto trueBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);
		auto afterBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);

		_builder->CreateStore(leftOperand, res);
		_builder->CreateCondBr(leftOperand, trueBlock, afterBlock);

		function->getBasicBlockList().push_back(trueBlock);
		function->getBasicBlockList().push_back(afterBlock);

		_builder->SetInsertPoint(trueBlock);
		const auto rightOperand = CodegenExpression(children[1]);
		_builder->CreateStore(rightOperand, res);
		_builder->CreateBr(afterBlock);
		_builder->SetInsertPoint(afterBlock);
	}
	else if (expression.kind == ExpressionKind::LOGICAL_OR)
	{
		const auto children = std::get<ExpressionChildren>(expression.value);
		const auto leftOperand = CodegenExpression(children[0]);

		auto falseBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);
		auto afterBlock = llvm::BasicBlock::Create(*_context, NextLabel().second);

		_builder->CreateStore(leftOperand, res);
		_builder->CreateCondBr(leftOperand, afterBlock, falseBlock);

		function->getBasicBlockList().push_back(falseBlock);
		function->getBasicBlockList().push_back(afterBlock);

		_builder->SetInsertPoint(falseBlock);
		const auto rightOperand = CodegenExpression(children[1]);
		_builder->CreateStore(rightOperand, res);
		_builder->CreateBr(afterBlock);
		_builder->SetInsertPoint(afterBlock);
	}
	else
	{
		throw std::runtime_error(__PRETTY_FUNCTION__);
	}

	return _builder->CreateLoad(resType, res);
}

llvm::Value *YepLang::Generator::CodegenPointerDereference(const Expression &expression)
{
	const auto children = std::get<ExpressionChildren>(expression.value);
	const auto pointer = CodegenExpression(children[0]);

	auto resType = CodegenType(expression.type.value());
	return _builder->CreateLoad(resType, pointer);
}

llvm::Value *YepLang::Generator::CodegenArraySubscript(const YepLang::Expression &expression)
{
	const auto children = std::get<ExpressionChildren>(expression.value);
	if (YepLang::TypeIsPointer(children[0].type.value()))
	{
		const auto pointer = CodegenExpression(children[0]);
		const auto index = CodegenExpression(children[1]);

		auto resType = CodegenType(expression.type.value());

		auto offsetPtr = _builder->CreateGEP(resType, pointer, index);
		return _builder->CreateLoad(resType, offsetPtr);
	}
	else
	{
		const auto identifier = std::get<std::string>(children[0].value);
		auto variable = FindGeneratorVariable(identifier);

		if (variable.has_value())
		{
			const auto &var = variable.value();
			const auto index = CodegenExpression(children[1]);

			auto arrayType = CodegenType(children[0].type.value());
			auto resType = CodegenType(expression.type.value());

			auto zero = llvm::ConstantInt::get(*_context, llvm::APInt(64, 0));
			auto offsetPtr = _builder->CreateGEP(arrayType, var.address, {zero, index});
			return _builder->CreateLoad(resType, offsetPtr);
		}
		else
		{
			throw std::invalid_argument(__PRETTY_FUNCTION__);
		}
	}
}

llvm::Value *YepLang::Generator::CodegenAddressOf(const YepLang::Expression &expression)
{
	const auto children = std::get<ExpressionChildren>(expression.value);
	const auto operand = children[0];

	const auto &name = std::get<std::string>(operand.value);
	auto variable = FindGeneratorVariable(name);

	if (variable.has_value())
	{
		return variable.value().address;
	}
	else
	{
		throw std::invalid_argument(__PRETTY_FUNCTION__);
	}

}

llvm::Value *YepLang::Generator::CodegenMemberAccess(const YepLang::Expression &expression)
{

	auto children = std::get<ExpressionChildren>(expression.value);
	{
		if (children[0].kind == ExpressionKind::VARIABLE)
		{
			const auto identifier = std::get<std::string>(children[0].value);
			auto variable = FindGeneratorVariable(identifier);
			auto baseAddress = variable.value().address;

			auto baseType = std::get<YepLang::Struct>(children[0].type.value());
			auto baseTypeCodegen = CodegenType(children[0].type.value());

			const auto field = std::get<std::string>(children[1].value);
			auto resType = CodegenType(expression.type.value());

			for (auto i = 0U; i < baseType.fields.size(); ++i)
			{
				if (baseType.fields[i].name == field)
				{
					auto offsetPtr = _builder->CreateStructGEP(baseTypeCodegen, baseAddress, i);
					return _builder->CreateLoad(resType, offsetPtr);
				}
			}
			throw std::invalid_argument(__PRETTY_FUNCTION__);

		}
		else if (children[0].kind == ExpressionKind::ARRAY_SUBSCRIPT)
		{
			if (YepLang::TypeIsPointer(children[0].type.value()))
			{
//				const auto pointer = CodegenExpression(children[0]);
//				const auto index = CodegenExpression(children[1]);
//
//				auto resType = CodegenType(expression.type.value());
//
//				auto offsetPtr = _builder->CreateGEP(resType, pointer, index);
//				return _builder->CreateLoad(resType, offsetPtr);
				throw std::invalid_argument(__PRETTY_FUNCTION__);
			}
			else
			{
				auto arraySubscript = YepLang::ExpressionGet<ExpressionChildren>(children[0]);

				const auto identifier = std::get<std::string>(arraySubscript[0].value);
				auto variable = FindGeneratorVariable(identifier);

				if (variable.has_value())
				{
					const auto &var = variable.value();
					const auto index = CodegenExpression(arraySubscript[1]);

					auto arrayType = CodegenType(arraySubscript[0].type.value());
					auto resType = CodegenType(children[0].type.value());
					resType->getScalarType()->print(llvm::outs());

					auto zero = llvm::ConstantInt::get(*_context, llvm::APInt(64, 0));

					const auto &baseType = std::get<YepLang::Struct>(children[0].type.value());
					auto baseTypeCodegen = CodegenType(children[0].type.value());

					const auto field = std::get<std::string>(children[1].value);

					for (auto i = 0U; i < baseType.fields.size(); ++i)
					{
						if (baseType.fields[i].name == field)
						{
							auto offsetPtr = _builder->CreateGEP(arrayType, var.address, {zero, index});
							auto fieldPtr = _builder->CreateStructGEP(resType, offsetPtr, i);
							return _builder->CreateLoad(baseTypeCodegen, fieldPtr);
						}
					}
					throw std::invalid_argument(__PRETTY_FUNCTION__);
				}
				else
				{
					throw std::invalid_argument(__PRETTY_FUNCTION__);
				}
			}
		}
		else
		{
			throw std::invalid_argument(__PRETTY_FUNCTION__);
		}
	}
}


