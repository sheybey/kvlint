# kvlint
kvlint is a small program designed to lint KeyValues files, such as those used in TF2 huds and as flat file storage for sourcemod plugins.

## usage
    kvlint -h | [-q] [-m] [-e [-s] [-w]] [-b] [-d] [-r] <filename> [...]
- -h: show usage message
- -q: require all keys and values to be quoted
- -m: allow raw newlines in strings
- -e: parse and validate escape sequences
- -s: ignore shrug emotes when validating escape sequences
- -w: ignore invalid escape sequences in the first root key string
- -b: allow block comments
- -d: validate #base directives
- -r: allow multiple root keys

## nitpicks / possible issues
- When reading non-ASCII text, behavior is undefined.
- Some error messages could be refined a bit.
- More specific checks for certain mistakes would be better.
- Multi-line behavior is ill-defined. It is currently designed to pass budhud.

example output: http://puu.sh/iySB8/5ace25eb0d.txt
