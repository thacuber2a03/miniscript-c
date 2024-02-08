#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "miniscript.h"

static char buffer[1024];

static void repl(ms_VM *vm)
{
	for (;;)
	{
		printf("> ");
		if (!fgets(buffer, sizeof buffer, stdin)) exit(-1);
		ms_interpretString(vm, buffer);
	}
}

static char *runFile(ms_VM *vm, char *path)
{
	FILE *fp = fopen(path, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "couldn't open file %s\n", path);
		exit(-1);
	}

	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	rewind(fp);

	char *source = malloc(size);
	if (source == NULL)
	{
		fprintf(stderr, "couldn't allocate enough memory\n");
		exit(-1);
	}

	if (!fread(source, 1, size, fp))
	{
		fprintf(stderr, "couldn't read from file");
		exit(-1);
	}

	ms_interpretString(vm, source);
	free(source);
}

int main(int argc, char *argv[])
{
	ms_VM *vm = ms_newVM(NULL);

	if (argc == 1)
		repl(vm);
	else if (argc == 2)
	{
		if (!strcmp(argv[1], "--test"))
			ms_runTestProgram(vm);
		else
			runFile(vm, argv[1]);
	}
	else
		fprintf(stderr, "usage: %s [script]", argv[0]);

	ms_freeVM(vm);
	return 0;
}
