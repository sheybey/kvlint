/*
 * kvlint.c - basic syntax check for KeyValues files
 * version 0.1
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
#define printerror(error) printf("%s: error in %s (line %d): %s\n", argv[0], argv[1], linecount, error);

typedef enum {KEY, SUBKEY, KEYSTRING, KEYSTRINGEND, VALUESTRING, VALUESTRINGEND} state;

int main(int argc, const char* argv[]) {
	FILE *kvfile;
	int line = 1;
	if (argc != 2) {
		printf("usage: %s <filename>\n", argv[0]);
		return 1;
	}
	kvfile = fopen(argv[1], "r");
	if (kvfile == NULL) {
		printf("%s: error: unable to open file %s\n", argv[0], argv[1]);
		return 1;
	}
	int character;
	int bracecount = 0;
	int space;
	int linecount = 1;
	state currentstate = KEY;
	while ((character = fgetc(kvfile)) != EOF) {
		if (character == '\n') {
			//a newline will always increase the linecount regardless of errors
			linecount++;
		}
		switch (currentstate) {
			case KEY:
				//newline, whitespace, close brace, or quote 
			    switch (character) {
			    	case '\n':
			    	case '\t':
			    	case ' ':
			    		//no state change
			    		break;
			    	case '}':
				    	if (--bracecount < 0) {
			    			printerror("unexpected close brace");
			    			bracecount = 0;
			    		}
			    		break;
			    	case '\'':
			    		printerror("unexpected single quote (use double quotes instead)");
			    	case '"':
			    		currentstate = KEYSTRING;
			    		break;
			    	default:
			    		printerror("unexpected character (maybe you forgot to quote a string)");
			    		break;
			    }
			    continue;
			case SUBKEY:
				//newline, whitespace, or open brace
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
					default:
						printerror("unexpected character (probably malformed or missing subkey)");
						break;
				}
				continue;
			case KEYSTRING:
				//anything except a newline
				switch (character) {
					case '\n':
						printerror("unterminated key string");
						currentstate = SUBKEY;
						break;
					case '"':
						space = 0;
						currentstate = KEYSTRINGEND;
						break;
					default:
						break;
				}
				continue;
		    case KEYSTRINGEND:
		        //newline, whitespace, or quote
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
		    			currentstate = VALUESTRING;
		    			break;
		    		default:
		    			printerror("unexpected character (maybe you forgot the newline before opening a subkey)");
		    			break;
		    	}
		    	continue;
		    case VALUESTRING:
		    	//anything except a newline
				switch (character) {
					case '\n':
						printerror("unterminated value string");
						currentstate = KEY;
						break;
					case '"':
						currentstate = VALUESTRINGEND;
						break;
					default:
						break;
				}
				continue;
		    case VALUESTRINGEND:
		    	//whitespace or newline
		    	switch (character) {
		    		case '\t':
		    		case ' ':
		    			//no state change
		    			break;
		    		case '\n':
		    			currentstate = KEY;
		    			break;
		    		default:
		    			printerror("unexpected character");
		    			break;
		    	}
		}
	}
	fclose(kvfile);
	if (bracecount > 0) {
		printf("%s: error in %s: unclosed key", argv[0], argv[1]);
	}
	return 0;
}
