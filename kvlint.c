/*
 * kvlint.c - basic syntax check for KeyValues files
 * version 0.3
 *
 * Copyright (c) 2015 Sam Heybey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice and this list of conditions.
 * 2. The author of this program is not liable for any damage caused by taking the program's output at face value.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define S_ISREG(m) (m & S_IFMT) == S_IFREG
#define BADCH (int)'?'
#define BADARG (int)':'
#define EMSG ""

int opterr = 1, optind = 1, optopt, optreset;
char* optarg;

int getopt(int nargc, const char** nargv, const char* ostr) {
	static char* place = EMSG;
	const char* optlistind;
	if (optreset || !*place) {
		optreset = 0;
		if (optind >= nargc || *(place = nargv[optind]) != '-') {
			place = EMSG;
			return -1;
		}
		if (place[1] && *++place == '-') {
			++optind;
			place = EMSG;
			return -1;
		}
	}
	if ((optopt = (int)*place++) == (int)':' ||
		!(optlistind = strchr(ostr, optopt))) {
		if (optopt == (int)'-') {
			return -1;
		}
		if (!*place) {
			++optind;
		}
		if (opterr && *ostr != ':') {
			printf("illegal option -- %c\n", optopt);
		}
		return BADCH;
	}
	if (*++optlistind != ':') {
		optarg = NULL;
		if (!*place) {
			++optind;
		}
	} else {
		if (*place) {
			optarg = place;
		} else if (nargc <= ++optind) {
			place = EMSG;
			if (*ostr == ':') {
				return BADARG;
			}
			if (opterr) {
				printf("option requires an argument -- %c\n", optopt);
			}
			return BADCH;
		} else {
			optarg = nargv[optind];
		}
		place = EMSG;
		++optind;
	}
	return optopt;
}
#else
#include <unistd.h>
#include <libgen.h>
#endif

#define printerror(error) printf("error in %s (line %d): %s\n", argv[optind], linecount, error)
#define MAX_STRING_LENGTH 1024

typedef enum {
	KEY, SUBKEY,
	KEYSTRING, KEYSTRINGEND,
	VALUESTRING, VALUESTRINGEND,
	STRINGESCAPE,
	SLASH, LINECOMMENT, BLOCKCOMMENT, BLOCKASTERISK,
	CONDITIONAL, CONDITIONALEND
} state;

int isfile(const char* filename) {
	struct stat st;
	if ((stat(filename, &st) != -1) && S_ISREG(st.st_mode)) {
		return 1;
	}
	return 0;
}

int main(int argc, char** argv) {
#ifndef _WIN32
	extern int optind;
#endif
	int opt;
	int die = 0;

	int requirequotes = 0;
	int allowmultiline = 0;
	int parseescapes = 0;
	int blockcomments = 0;
	int validatedirectives = 0;

	while ((opt = getopt(argc, argv, "qmebd")) != -1) {
		switch (opt) {
			case 'q':
				requirequotes = 1;
				break;
			case 'm':
				allowmultiline = 1;
				break;
			case 'e':
				parseescapes = 1;
				break;
			case 'b':
				blockcomments = 1;
				break;
			case 'd':
				validatedirectives = 1;
				break;
			case '?':
				//getopt prints an error message
				die = 1;
				break;
		}
	}

	if (die || optind >= argc) {
		printf("usage: %s [-q] [-m] [-e] [-b] [-d] <filename> [...]\n", argv[0]);
		printf("\t-q:\trequire all keys and values to be quoted\n");
		printf("\t-m:\tallow raw newlines in strings\n");
		printf("\t-e:\tparse and validate escape sequences\n");
		printf("\t-b:\tallow block comments\n");
		printf("\t-d:\tvalidate #base directives\n");
		return 1;
	}

	for (; optind < argc; optind++) {

		FILE *kvfile;

		int bracecount = 0;
		int linecount = 1;

		int character;

		int space = 0;
		int quoted = 0;
		int directive = 0;
		int checkfile = 0;
		int overflow = 0;

		char directivename[MAX_STRING_LENGTH] = "";
		int directiveindex = 0;

		char string[MAX_STRING_LENGTH] = "";
		int stringindex = 0;

		state prevstate;
		state currentstate = KEY;

		kvfile = fopen(argv[optind], "r");

		if (kvfile == NULL) {
			printf("%s: error: unable to open file %s\n", argv[0], argv[optind]);
			continue;
		}

		while ((character = fgetc(kvfile)) != EOF) {
			if (character == '\r') {
				character = fgetc(kvfile);
				if (character != '\n') {
					printerror("unexpected carriage return, stopping");
					fclose(kvfile);
					break;
				}
			}
			if (character == '\n') {
				//a newline will always increase the linecount regardless of errors
				linecount++;
			}
			switch (currentstate) {
				case KEY:
					//newline, whitespace, close brace, string, or comment
					switch (character) {
						case '\n':
						case '\t':
						case ' ':
							//no state change
							break;
						case '}':
							if (--bracecount < 0) {
								if (requirequotes) {
									printerror("unexpected close brace");
								} else {
									printerror("unexpected close brace (you cannot use braces in unquoted strings)");
								}
								bracecount = 0;
							}
							break;
						case '{':
							printerror("unexpected open brace (maybe you forgot to name a key)");
							bracecount++;
							break;
						case '\'':
							printerror("unexpected single quote (use double quotes instead)");
							break;
						case '"':
							quoted = 1;
							currentstate = KEYSTRING;
							break;
						case '/':
							prevstate = KEY;
							currentstate = SLASH;
							break;
						case '[':
							printerror("conditionals must be on the same line as the key they apply to");
							break;
						default:
							if (requirequotes) { 
								printerror("unexpected character (maybe you forgot to quote a string)");
							} else {
								quoted = 0;
								currentstate = KEYSTRING;
							}
							break;
					}
					break;
				case SUBKEY:
					//newline, whitespace, open brace, or comment
					switch (character) {
						case '\n':
						case '\t':
						case ' ':
							//no state change
							break;
						case '{':
							bracecount++;
							currentstate = KEY;
							break;
						case '/':
							prevstate = SUBKEY;
							currentstate = SLASH;
							break;
						case '[':
							printerror("conditionals must be on the same line as the key they apply to");
							break;
						default:
							printerror("unexpected character (probably malformed or missing subkey)");
							break;
					}
					break;
				case KEYSTRING:
					//anything except a newline
					if (stringindex == 0) {
						string[0] = '\0';
						directivename[0] = '\0';
						overflow = 0;
					}
					if (stringindex == MAX_STRING_LENGTH) {
						printerror("key string size limit exceeded");
						string[MAX_STRING_LENGTH - 1] = '\0';
						directivename[MAX_STRING_LENGTH - 1] = '\0';
						overflow = 1;
					}
					if (!overflow) {
						string[stringindex] = character;
						if (directive) {
							directivename[directiveindex++] = character;
						}
					}
					switch (character) {
						case '\t':
							if (quoted) {
								if (parseescapes) {
									printerror("unescaped tab in key string");
								}
							} else {
								space = 1;
								currentstate = KEYSTRINGEND;
							}
							break;
						case ' ':
							if (!quoted) {
								space = 1;
								currentstate = KEYSTRINGEND;
							}
							break;
						case '\n':
							if (quoted) {
								if (!allowmultiline) {
									printerror("unterminated key string");
									currentstate = SUBKEY;
								}
							} else {
								currentstate = SUBKEY;
							}
							break;
						case '\\':
							if (parseescapes) {
								if (quoted) {
									prevstate = KEYSTRING;
									currentstate = STRINGESCAPE;
								} else {
									printerror("backslash in unquoted key string (should you be parsing escape sequences?)");
								}
							}
							break;
						case '"':
							if (quoted) {
								space = 0;
								currentstate = KEYSTRINGEND;
							} else {
								printerror("double-quote in unquoted key string");
							}
							break;
						case '{':
						case '}':
							if (!quoted) {
								printerror("unexpected brace in key string (you cannot use braces in unquoted strings)");
							}
							break;
						case '#':
							if (validatedirectives && stringindex == 0) {
								directive = 1;
							}
							break;
						default:
							//no state change
							break;
					}
					if (currentstate == KEYSTRINGEND) {
						if (!overflow) {
							string[stringindex] = '\0';
						}
						stringindex = -1;
						if (directive) {
							directive = 0;
							if (!overflow) {
								directivename[directiveindex - 1] = '\0';
							}
							directiveindex = 0;
							if (strcmp(directivename, "base") == 0) {
								checkfile = 1;
							}
						}
					}
					stringindex++;
					break;
				case KEYSTRINGEND:
					//newline, whitespace, string, comment, or conditional
					switch (character) {
						case '\n':
							currentstate = SUBKEY;
							break;
						case '\t':
						case ' ':
							space = 1;
							break;
						case '"':
							if (!space) {
								printerror("missing space between key and value strings");
							}
							quoted = 1;
							currentstate = VALUESTRING;
							break;
						case '/':
							prevstate = KEYSTRINGEND;
							currentstate = SLASH;
							break;
						case '[':
							prevstate = KEYSTRINGEND;
							currentstate = CONDITIONAL;
							break;
						case '{':
							bracecount++;
							currentstate = KEY;
							printerror("braces should be on their own line, or quoted if they are part of a string");
							break;
						case '}':
							printerror("unexpected close brace (possibly unquoted value string)");
							break;
						default:
							if (requirequotes) {
								printerror("unexpected character after key string (possibly unquoted value string)");
							} else {
								quoted = 0;
								currentstate = VALUESTRING;
							}
							break;
					}
					break;
				case VALUESTRING:
					//anything except a newline
					if (stringindex == 0) {
						string[0] = '\0';
						overflow = 0;
					}
					if (stringindex == MAX_STRING_LENGTH) {
						printerror("value string size limit exceeded");
						string[MAX_STRING_LENGTH - 1] = '\0';
						overflow = 1;
					}
					if (!overflow) {
						string[stringindex] = character;
					}
					switch (character) {
						case '\t':
							if (quoted) {
								if (!allowmultiline) {
									printerror("unescaped tab in value string");
								}
							} else {
								space = 1;
								currentstate = VALUESTRINGEND;
							}
							break;
						case ' ':
							if (!quoted) {
								space = 1;
								currentstate = VALUESTRINGEND;
							}
							break;
						case '\n':
							if (quoted) {
								if (!allowmultiline) {
									printerror("unterminated value string");
									currentstate = KEY;
								}
							} else {
								currentstate = KEY;
							}
							break;
						case '\\':
							if (parseescapes) {
								if (quoted) {
									prevstate = VALUESTRING;
									currentstate = STRINGESCAPE;
								} else {
									printerror("backslash in unquoted value string (should you be parsing escape sequences?)");
								}
							}
							break;
						case '"':
							currentstate = VALUESTRINGEND;
							break;
						case '}':
						case '{':
							if (!quoted) {
								printerror("unexpected brace in value string (you cannot use braces in unquoted strings)");
							}
							break;
						default:
							//no state change
							break;
					}
					if (!overflow && currentstate == VALUESTRINGEND) {
						string[stringindex] = '\0';
					}
					stringindex++;
					break;
				case VALUESTRINGEND:
					//whitespace, newline, comment, or conditional
					if (checkfile) {
						checkfile = 0;
						if (!isfile(string)) {
							printerror("unreadable included file");
						}
					}
					switch (character) {
						case '\t':
						case ' ':
							//no state change
							break;
						case '\n':
							currentstate = KEY;
							break;
						case '/':
							prevstate = VALUESTRINGEND;
							currentstate = SLASH;
							break;
						case '[':
							prevstate = VALUESTRINGEND;
							currentstate = CONDITIONAL;
							break;
						default:
							printerror("unexpected character after value string (maybe you forgot to use quotes)");
							break;
					}
					break;
				case STRINGESCAPE:
					//backslash, t, n, quote
					currentstate = prevstate;
					switch (character) {
						case '\\':
						case 't':
						case 'n':
						case '"':
							//no state change
							break;
						default:
							switch (prevstate) {
								case KEYSTRING:
									printerror("invalid escape sequence in key string");
									break;
								case VALUESTRING:
									printerror("invalid escape sequence in value string");
									break;
								default:
									printerror("you've found a bug in kvlint! please submit an issue on github with this error message and the file you're linting.");
									break;
							}
							break;
					}
					break;
				case SLASH:
					//forward slash
					switch (character) {
						case '/':
							currentstate = LINECOMMENT;
							break;
						case '*':
							if (blockcomments) {
								currentstate = BLOCKCOMMENT;
							} else {
								currentstate = LINECOMMENT;
								printerror("only line comments are allowed. block comments act as line comments in most games and can cause unexpected behavior");
							}
							break;
						default:
							currentstate = LINECOMMENT;
							printerror("bogus comment");
							break;
					}
					break;
				case LINECOMMENT:
					//ignore the rest of the line
					switch (character) {
						case '\n':
							switch (prevstate) {
								case KEY:
								case VALUESTRINGEND:
									currentstate = KEY;
									break;
								case SUBKEY:
								case KEYSTRINGEND:
									currentstate = SUBKEY;
									break;
								default:
									printerror("you've found a bug in kvlint! please submit an issue on github with this error message and the file you're linting.");
									printerror("unexpected parser state in linecomment");
									break;
							}
						default:
							//no state change
							break;
					}
					break;
				case BLOCKCOMMENT:
					//ignore until */
					switch (character) {
						case '*':
							currentstate = BLOCKASTERISK;
							break;
						default:
							//no state change
							break;
					}
					break;
				case BLOCKASTERISK:
					//asterisk in block comment
					switch (character) {
						case '*':
							//no state change
							break;
						case '/':
							currentstate = prevstate;
							break;
						default:
							currentstate = BLOCKCOMMENT;
							break;
					}
					break;
				case CONDITIONAL:
					//ignore until ]
					switch (character) {
						case '\n':
							printerror("unterminated conditional");
							switch (prevstate) {
								case VALUESTRINGEND:
									currentstate = KEY;
									break;
								case KEYSTRINGEND:
									currentstate = SUBKEY;
									break;
								default:
									printerror("you've found a bug in kvlint! please submit an issue on github with this error message and the file you're linting.");
									printerror("unexpected parser state in conditional");
									break;
							}
							break;
						case ']':
							currentstate = CONDITIONALEND;
							break;
					}
					break;
				case CONDITIONALEND:
					//whitespace, newline, or comment
					switch (character) {
						case ' ':
						case '\t':
							//no state change
							break;
						case '\n':
							switch (prevstate) {
								case VALUESTRINGEND:
									currentstate = KEY;
									break;
								case KEYSTRINGEND:
									currentstate = SUBKEY;
									break;
								default:
									printerror("you've found a bug in kvlint! please submit an issue on github with this error message and the file you're linting.");
									printerror("unexpected parser state in conditionalend");
									break;
							}
							break;
						case '[':
							printerror("only one conditional may be used per key");
							break;
						case '/':
							currentstate = SLASH;
							break;
						default:
							printerror("unexpected character after conditional");
							break;
					}
					break;
			}
		}
		fclose(kvfile);
		if (bracecount > 0) {
			printf("error in %s: unclosed key\n", argv[optind]);
		}
		if (currentstate == SUBKEY) {
			printf("error in %s: trailing key string", argv[optind]);
		}
	}
	
	return 0;
}
