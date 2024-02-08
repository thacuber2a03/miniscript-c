#include <string.h>

#include "ms_scanner.h"

const char *ms_getTokenTypeName(ms_TokenType type)
{
	switch (type)
	{
		#define TOKEN(t) case t: return #t;
		#include "ms_tokens.h"
		#undef TOKEN
	}
}

void ms_initScanner(ms_Scanner *scanner, char *source)
{
	scanner->start = scanner->current = source;
	scanner->line = 0;
}

static inline bool isAtEnd(ms_Scanner *scanner) { return *scanner->current == '\0'; }

static inline bool check(ms_Scanner *scanner, char c) { return *scanner->current == c; }

static inline char advance(ms_Scanner *scanner) { return *scanner->current++; }

static inline bool match(ms_Scanner *scanner, char c)
{
	if (!check(scanner, c)) return false;
	advance(scanner); return true;
}

static ms_Token newToken(ms_Scanner *scanner, ms_TokenType type)
{
	ms_Token token;
	token.type = type;
	token.start = scanner->start;
	token.length = scanner->current - scanner->start;
	token.line = scanner->line;
	return token;
}

static ms_Token errToken(ms_Scanner *scanner, const char* err)
{
	ms_Token token;
	token.type = MS_TOK_ERROR;
	token.start = err;
	token.length = strlen(err);
	token.line = scanner->line;
	return token;
}

static inline char peek(ms_Scanner *scanner) { return *scanner->current; }
static inline char peekNext(ms_Scanner *scanner) { return scanner->current[1]; }

static inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
static inline bool isAlpha(char c)
{
	return c >= 'A' && c <= 'Z'
	    || c >= 'a' && c <= 'z'
	    || c == '_'; 
}

static ms_Token checkKeyword
	(ms_Scanner *scanner, const char *keyword, int length, ms_TokenType type)
{
	if ((scanner->current - scanner->start) != length)
		return newToken(scanner, MS_TOK_ID);

	if (strncmp(keyword, scanner->start, length) != 0)
		return newToken(scanner, MS_TOK_ID);

	return newToken(scanner, type);
}

static ms_Token scanToken(ms_Scanner *scanner);

static ms_Token scanIdentifier(ms_Scanner *scanner)
{
	while (!isAtEnd(scanner)
	   && (isAlpha(peek(scanner)) || isDigit(peek(scanner))))
		advance(scanner);

	// TODO: should `checkKeyword` check the entire string?
	switch (*scanner->start)
	{
		case 'a': return checkKeyword(scanner, "and", 3, MS_TOK_AND);
		case 'e': {
			char *s = scanner->start;
			ms_TokenType type = scanToken(scanner).type;

			if (type >= MS_TOK_FUNC && type <= MS_TOK_FOR)
			{
				scanner->start = s;
				return newToken(scanner, type+1);
			}

			return errToken(scanner, "'end' without proper following keyword ('if', 'while', etc.)");
		}
		case 'f':
			if (scanner->current - scanner->start > 1)
				switch (scanner->start[1])
				{
					case 'a': return checkKeyword(scanner, "false", 5, MS_TOK_FALSE);
					case 'o': return checkKeyword(scanner, "for", 3, MS_TOK_FOR);
					case 'u': return checkKeyword(scanner, "function", 8, MS_TOK_FUNC);
				}
			break;
		case 'i': return checkKeyword(scanner, "if", 2, MS_TOK_IF);
		case 'n':
			if (scanner->current - scanner->start > 1)
				switch (scanner->start[1])
				{
					case 'o': return checkKeyword(scanner, "not", 3, MS_TOK_NOT);
					case 'u': return checkKeyword(scanner, "null", 4, MS_TOK_NULL);
				}
			break;
		case 'o': return checkKeyword(scanner, "or", 2, MS_TOK_OR);
		case 't': return checkKeyword(scanner, "true", 4, MS_TOK_TRUE);
		case 'w': return checkKeyword(scanner, "while", 5, MS_TOK_WHILE);
	}

	return newToken(scanner, MS_TOK_ID);
}

static ms_Token scanNumber(ms_Scanner *scanner)
{
	while (isDigit(peek(scanner))) advance(scanner);

	if (peek(scanner) == '.' && isDigit(peekNext(scanner)))
	{
		advance(scanner);
		while (isDigit(peek(scanner))) advance(scanner);
	}

	return newToken(scanner, MS_TOK_NUM);
}

static ms_Token scanToken(ms_Scanner *scanner)
{
	char c = advance(scanner);
	switch (c)
	{
		case '\n':
			scanner->line++;
			return newToken(scanner, MS_TOK_NEWLINE);

		case ' ': case '\r': case '\t':
			return ms_nextToken(scanner);
		
#define OP_ASSIGN(scanner, type) do {                          \
	if (match(scanner, '=')) return newToken(scanner, (type)+1); \
	return newToken(scanner, type);                              \
} while(false)

		case '+': OP_ASSIGN(scanner, MS_TOK_PLUS);
		case '-': OP_ASSIGN(scanner, MS_TOK_MINUS);
		case '*': OP_ASSIGN(scanner, MS_TOK_STAR);

		case '/':
			// can also be the start of a comment
			if (match(scanner, '/'))
			{
				while (!isAtEnd(scanner) && !check(scanner, '\n')) advance(scanner);
				return ms_nextToken(scanner);
			}

			// it isn't
			OP_ASSIGN(scanner, MS_TOK_SLASH);

		case '^': OP_ASSIGN(scanner, MS_TOK_CARET);
		case '%': OP_ASSIGN(scanner, MS_TOK_PERCENT);

		case '>': OP_ASSIGN(scanner, MS_TOK_GREATER);
		case '<': OP_ASSIGN(scanner, MS_TOK_LESS);
		case '=': OP_ASSIGN(scanner, MS_TOK_ASSIGN);

#undef OP_ASSIGN

		case '!':
			if (match(scanner, '=')) return newToken(scanner, MS_TOK_NEQ);
			return errToken(scanner, "Expected '=' after '!'");

		case ':': return newToken(scanner, MS_TOK_COLON);
		case ';': return newToken(scanner, MS_TOK_SEMI);

		default:
			if (isDigit(c)) return scanNumber(scanner);
			if (isAlpha(c)) return scanIdentifier(scanner);
			return errToken(scanner, "Unknown character");
	}
}

ms_Token ms_nextToken(ms_Scanner *scanner)
{
	scanner->start = scanner->current;
	if (!isAtEnd(scanner)) return scanToken(scanner);

	return newToken(scanner, MS_TOK_EOF);
}
