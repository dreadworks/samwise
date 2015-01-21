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

As far as possible, code and documentation should never exceed 80 characters per line. The code documentation gets rendered by doxygen with the java style syntax using the `@` prefix. Doxygen and CLASS -style comments are getting mixed:

```C
//  --------------------------------------------------------------------------
/// Brief description ending with the full stop.
/// Detailed description spanning multiple starts after the first sentence.
///
/// @param   args some arguments
/// @param   argc argc size of the argument vector
/// @return  0 for success, -1 for error
int
sam_stuff (void *args, int argc)
{
    // ...
}
```

The detailed documentation of public methods (described in an accompanying *.h file) can be found in the *.h file. The *.c file just contains a reference to to the documentation of that function in the header file.
