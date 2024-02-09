#ifndef MS_SCANNER_H
#define MS_SCANNER_H

#include "ms_common.h"

typedef enum {
	#define TOKEN(t) t,
	#include "ms_tokens.h"
	#undef TOKEN
} ms_TokenType;

const char *ms_getTokenTypeName(ms_TokenType type);

typedef struct {
	ms_TokenType type;
	const char *start;
	int length, line;
} ms_Token;

typedef struct {
	char *start, *current;
	int line;
} ms_Scanner;

void ms_initScanner(ms_Scanner *scanner, char *source);
ms_Token ms_nextToken(ms_Scanner *scanner);
void ms_debugScanner(char *str);

#endif
