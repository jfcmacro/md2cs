# `md2cs` (Markdown to 2 Coding Stories)

## Definition

This tool (`md2cs`) translates a Markdown story (`story.md`) into a [coding story](https://codingstories.io/).

The story file (`story.md`) describes the different parts of a [coding story](https://codingstories.io/become-author/guide).
Each coding story is divided into pages. A section represents each page of
the `story.md` file. That section has two parts: a header and a body. The
header begins with a line `---` and contains a series of keys and values in
YAML format. The header ends with `---`. Next to the header comes the body.
The body contains markdown marks.

## Structure of the `story.md` file

### Header 

The following list shows the accepted keys.

+ `repository:`. Indicates the URL of a repository that contains the source code
  that will be downloaded. A page can have source code only from the only
  `repository`. Other pages can have source code from a different `repository`.
+ `origin:`. It is the URL of the repository that will contain the coding story.
  Only one `origin` key can be on the `story.md` file.
+ `focus:`. Indicates a filename where the current page will be focused.
+ `tag:` or `branch`. A tag or branch where to checkout the source code
  from the current repository.

### Body

The body contains two elements: The `README.md` file of the `origin` 
repository and all pages of the generated `.story.md` file.

The first page of `story.md` contains the generated `README.md` file. 

From the second page of the  `story.md` file, it starts the pages of the coding story.

The first page can have only the title of the first and second levels. The following
pages only can have titles from the third level onwards.  

## Generates the coding story

You will compose all the coding stories by writing a `story.md` file. Once you have written down the `story.md`, you can generate the coding story using the command `md2cs`.

You can generate the repository in two ways:  *local* and *remote*. The *local stage* creates a "local" repository with all the stories divided into different commits. The *remote stage* uploads the complete story into the remote (`origin`) repository.

The command `md2cs` expects to be executed on a directory with this structure.

```shell
+ `coding story project`
  - README.md
  - story.md
  [+ dirs]
```

Using the `md2cs`command in this way, you can generate a local repository.

```shell
`coding story project`$ md2cs
```

This creates a `target` directory on the project. This `target` contains different directories from the various source code repositories (`source-code-respository-i`), which depends on the other reference to `repository` keys on the headers of the `story.md` file. 

```shell
+ `coding story project`
  - README.md
  - story.md
  [+ dirs]
  + target
    + repositories
      + source-code-repository-1
      + ...
      + source-code-repository-n
    + repository
      - README.md
      - .story.md
      + [dirs-source-code]
      - [project-files]
```

When the coding story is ready, you can upload the `origin` repository this way. 

```shell
`coding story project`$ md2cs -u
```

To execute this command, you must consider two situations: the origin repository is newer and already contains a coding story. If your situation is the first one, you don't have a problem executing this command. But, if your situation is the second one, you must enable the force reset on the server where the repository is hosted.