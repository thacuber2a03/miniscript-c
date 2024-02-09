#include <stdlib.h>
#include <stdio.h>

#include "ms_compiler.h"
#include "ms_scanner.h"
#include "ms_value.h"
#include "ms_code.h"

#ifdef MS_DEBUG_PRINT_CODE
#include "ms_debug.h"
#endif

typedef struct {
	ms_Scanner scanner;
	ms_VM *vm;
	ms_Token previous, current;
	ms_Code *currentCode;
	bool hadError;
} ms_Compiler;

typedef void (*ParseFn)(ms_Compiler *compiler);

typedef enum {
	PREC_NONE,
	PREC_FUNCTION,
	PREC_OR,
	PREC_AND,
	PREC_NOT,
	PREC_ISA,
	PREC_COMPARISON, // > < >= <=
	PREC_TERM,       // + -
	PREC_FACTOR,     // * / %
	PREC_UNARY,      // -
	PREC_NEW,
	PREC_POWER,
	PREC_CALL,
	PREC_MAP,
	PREC_LIST,
	PREC_QUANTITY,
	PREC_ATOM,
} ParsePrecedence;

typedef struct {
	ParseFn prefix, infix;
	ParsePrecedence precedence;
} ParseRule;

static void initCompiler
	(ms_Compiler *compiler, ms_VM *vm, ms_Scanner scanner, ms_Code *mainCode)
{
	compiler->hadError = false;
	compiler->scanner = scanner;
	compiler->currentCode = mainCode;
	compiler->vm = vm;
}

static void errorAt(ms_Compiler *compiler, ms_Token *token, const char* message)
{
	fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == MS_TOK_EOF)
    fprintf(stderr, " at end of file");
  else if (token->type != MS_TOK_ERROR)
    fprintf(stderr, " at '%.*s'", token->length, token->start);

  fprintf(stderr, ": %s\n", message);
  compiler->hadError = true;
}

static void error(ms_Compiler *compiler, const char *message) { errorAt(compiler, &compiler->previous, message); }
static void errorAtCurrent(ms_Compiler *compiler, const char *message) { errorAt(compiler, &compiler->current, message); }

static void advance(ms_Compiler *compiler)
{
	compiler->previous = compiler->current;
	for (;;)
	{
		compiler->current = ms_nextToken(&compiler->scanner);
		if (compiler->current.type != MS_TOK_ERROR) break;

		errorAtCurrent(compiler, compiler->current.start);
	}
}

static void consume(ms_Compiler *compiler, ms_TokenType type, const char *message)
{
	if (compiler->current.type == type)
	{
		advance(compiler);
		return;
	}

	errorAtCurrent(compiler, message);
}

static void emitByte(ms_Compiler *compiler, uint8_t byte)
{
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte);
}

static void emitBytes(ms_Compiler *compiler, uint8_t byte1, uint8_t byte2)
{
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte1);
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte2);
}

static void emitReturn(ms_Compiler *compiler)
{
	emitByte(compiler, MS_OP_RETURN);
}

static uint8_t makeConstant(ms_Compiler *compiler, ms_Value value)
{
	// TODO: make the operand a 16 bit index
	int constant = ms_addConstToCode(compiler->vm, compiler->currentCode, value);
	if (constant > UINT8_MAX)
	{
		error(compiler, "Too many constants in one chunk.");
		return 0;
	}
	return (uint8_t)constant;
}

static void emitConstant(ms_Compiler *compiler, ms_Value value)
{
	emitBytes(compiler, MS_OP_CONST, makeConstant(compiler, value));
}

static void endCompiler(ms_Compiler *compiler)
{
	emitReturn(compiler);
#ifdef MS_DEBUG_PRINT_CODE
	if (!compiler->hadError)
		ms_disassembleCode(compiler->currentCode, "code");
#endif
}

////////////////////////////

static void expression();
static ParseRule *getRule(ms_TokenType type);
static void parsePrecedence(ms_Compiler* compiler, ParsePrecedence precedence);

static void binary(ms_Compiler *compiler)
{
	ms_TokenType operatorType = compiler->previous.type;
	ParseRule *rule = getRule(operatorType);
	parsePrecedence(compiler, (ParsePrecedence)(rule->precedence + 1));

	switch (operatorType)
	{
		case MS_TOK_PLUS:    emitByte(compiler, MS_OP_ADD);      break;
		case MS_TOK_MINUS:   emitByte(compiler, MS_OP_SUBTRACT); break;
		case MS_TOK_STAR:    emitByte(compiler, MS_OP_MULTIPLY); break;
		case MS_TOK_SLASH:   emitByte(compiler, MS_OP_DIVIDE);   break;
		case MS_TOK_PERCENT: emitByte(compiler, MS_OP_MODULO);   break;
		case MS_TOK_CARET:   emitByte(compiler, MS_OP_POWER);    break;
		default: return; // unreachable
	}
}

static void grouping(ms_Compiler *compiler)
{
	expression(compiler);
	consume(compiler, MS_TOK_RPAREN, "Expected ')' after expression");
}

static void number(ms_Compiler *compiler)
{
	double value = strtod(compiler->previous.start, NULL);
	emitConstant(compiler, MS_FROM_NUM(value));
}

static void unary(ms_Compiler *compiler)
{
	parsePrecedence(compiler, PREC_UNARY);
	emitByte(compiler, MS_OP_NEGATE);
}

ParseRule rules[MS_TOK__END] = {
	[MS_TOK_PLUS]    = {NULL,     binary, PREC_TERM  },
	[MS_TOK_MINUS]   = {NULL,     binary, PREC_TERM  },
	[MS_TOK_STAR]    = {NULL,     binary, PREC_FACTOR},
	[MS_TOK_SLASH]   = {NULL,     binary, PREC_FACTOR},
	[MS_TOK_PERCENT] = {NULL,     binary, PREC_FACTOR},
	[MS_TOK_CARET]   = {NULL,     binary, PREC_POWER },
	[MS_TOK_NUM]     = {number,   NULL,   PREC_NONE  },
};

static void parsePrecedence(ms_Compiler *compiler, ParsePrecedence precedence)
{
	advance(compiler);
	ParseFn prefixRule = getRule(compiler->previous.type)->prefix;
	if (prefixRule == NULL)
	{
		error(compiler, "Expected an expression.");
		return;
	}

	prefixRule(compiler);

	while (precedence <= getRule(compiler->current.type)->precedence)
	{
		advance(compiler);
		ParseFn infixRule = getRule(compiler->previous.type)->infix;
		infixRule(compiler);
	}
}

static ParseRule *getRule(ms_TokenType type) { return &rules[type]; }

static void expression(ms_Compiler *compiler)
{
	parsePrecedence(compiler, PREC_FUNCTION);
}

////////////////////////////

ms_InterpretResult ms_compileString(ms_VM* vm, char *source, ms_Code *code)
{
	ms_Scanner scanner;
	ms_initScanner(&scanner, source);

	ms_Compiler compiler;
	initCompiler(&compiler, vm, scanner, code);

	advance(&compiler);
	expression(&compiler);
	consume(&compiler, MS_TOK_EOF, "Expected end of expression");

	endCompiler(&compiler);

	return compiler.hadError ? MS_INTERPRET_COMPILE_ERROR : MS_INTERPRET_OK;
}
