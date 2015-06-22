# kvlint
kvlint is a small program designed to lint KeyValues files, such as those used in TF2 huds and as flat file storage for sourcemod plugins.

This version is naive and assumes that there is a root node.

## usage
```sh
kvlint [-m] [-q] <filename>
```
- -m: allow raw newlines in strings
- -q: require all keys and values to be quoted
