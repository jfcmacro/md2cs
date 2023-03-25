# `md2cs` (Markdown to 2 Coding Stories)

## Definition

This tool (`md2cs`) translates a Markdown story (`story.md`) into a coding story.

The story file (`story.md`) describes the different parts of a coding story.
Each coding story is divided into pages. A section represents each page on
the `story.md` file. That section has two parts: a header and a body. The
header begins with a line `---` and contains a series of keys and values in
YAML format. The header ends with `---`. Next to the header comes the body,
the body contains markdown marks.

## Body keys

The next list is accepts keys.

+ `repository:`. Indicates the URL of a repository that contains source code
  that will be download. A page can have source code only from only
  `repository`. Other pages can have source code from a different `repository`.
+ `origin:`. It is a URL of the repository that will contain the coding story.
  Only can be indicate ones.
+ `focus:`. Indicates a filename where the current page will be focused.
+ `tag:` or `branch`. Indicate a specific code to download from the last
  `repository` defined.

## md2cs command line

```shell
$ md2cs -h
```