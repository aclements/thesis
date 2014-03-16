#ifndef _AUSTIN_ARGS_H
#define _AUSTIN_ARGS_H

#include <stdbool.h>
#include <stddef.h>

struct Args_Info
{
        const char *name;
        struct Args_Type *type;
        union
        {
                void *dest;
                bool *dBool;
                int *dInt;
                size_t *dSize;
                const char **dString;
        } dest;
        const struct Args_Choice
        {
                const char *name;
                int val;
        } *choices;
        const char *varname;
        const char *help;
};

struct Args_Type
{
        bool (*parse)(const struct Args_Info *arg, const char *str);
        char *(*show)(const struct Args_Info *arg);
        enum {
                ARGS_ARGUMENT_NONE = 0,
                ARGS_ARGUMENT_REQUIRED = 1,
                ARGS_ARGUMENT_OPTIONAL = 2,
        } hasArg;
};

extern struct Args_Type Args_TypeBool, Args_TypeInt, Args_TypeSize,
        Args_TypeString, Args_TypeChoice;
#define ARGS_BOOL(d)   .type = &Args_TypeBool,   .dest = {.dBool = (d)}
#define ARGS_INT(d)    .type = &Args_TypeInt,    .dest = {.dInt = (d)}
#define ARGS_SIZE(d)   .type = &Args_TypeSize,   .dest = {.dSize = (d)}
#define ARGS_STRING(d) .type = &Args_TypeString, .dest = {.dString = (d)}
#define ARGS_CHOICE(d, c) \
        .type = &Args_TypeChoice, .dest = {.dInt = (d)}, .choices = c

/**
 * Parse an argument vector, returning the number of arguments
 * consumed.
 *
 * The argument vector should include the program name.  If the
 * returned value is less than argc, then non-option arguments remain
 * on the command line.
 */
int
Args_Parse(const struct Args_Info args[], int argc, char * const *argv);
/**
 * Return a summary of all argument values, including those unchanged
 * from their defaults.
 *
 * This is in a form suitable for passing to the shell.
 */
char *
Args_Summarize(const struct Args_Info args[]);

#endif // _AUSTIN_ARGS_H
