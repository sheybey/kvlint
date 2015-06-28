/*
 * kvlint.c - basic syntax check for KeyValues files
 * version 0.2
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
#include <unistd.h>
#include <stdlib.h>

#define printerror(error) printf("error in %s (line %d): %s\n", argv[optind], linecount, error);

typedef enum {KEY, SUBKEY, KEYSTRING, KEYSTRINGEND, VALUESTRING, VALUESTRINGEND, STRINGESCAPE, SLASH, COMMENT, CONDITIONAL, CONDITIONALEND} state;

int main(int argc, char* argv[]) {

	FILE *kvfile;

	int character;
	int space;
	int quoted;

	state prevstate;
	state currentstate = KEY;

	extern int optind;
	int opt;
	int die = 0;

	int requirequotes = 0;
	int allowmultiline = 0;
	int parseescapes = 0;

	while ((opt = getopt(argc, argv, "qme")) != -1) {
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
			case '?':
				//getopt prints an error message
				die = 1;
				break;
		}
	}

	if (die || optind >= argc) {
		printf("usage: %s [-q] [-m] [-e] <filename>\n", argv[0]);
		printf("\t-q:\trequire all keys and values to be quoted\n");
		printf("\t-m:\tallow raw newlines in strings\n");
		printf("\t-e:\tparse and validate escape sequences\n");
		return 1;
	}


	for (; optind < argc; optind++) {

		int bracecount = 0;
		int linecount = 1;

		kvfile = fopen(argv[optind], "r");
		if (kvfile == NULL) {
			printf("%s: error: unable to open file %s\n", argv[0], argv[optind]);
			return 1;
		}

		while ((character = fgetc(kvfile)) != EOF) {
			if (character == '\r') {
				character = fgetc(kvfile);
				if (character != '\n') {
					printerror("unexpected carriage return. state is dead, exiting");
					fclose(kvfile);
					return 1;
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
									printerror("unexpected close brace (you cannot use braces in unquoted strings)")
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
					switch (character) {
						case '\t':
							if (quoted) {
								printerror("unescaped tab in key string");
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
								printerror("unexpected brace in key string (you cannot use braces in unquoted strings)")
							}
							break;
						default:
							//no state change
							break;
					}
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
							printerror("braces should be on their own line, or quoted if they are part of a string")
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
					break;
				case VALUESTRINGEND:
					//whitespace, newline, comment, or conditional
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
							}
							break;
					}
					break;
				case SLASH:
					//forward slash
					currentstate = COMMENT;
					switch (character) {
						case '/':
							//no state change
							break;
						case '*':
							printerror("only line comments are allowed. inline comments act as line comments in most games and can cause unexpected behavior");
							break;
						default:
							printerror("bogus comment");
							break;
					}
					break;
				case COMMENT:
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
							}
						default:
							//no state change
							break;
					}
					break;
				case CONDITIONAL:
					//ignore until ]
					switch(character) {
						case '\n':
							printerror("unterminated conditional")
							switch (prevstate) {
								case VALUESTRINGEND:
									currentstate = KEY;
									break;
								case KEYSTRINGEND:
									currentstate = SUBKEY;
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
					switch(character) {
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
	}
	
	return 0;
}
