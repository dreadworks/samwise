<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [SAMWISE](#samwise)
  - [Dependencies](#dependencies)
    - [Building samwise](#building-samwise)
    - [Render documentation](#render-documentation)
  - [Tools](#tools)
    - [Description](#description)
    - [Release a new version - release.fish](#release-a-new-version---releasefish)
    - [Install dependencies locally - deps.fish](#install-dependencies-locally---depsfish)
    - [Create a TAGS file - tags.fish](#create-a-tags-file---tagsfish)
  - [Project Setup](#project-setup)
    - [Code and Project Conventions](#code-and-project-conventions)
    - [Constrictions of the CLASS spec](#constrictions-of-the-class-spec)
    - [Additional Conventions](#additional-conventions)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# SAMWISE #

## Dependencies ##

### Building samwise ###

Samwise relies on some libraries:

| Library name                                       | Version |
|----------------------------------------------------|---------|
| [ZeroMQ](http://zeromq.org/)                       | 4.0.5   |
| [CZMQ](http://czmq.zeromq.org/)                    | 2.2.0   |
| [RabbitMQ-C](https://github.com/alanxz/rabbitmq-c) | 0.5.2   |


### Render documentation ###

To render the documentation the program `doxygen` is used. The rendered documentation can be found in `/samwise/doc/*`.


## Tools ##

### Description ###

Currently there are a number of shell script files inside the `samwise/tools` directory. As their suffix hints, they are written for the [fish shell](fishshell.com). You can safely run them like `. tools/script.fish argument1`. Please see below for a documentation of the various scripts.


### Release a new version - release.fish ###

This script updates all places where version numbers are used. Currently the touched files are `include/sam.h`, `Doxyfile` and `configure.ac`. A new `configure.scan` is created for this purpose. It also creates a new `configure` script and runs `make` afterwards. You can call it like this:

```bash
. tools/release.fish major minor patch [cflags]
# where major, minor and patch are numbers
# optional: cflags - additional compile flags

# for example
. tools/release.fish 1 0 3 "-g"

```


### Install dependencies locally - deps.fish ###

This script automatically downloads all libraries samwise depends on and installs them into the projects root directory. It creates a folder `usr` used as the prefix to build all dependencies. Afterwards, it invokes `./configure` and passes the appropriate `CFLAGS` to include the `lib/` and `include/` directories of the local installation.

```bash
. tools/deps.fish [cflags]
# optionally pass in additional compile flags
# for example
. tools/deps.fish "-g"
```


### Create a TAGS file - tags.fish ###

To efficiently jump to function definitions with emacs using `find-tag` and it's friends, a TAGS file must be generated using the `etags` program. To build the symbol table and access the files containing the definitions and declarations, `tags.fish` clones all dependencies into `.tags` and creates `samwise/TAGS`.

```bash
. tools/tags.fish
```



## Project Setup ##

### Code and Project Conventions ###

This project uses the [C Language Style for Scalability (CLASS)](http://rfc.zeromq.org/spec:21) code and project structure conventions. The projects name is `samwise`, the project abbreviation `sam` and the project prefix `sam_`. The specification applies to everything inside the `/samwise` directory. The C language level is C99.


### Constrictions of the CLASS spec ###

I wholeheartedly disagree that omitting the curly braces for one-line if statements is a good idea. So I'm going to break the CLASS spec for if statements:

```C
// always like this, please:
if (condition) {
    // ...
}
```

C++ is not going to be supported. It is not needed to wrap code in statements like

```C
#ifdef __cplusplus
extern "C" {
#endif

// ...

#ifdef __cplusplus
}
#endif
```

### Additional Conventions ###

As far as possible, code and documentation should never exceed 80 characters per line. The code documentation gets rendered by doxygen with the java style syntax using the `@` prefix. Doxygen and CLASS -style comments are getting mixed.

Files should be documented like this:

```C
/*  =========================================================================

    name - summary

    license

    =========================================================================
*/
/**

   @brief brief description

   thorough description

*/

// ...

```

The header file just contains a brief description of the function, the necessary parameters and the expected return value. A more thorough documentation of the specific implementation of that function can be found in the *.c file.

For *.h files:

```C
//  --------------------------------------------------------------------------
/// @brief   short description of the functions purpose
/// @param   args some arguments
/// @param   argc argc size of the argument vector
/// @return  0 for success, -1 for error
int
sam_stuff (void *args, int argc);
```

For *.c files:

```C
//  --------------------------------------------------------------------------
/// Brief description ending with the full stop.
/// Detailed description after the first sentence. Should be used to precisely
/// describe how the function works. Inline comments should be avoided as much
/// as possible - the code should be self-explanatory.
int
sam_stuff (void *args, int argc)
{
    // ...
}
```


## Notes ##

* capture local amqp network traffic with tshark: `sudo tshark -f "tcp port 5672" -R amqp -i lo`

