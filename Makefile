CC=gcc
CFLAGS=-pedantic -Wall
LDFLAGS=
RM=rm -f
MV=mv -f
CP=cp
MKDIR=mkdir -p
README=README.md LICENSE
MSBUILD="/c/Program Files (x86)/MSBuild/14.0/Bin/MSBuild.exe"
MSFLAGS=//t:Rebuild //p:Configuration=Release //p:Platform=x86
PREFIX=$(DESTDIR)/usr/local

all: kvlint

kvlint: kvlint.o
	$(CC) $(LDFLAGS) kvlint.o -o kvlint
	strip kvlint

kvlint.o: kvlint.c

msbuild:
	$(MSBUILD) kvlint.sln $(MSFLAGS)
	$(MV) Release/kvlint.exe .

clean:
	$(RM) -r kvlint kvlint.o Release

distclean:
	$(RM) *.tar.gz *.zip

fullclean: clean distclean

zip: clean msbuild
	zip kvlint-win32 kvlint.exe $(README)

tar: clean kvlint
	tar czf kvlint-linux32.tar.gz kvlint $(README)

install:
	$(MKDIR) $(PREFIX)/share/doc/kvlint/
	$(CP) $(README) $(PREFIX)/share/doc/kvlint/
	$(CP) kvlint $(PREFIX)/bin/

uninstall:
	$(RM) $(PREFIX)/bin/kvlint
	$(RM) -r $(PREFIX)/share/doc/kvlint
