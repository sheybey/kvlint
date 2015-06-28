# kvlint
kvlint is a small program designed to lint KeyValues files, such as those used in TF2 huds and as flat file storage for sourcemod plugins.

This version is naive and assumes that there is a root node.

## usage
    kvlint [-q] [-m] [-e] <filename>
- -q: require all keys and values to be quoted
- -m: allow raw newlines in strings
- -e: parse and validate escape sequences

## nitpicks / possible issues
- When reading unicode text, behavior is undefined.
- Some error messages could be refined a bit.
- More specific checks for certain mistakes would be better.
- Multi-line behavior is ill-defined. It is currently designed to pass budhud.
- Doesn't check that there is a root node.
- Doesn't check validity of "#" macro keys.

example output: http://puu.sh/iySB8/5ace25eb0d.txt
