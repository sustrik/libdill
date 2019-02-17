generate.js generates markdown files for libdill documentation.
These are further processed to generate both UNIX and HTML man pages.

Schema:

    name: name of the function
    section: the name of the section the function belongs to
    info: short description of the function
    is_in_libdillimpl: if true, the function is in libdillimpl.h
    protocol : { // should be present only if the function is related
                 // to a network protocol
        name: name of the protocol
        info: description of the protocol
        example: example of usage of the protocol, a piece of C code
    }
    result: { // omit this field for void functions
        type: return type of the function
    }
    args: [ // may be ommited for functions with no arguments
        {
            name: name of the argument
            type: type of the argument
            suffix: part of the type to go after the name (e.g. "[]")
            info: description of the argument
        }
    ]
    add_to_synopsis: a piece of code to be added to synopsis, between the
                     include and the function declaration
    has_iol: if true, adds I/O list to the list of arguments
    has_deadline: if true, adds deadline to the list of arguments
    has_handle_argument: set to true if at least one of the arguments is
                         a handle
    allocates_handle: set to true is the function allocates at least one handle
    prologue: the part of the description of the function to go before the list
              of arguments
    epilogue: the part of the description of the function to go after the list
              of arguments
    errors: a list of possible errors, the descriptions will be pulled from
            standard_errors object
    custom_errors: [{ // custom errors take precedence over standard errors
        error name: error description
    }]
    example: example of the usage of the function, a piece of C code;
             if present, overrides the protocol example
    mem: size // if set, _mem variant of the function will be generated;
              // size if the name of the storage struct, e.h. "tcp_storage".

