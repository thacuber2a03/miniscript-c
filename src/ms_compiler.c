#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

typedef void (*ParseFn)(ms_Compiler *compiler, bool canAssign);

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
	size_t depth;
} Local;

typedef struct {
	Local locals[UINT8_COUNT];
	size_t localCount, scopeDepth;
} Record;

struct ms_Compiler {
	ms_Scanner scanner;
	ms_VM *vm;
	ms_Token previous, current;
	ms_Code *currentCode;
	Record *currentRecord;
	bool hadError, panicMode;
};

static void initCompiler
	(ms_Compiler *compiler, ms_VM *vm, ms_Scanner scanner, ms_Code *mainCode)
{
	compiler->hadError = compiler->panicMode = false;
	compiler->scanner = scanner;
	compiler->currentCode = mainCode;
	compiler->vm = vm;
}

static void initRecord(ms_Compiler *compiler, Record *rec)
{
	rec->localCount = 0;
	rec->scopeDepth = 0;
	compiler->currentRecord = rec;
}

static void errorAt(ms_Compiler *compiler, ms_Token *token, const char* message)
{
	if (compiler->panicMode) return;
	compiler->panicMode = true;

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

static inline bool check(ms_Compiler *compiler, ms_TokenType type)
{
	return compiler->current.type == type;
}

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
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte);
}

static void emitBytes(ms_Compiler *compiler, uint8_t byte1, uint8_t byte2)
{
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte1);
	ms_addByteToCode(compiler->vm, compiler->currentCode, byte2);
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
	emitByte(compiler, MS_OP_RETURN);
}

static uint8_t makeConstant(ms_Compiler *compiler, ms_Value value)
{
	// TODO: make the operand a 16 bit index
	size_t constant = ms_addConstToCode(compiler->vm, compiler->currentCode, value);
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

static void patchJump(ms_Compiler *compiler, size_t offset)
{
	size_t jump = compiler->currentCode->count - offset - 2;
	if (jump > UINT16_MAX)
	{
		error(compiler, "Too much jump to code over.");
	}

	compiler->currentCode->data[offset] = (jump >> 8) & 0xff;
	compiler->currentCode->data[offset + 1] = jump & 0xff;
}

static void endCompiler(ms_Compiler *compiler)
{
	emitReturn(compiler);
#ifdef MS_DEBUG_PRINT_CODE
	if (!compiler->hadError)
	{
		fprintf(stderr, "compiler: code disassembly:\n");
		ms_disassembleCode(compiler->currentCode, "code");
	}
#endif
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

static void expression(ms_Compiler *compiler);
static ParseRule *getRule(ms_TokenType type);
static void parsePrecedence(ms_Compiler* compiler, ParsePrecedence precedence);

static uint8_t identifierConstant(ms_Compiler *compiler, ms_Token *name)
{
	return makeConstant(compiler, MS_FROM_OBJ(ms_copyString(compiler->vm, name->start, name->length)));
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

	int idx = ms_findValueInList(&compiler->currentCode->constants, MS_FROM_OBJ(ms_copyString(compiler->vm, name->start, name->length)));
	if (idx == -1) return -2; // undefined

	return -1; // global
}

static int addLocal(ms_Compiler *compiler, ms_Token name)
{
	Record *rec = compiler->currentRecord;
	if (rec->localCount == UINT8_COUNT)
	{
		error(compiler, "Too many local variables in one block.");
		return -1;
	}

	size_t idx = rec->localCount++;
	Local *local = &rec->locals[idx];
	local->name = name;
	local->depth = rec->scopeDepth;
	return idx;
}

static void binary(ms_Compiler *compiler, bool canAssign)
{
	MS_UNUSED(canAssign);
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

static void grouping(ms_Compiler *compiler, bool canAssign)
{
	MS_UNUSED(canAssign);
	expression(compiler);
	consume(compiler, MS_TOK_RPAREN, "Expected ')' after expression");
}

static void number(ms_Compiler *compiler, bool canAssign)
{
	MS_UNUSED(canAssign);
	double value = strtod(compiler->previous.start, NULL);
	emitConstant(compiler, MS_FROM_NUM(value));
}

static void string(ms_Compiler *compiler, bool canAssign)
{
	MS_UNUSED(canAssign);
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

static void namedVariable(ms_Compiler *compiler, ms_Token name, bool canAssign)
{
	uint8_t get;
	int arg = resolveLocal(compiler, &name);
	if (arg == -2)
		error(compiler, "Undefined variable.");

	if (arg != -1)
		get = MS_OP_GET_LOCAL;
	else
	{
		arg = identifierConstant(compiler, &name);
		get = MS_OP_GET_GLOBAL;
	}

	emitBytes(compiler, get, arg);
}

static void variable(ms_Compiler *compiler, bool canAssign)
{
	ms_TokenType prefix = compiler->previous.type;
	if (prefix == MS_TOK_AT_SIGN) advance(compiler);
	namedVariable(compiler, compiler->previous, canAssign);
	if (prefix != MS_TOK_AT_SIGN) emitByte(compiler, MS_OP_INVOKE);
}

static void literal(ms_Compiler *compiler, bool canAssign)
{
	MS_UNUSED(canAssign);
	ms_TokenType operatorType = compiler->previous.type;
	switch (operatorType)
	{
		case MS_TOK_NULL:  emitByte(compiler, MS_OP_NULL);  break;
		case MS_TOK_TRUE:  emitByte(compiler, MS_OP_TRUE);  break;
		case MS_TOK_FALSE: emitByte(compiler, MS_OP_FALSE); break;
		default: return; // unreachable
	}
}

static void unary(ms_Compiler *compiler, bool canAssign)
{
	MS_UNUSED(canAssign);
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

	bool canAssign = precedence <= PREC_FUNCTION;
	prefixRule(compiler, canAssign);

	while (precedence <= getRule(compiler->current.type)->precedence)
	{
		advance(compiler);
		ParseFn infixRule = getRule(compiler->previous.type)->infix;
		infixRule(compiler, canAssign);
	}

	if (canAssign && match(compiler, MS_TOK_ASSIGN))
		error(compiler, "Invalid assignment target.");
}

static ParseRule *getRule(ms_TokenType type) { return &rules[type]; }

static void expression(ms_Compiler *compiler)
{
	parsePrecedence(compiler, PREC_FUNCTION);
}

////////////////////////////

static void synchronize(ms_Compiler *compiler)
{
	compiler->panicMode = false;

	while (compiler->current.type != MS_TOK_EOF)
	{
		if (compiler->previous.type == MS_TOK_NEWLINE) return;
		if (checkKeyword(compiler)) return;
		advance(compiler);
	}
}

static void assignment(ms_Compiler *compiler)
{
	if (check(compiler, MS_TOK_ID))
	{
		advance(compiler);

		uint8_t set = MS_OP_SET_LOCAL;
		int arg = resolveLocal(compiler, &compiler->previous);
		if (arg == -2)
		{
			if (compiler->currentRecord->scopeDepth == 0)
			{
				arg = identifierConstant(compiler, &compiler->previous);
				set = MS_OP_SET_GLOBAL;
			}
			else
			{
				arg = -1;
				addLocal(compiler, compiler->previous);
			}
		}
		else if (arg == -1)
		{
			arg = identifierConstant(compiler, &compiler->previous);
			set = MS_OP_SET_GLOBAL;
		}

		consume(compiler, MS_TOK_ASSIGN, "Expected '=' after variable name.");
		expression(compiler);

		if (arg != -1) emitBytes(compiler, set, arg);
	}
	else errorAtCurrent(compiler, "Expected identifier.");
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
				expression(compiler);
				consume(compiler, MS_TOK_THEN, "Expected 'then' after condition.");
				consume(compiler, MS_TOK_NEWLINE, "Expected newline after 'then'."); // TODO

				size_t thenJump = emitJump(compiler, MS_OP_JUMP_IF_FALSE);
				emitByte(compiler, MS_OP_POP);

				beginScope(compiler);
				while (!check(compiler, MS_TOK_END_IF) && !check(compiler, MS_TOK_ELSE) && !check(compiler, MS_TOK_EOF))
					statement(compiler);
				endScope(compiler);

				if (match(compiler, MS_TOK_ELSE))
				{
					consume(compiler, MS_TOK_NEWLINE, "Expected newline after 'else'."); // TODO
					size_t elseJump = emitJump(compiler, MS_OP_JUMP);
					patchJump(compiler, thenJump);
					emitByte(compiler, MS_OP_POP);

					beginScope(compiler);
					while (!check(compiler, MS_TOK_END_IF) && !check(compiler, MS_TOK_EOF))
						statement(compiler);
					endScope(compiler);

					consume(compiler, MS_TOK_END_IF, "Expected 'end if' to close if statement block.");
					patchJump(compiler, elseJump);
				}
				else
				{
					consume(compiler, MS_TOK_END_IF, "Expected 'end if' to close if statement block.");
					patchJump(compiler, thenJump);
				}
				break;

			default:
				error(compiler, "Unexpected keyword.");
		}
	}
	else
	{
		assignment(compiler);
	}
	
	consume(compiler, MS_TOK_NEWLINE, "Expected newline after statement.");
	if (compiler->panicMode) synchronize(compiler);
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

ms_InterpretResult ms_compileString(ms_VM* vm, char *source, ms_Code *code)
{
#ifdef MS_DEBUG_COMPILATION
	fprintf(stderr, "compiler: setting up compiler\n");
#endif
	ms_Scanner scanner;
	ms_initScanner(&scanner, source);

	ms_Compiler compiler;
	initCompiler(&compiler, vm, scanner, code);

	Record rec;
	initRecord(&compiler, &rec);

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

	endCompiler(&compiler);

	return compiler.hadError ? MS_INTERPRET_COMPILE_ERROR : MS_INTERPRET_OK;
}
