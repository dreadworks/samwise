# SAMWISE #

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

    copyright

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


## Dependencies ##

Samwise relies on some libraries:

| Library name                    | Version |
|---------------------------------|---------|
| [ZeroMQ](http://zeromq.org/)    | 4.0.5   |
| [CZMQ](http://czmq.zeromq.org/) | 2.2.0   |


To render the documentation the program `doxygen` is used. The rendered documentation can be found in `/samwise/doc/*`.
