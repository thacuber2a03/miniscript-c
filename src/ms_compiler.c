#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ms_compiler.h"
#include "ms_object.h"
#include "ms_scanner.h"
#include "ms_value.h"
#include "ms_code.h"
#include "ms_mem.h"

#ifdef MS_DEBUG_PRINT_CODE
#include "ms_debug.h"
#endif

typedef struct ms_Compiler ms_Compiler;

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
	PREC_ADDRESS,
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

typedef struct {
	ms_Token name;
	int depth;
} Local;

typedef enum {
	TYPE_FUNCTION,
	TYPE_SCRIPT,
} FunctionType;

typedef struct Record {
	struct Record *enclosing;
	ms_ObjFunction *function;
	FunctionType type;
	Local locals[UINT8_COUNT];
	int localCount, scopeDepth;
} Record;

struct ms_Compiler {
	ms_Scanner scanner;
	ms_VM *vm;
	ms_Token previous, current;
	ms_Code *currentCode;
	Record *currentRecord;
	bool hadError;
};

static void initCompiler
	(ms_Compiler *compiler, ms_VM *vm, ms_Scanner scanner)
{
	compiler->hadError = false;
	compiler->scanner = scanner;
	compiler->vm = vm;
	compiler->currentRecord = NULL;
}

static void initRecord(ms_Compiler *compiler, Record *rec, FunctionType type)
{
	rec->enclosing = compiler->currentRecord;
	rec->function = NULL;
	rec->type = type;
	rec->localCount = 0;
	rec->scopeDepth = 0;
	rec->function = ms_newFunction(compiler->vm);

	compiler->currentRecord = rec;
	compiler->currentCode = &rec->function->code;

	Local *local = &rec->locals[rec->localCount++];
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
}

static void advance(ms_Compiler *compiler);

static void errorAt(ms_Compiler *compiler, ms_Token *token, const char* message)
{
	if (compiler->hadError) return;
	fprintf(stderr, "Compiler Error: %s [line %u]\n", message, token->line);
	advance(compiler);
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

static bool vcheck(ms_Compiler *compiler, int n, ...)
{
	va_list arg;
	va_start(arg, n);

	for (int i = 0; i < n; i++)
		if (compiler->current.type == va_arg(arg, ms_TokenType))
			return true;

	va_end(arg);
	return false;
}

#define check(compiler, type) vcheck(compiler, 1, type)

static bool match(ms_Compiler *compiler, ms_TokenType type)
{
	if (!check(compiler, type)) return false;
	advance(compiler);
	return true;
}

static inline void skipNewlines(ms_Compiler *compiler)
{
	// the power of side effects
	while (match(compiler, MS_TOK_NEWLINE));
}

static inline bool checkKeyword(ms_Compiler *compiler)
{
	return compiler->current.type > MS_TOK__KEYWORD_START
	    && compiler->current.type < MS_TOK__KEYWORD_END;
}

static void emitByte(ms_Compiler *compiler, uint8_t byte)
{
	ms_addByteToCode(
		compiler->vm, compiler->currentCode,
		byte, compiler->current.line
	);
}

static void emitBytes(ms_Compiler *compiler, uint8_t byte1, uint8_t byte2)
{
	int line = compiler->current.line;
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte1, line);
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte2, line);
}

static void emitLoop(ms_Compiler *compiler, int loopStart)
{
	emitByte(compiler, MS_OP_LOOP);

	int offset = compiler->currentCode->count - loopStart + 2;
	if (offset > UINT16_MAX) error(compiler, "Loop body too large");

	emitByte(compiler, (offset >> 8) & 0xff);
	emitByte(compiler,  offset       & 0xff);
}

static size_t emitJump(ms_Compiler *compiler, uint8_t instruction)
{
	emitByte(compiler, instruction);
	emitByte(compiler, 0xff);
	emitByte(compiler, 0xff);
	return compiler->currentCode->count - 2;
}

static void emitReturn(ms_Compiler *compiler)
{
	emitBytes(compiler, MS_OP_NULL, MS_OP_RETURN);
}

static uint8_t makeConstant(ms_Compiler *compiler, ms_Value value)
{
	// TODO: make the operand a 16 bit index
	size_t constant = ms_addConstToCode(compiler->vm, compiler->currentCode, value);
	if (constant > UINT8_MAX)
	{
		error(compiler, "Too many constants in one chunk");
		return 0;
	}
	return (uint8_t)constant;
}

static void emitConstant(ms_Compiler *compiler, ms_Value value)
{
	emitBytes(compiler, MS_OP_CONST, makeConstant(compiler, value));
}

static void patchJump(ms_Compiler *compiler, size_t offset)
{
	size_t jump = compiler->currentCode->count - offset - 2;
	if (jump > UINT16_MAX) error(compiler, "Too much jump to code over");

	compiler->currentCode->data[offset] = (jump >> 8) & 0xff;
	compiler->currentCode->data[offset + 1] = jump & 0xff;
}

static ms_ObjFunction *endCompiler(ms_Compiler *compiler)
{
	emitReturn(compiler);
	ms_ObjFunction *function = compiler->currentRecord->function;

#ifdef MS_DEBUG_PRINT_CODE
	if (!compiler->hadError)
	{
		fprintf(stderr, "compiler: code disassembly:\n");
		ms_disassembleCode(compiler->currentCode, "code");
	}
#endif

	compiler->currentRecord = compiler->currentRecord->enclosing;
	if (compiler->currentRecord != NULL)
		compiler->currentCode = &compiler->currentRecord->function->code;
	return function;
}

static void beginScope(ms_Compiler *compiler)
{
	compiler->currentRecord->scopeDepth++;
}

static void endScope(ms_Compiler *compiler)
{
	Record *rec = compiler->currentRecord;
	rec->scopeDepth--;

	while (rec->localCount > 0
	   &&  rec->locals[rec->localCount - 1].depth > rec->scopeDepth)
	{
		emitByte(compiler, MS_OP_POP);
		rec->localCount--;
	}
}

////////////////////////////

#define block(compiler, ...) do {                                               \
  beginScope(compiler);                                                         \
  skipNewlines(compiler);                                                       \
  int numArgs = sizeof((ms_TokenType[]){__VA_ARGS__})/sizeof(ms_TokenType);     \
  while (!vcheck(compiler, numArgs+1, MS_TOK_EOF, __VA_ARGS__)) {               \
    statement(compiler);                                                        \
    skipNewlines(compiler);                                                     \
  }                                                                             \
  endScope(compiler);                                                           \
} while(0)

static void statement(ms_Compiler *compiler);
static void expression(ms_Compiler *compiler);
static ParseRule *getRule(ms_TokenType type);
static void parsePrecedence(ms_Compiler* compiler, ParsePrecedence precedence);

static ms_Value identifierObject(ms_Compiler *compiler, ms_Token *name)
{
	return MS_FROM_OBJ(ms_copyString(compiler->vm, name->start, name->length));
}

static inline uint8_t identifierConstant(ms_Compiler *compiler, ms_Token *name)
{
	return makeConstant(compiler, identifierObject(compiler, name));
}

static bool identifiersEqual(ms_Token* a, ms_Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(ms_Compiler *compiler, ms_Token *name)
{
	Record *rec = compiler->currentRecord;
	for (int i = rec->localCount - 1; i >= 0; i--)
	{
		Local* local = &rec->locals[i];
		if (identifiersEqual(name, &local->name))
			return i;
	}

	int idx = ms_findValueInList(&compiler->currentCode->constants, identifierObject(compiler, name));
	if (idx == -1) return -2; // undefined

	return -1; // global
}

static int addLocal(ms_Compiler *compiler, ms_Token name)
{
	Record *rec = compiler->currentRecord;
	if (rec->localCount == UINT8_COUNT)
	{
		error(compiler, "Too many local variables in one block");
		return -1;
	}

	size_t idx = rec->localCount++;
	Local *local = &rec->locals[idx];
	local->name = name;
	local->depth = rec->scopeDepth;
	return idx;
}

static void binary(ms_Compiler *compiler)
{
	ms_TokenType operatorType = compiler->previous.type;
	ParseRule *rule = getRule(operatorType);
	parsePrecedence(compiler, (ParsePrecedence)(rule->precedence + 1));

	switch (operatorType)
	{
		case MS_TOK_PLUS:    emitByte(compiler, MS_OP_ADD);           break;
		case MS_TOK_MINUS:   emitByte(compiler, MS_OP_SUBTRACT);      break;
		case MS_TOK_STAR:    emitByte(compiler, MS_OP_MULTIPLY);      break;
		case MS_TOK_SLASH:   emitByte(compiler, MS_OP_DIVIDE);        break;
		case MS_TOK_PERCENT: emitByte(compiler, MS_OP_MODULO);        break;
		case MS_TOK_CARET:   emitByte(compiler, MS_OP_POWER);         break;

		case MS_TOK_NEQ:     emitByte(compiler, MS_OP_NOT_EQUAL);     break;
		case MS_TOK_EQUAL:   emitByte(compiler, MS_OP_EQUAL);         break;
		case MS_TOK_LESS:    emitByte(compiler, MS_OP_LESS);          break;
		case MS_TOK_GREATER: emitByte(compiler, MS_OP_GREATER);       break;
		case MS_TOK_LEQ:     emitByte(compiler, MS_OP_LESS_EQUAL);    break;
		case MS_TOK_GEQ:     emitByte(compiler, MS_OP_GREATER_EQUAL); break;

		case MS_TOK_AND:     emitByte(compiler, MS_OP_AND);           break;
		case MS_TOK_OR:      emitByte(compiler, MS_OP_OR);            break;
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

static void string(ms_Compiler *compiler)
{
	char* str = MS_MEM_MALLOC_ARR(compiler->vm, char, compiler->previous.length);
	size_t realLen = 0;

	ms_Token tok = compiler->previous;
	for (int i = 1; i < tok.length-1; i++)
	{
		char c = tok.start[i];
		if (c == '"' && tok.start[i+1] == '"') i++;
		str[realLen] = c;
		realLen++;
	}

	str = MS_MEM_REALLOC_ARR(compiler->vm, char, str, tok.length, realLen + 1);
	str[realLen] = '\0';

	emitConstant(compiler, MS_FROM_OBJ(ms_newString(compiler->vm, str, realLen)));
}

static void variable(ms_Compiler *compiler)
{
	ms_TokenType prefix = compiler->previous.type;
	if (prefix == MS_TOK_AT_SIGN) advance(compiler);

	uint8_t get;
	int arg = resolveLocal(compiler, &compiler->previous);

	if (arg == -2) error(compiler, "Undefined variable");

	if (arg != -1)
		get = MS_OP_GET_LOCAL;
	else
	{
		arg = identifierConstant(compiler, &compiler->previous);
		get = MS_OP_GET_GLOBAL;
	}

	emitBytes(compiler, get, arg);

	if (prefix != MS_TOK_AT_SIGN)
		// TODO: arguments
		emitBytes(compiler, MS_OP_INVOKE, 0);
}

static void function(ms_Compiler *compiler)
{
	Record record;
	initRecord(compiler, &record, TYPE_FUNCTION);
	beginScope(compiler);

	// no params for now
	consume(compiler, MS_TOK_NEWLINE, "Expected newline after 'function'");

	block(compiler, MS_TOK_END_FUNC);

	consume(compiler, MS_TOK_END_FUNC, "Expected 'end function'");
	
	ms_ObjFunction *function = endCompiler(compiler);
	emitBytes(compiler, MS_OP_CONST, makeConstant(compiler, MS_FROM_OBJ(function)));
}

static void literal(ms_Compiler *compiler)
{
	ms_TokenType operatorType = compiler->previous.type;
	switch (operatorType)
	{
		case MS_TOK_NULL:  emitByte(compiler, MS_OP_NULL);  break;
		case MS_TOK_TRUE:  emitByte(compiler, MS_OP_TRUE);  break;
		case MS_TOK_FALSE: emitByte(compiler, MS_OP_FALSE); break;
		default: return; // unreachable
	}
}

static void unary(ms_Compiler *compiler)
{
	ms_TokenType operatorType = compiler->previous.type;
	parsePrecedence(compiler, PREC_UNARY);
	switch (operatorType)
	{
		case MS_TOK_MINUS: emitByte(compiler, MS_OP_NEGATE); break;
		case MS_TOK_NOT:   emitByte(compiler, MS_OP_NOT);    break;
		default: MS_UNREACHABLE("unary");
	}
}

ParseRule rules[MS_TOK__END] = {
	[MS_TOK_PLUS]    = {NULL,     binary, PREC_TERM      },
	[MS_TOK_MINUS]   = {unary,    binary, PREC_TERM      },
	[MS_TOK_STAR]    = {NULL,     binary, PREC_FACTOR    },
	[MS_TOK_SLASH]   = {NULL,     binary, PREC_FACTOR    },
	[MS_TOK_PERCENT] = {NULL,     binary, PREC_FACTOR    },
	[MS_TOK_CARET]   = {NULL,     binary, PREC_POWER     },

	[MS_TOK_EQUAL]   = {NULL,     binary, PREC_COMPARISON},
	[MS_TOK_NEQ]     = {NULL,     binary, PREC_COMPARISON},
	[MS_TOK_LESS]    = {NULL,     binary, PREC_COMPARISON},
	[MS_TOK_GREATER] = {NULL,     binary, PREC_COMPARISON},
	[MS_TOK_LEQ]     = {NULL,     binary, PREC_COMPARISON},
	[MS_TOK_GEQ]     = {NULL,     binary, PREC_COMPARISON},

	[MS_TOK_AND]     = {NULL,     binary, PREC_AND       },
	[MS_TOK_OR]      = {NULL,     binary, PREC_OR        },
	[MS_TOK_NOT]     = {unary,    NULL,   PREC_NONE      },

	[MS_TOK_AT_SIGN] = {variable, NULL,   PREC_NONE      },

	[MS_TOK_LPAREN]  = {grouping, NULL,   PREC_NONE      },

	[MS_TOK_TRUE]    = {literal,  NULL,   PREC_NONE      },
	[MS_TOK_FALSE]   = {literal,  NULL,   PREC_NONE      },
	[MS_TOK_NULL]    = {literal,  NULL,   PREC_NONE      },

	[MS_TOK_NUM]     = {number,   NULL,   PREC_NONE      },
	[MS_TOK_STR]     = {string,   NULL,   PREC_NONE      },
	[MS_TOK_ID]      = {variable, NULL,   PREC_NONE      },

	[MS_TOK_FUNC]    = {function, NULL,   PREC_FUNCTION  },
};

static void parsePrecedence(ms_Compiler *compiler, ParsePrecedence precedence)
{
	advance(compiler);
	ParseFn prefixRule = getRule(compiler->previous.type)->prefix;
	if (prefixRule == NULL)
	{
		error(compiler, "Expected an expression");
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

static void assignment(ms_Compiler *compiler)
{
	if (check(compiler, MS_TOK_ID))
	{
		advance(compiler);

		uint8_t set = MS_OP_SET_LOCAL;
		int arg = resolveLocal(compiler, &compiler->previous);
		if ((arg == -2 && compiler->currentRecord->scopeDepth == 0) || arg == -1)
		{
			arg = identifierConstant(compiler, &compiler->previous);
			set = MS_OP_SET_GLOBAL;
		}
		else
		{
			arg = -3;
			addLocal(compiler, compiler->previous);
		}

		consume(compiler, MS_TOK_ASSIGN, "Expected '=' after variable name");
		expression(compiler);
		consume(compiler, MS_TOK_NEWLINE, "Expected newline after expression");

		if (arg != -3) emitBytes(compiler, set, arg);
	}
	else errorAtCurrent(compiler, "Expected identifier");
}

static void ifStatement(ms_Compiler *compiler)
{
	expression(compiler);
	consume(compiler, MS_TOK_THEN, "Expected 'then' after condition");
	consume(compiler, MS_TOK_NEWLINE, "Expected newline after 'then'");

	size_t thenJump = emitJump(compiler, MS_OP_JUMP_IF_FALSE);
	emitByte(compiler, MS_OP_POP);

	block(compiler, MS_TOK_END_IF);

	consume(compiler, MS_TOK_END_IF, "Expected 'end if'");

	size_t endJump = emitJump(compiler, MS_OP_JUMP);
	patchJump(compiler, thenJump);
	emitByte(compiler, MS_OP_POP);
	patchJump(compiler, endJump);
}

static void statement(ms_Compiler *compiler)
{
	if (checkKeyword(compiler)
	&& !check(compiler, MS_TOK_NOT)
	&& !check(compiler, MS_TOK_TRUE)
	&& !check(compiler, MS_TOK_FALSE))
	{
		advance(compiler);
		switch (compiler->previous.type)
		{
			case MS_TOK_IF:
				ifStatement(compiler);
				break;

			case MS_TOK_WHILE: {
				int loopStart = compiler->currentCode->count;
				expression(compiler);
				consume(compiler, MS_TOK_NEWLINE, "Expected newline after expression");

				int exitJump = emitJump(compiler, MS_OP_JUMP_IF_FALSE);
				emitByte(compiler, MS_OP_POP);

				block(compiler, MS_TOK_END_WHILE);

				consume(compiler, MS_TOK_END_WHILE, "Expected 'end while'");

				emitLoop(compiler, loopStart);

				patchJump(compiler, exitJump);
				emitByte(compiler, MS_OP_POP);
			} break;

			case MS_TOK_RETURN: {
				if (match(compiler, MS_TOK_NEWLINE))
					emitReturn(compiler);
				else
				{
					expression(compiler);
					consume(compiler, MS_TOK_NEWLINE, "Expected newline after expression");
					emitByte(compiler, MS_OP_RETURN);
				}
			} break;

			default:
				MS_UNREACHABLE("statement");
		}
	}
	else
	{
		assignment(compiler);
	}	
}

static void program(ms_Compiler *compiler)
{
	skipNewlines(compiler);
	while (!match(compiler, MS_TOK_EOF))
	{
		statement(compiler);
		skipNewlines(compiler);
	}
}

ms_ObjFunction *ms_compileString(ms_VM* vm, char *source)
{
#ifdef MS_DEBUG_COMPILATION
	fprintf(stderr, "compiler: setting up compiler\n");
#endif
	ms_Scanner scanner;
	ms_initScanner(&scanner, source);
	ms_debugScanner(source);

	ms_Compiler compiler;
	initCompiler(&compiler, vm, scanner);

	Record rec;
	initRecord(&compiler, &rec, TYPE_SCRIPT);

	advance(&compiler);

#ifdef MS_DEBUG_COMPILATION
	fprintf(stderr, "compiler: set-up complete, starting compilation...\n");
#endif

	program(&compiler);

#ifdef MS_DEBUG_COMPILATION
	fprintf(stderr,
		"compiler: compilation finished %ssuccessfully\n",
		compiler.hadError ? "un" : ""
	);
#endif

	ms_ObjFunction *function = endCompiler(&compiler);
	return compiler.hadError ? NULL : function;
}
