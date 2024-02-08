#include <stdio.h>

#include "ms_compiler.h"
#include "ms_scanner.h"

// currently, this file only has code for testing out the scanner.

ms_InterpretResult ms_compileString(ms_VM* vm, char *str, ms_Code *code)
{
	ms_Scanner scanner;
	ms_initScanner(&scanner, str);

	int prevLine = -1;
	for (;;)
	{
		ms_Token tok = ms_nextToken(&scanner);

		if (prevLine != tok.line)
			printf("%04d | ", tok.line);
		else
			printf("     | ");

		printf("%s", ms_getTokenTypeName(tok.type));

		if (tok.type != MS_TOK_NEWLINE)
			printf(" '%.*s'", tok.length, tok.start);

		putchar('\n');

		prevLine = tok.line;
		if (tok.type == MS_TOK_EOF) break;
	}

	// so the VM doesn't run anything until both the scanner and compiler are finished
	return MS_INTERPRET_COMPILE_ERROR;
}
