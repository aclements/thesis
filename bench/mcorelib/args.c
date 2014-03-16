#define _GNU_SOURCE             /* asprintf */

#include "args.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
Args_Parse(const struct Args_Info args[], int argc, char * const *argv)
{
        // Construct getopt option array
        int num;
        for (num = 0; args[num].name; ++num);

        struct option *os = malloc((num+1) * sizeof *os);
        if (!os) {
                fprintf(stderr, "Failed to allocate option array");
                exit(1);
        }

        for (int i = 0; i < num; ++i) {
                os[i].name = args[i].name;
                os[i].has_arg = args[i].type->hasArg;
                os[i].flag = NULL;
                os[i].val = 0;
        }
        memset(&os[num], 0, sizeof os[num]);

        // Process arguments
        optind = 0;
        int res, opt;
        while ((res = getopt_long(argc, argv, "", os, &opt)) != -1) {
                switch (res) {
                case 0:
                        // Long option
                        if (!args[opt].type->parse(&args[opt], optarg))
                                exit(2);
                        break;
                case '?':
                        exit(2);
                }
        }

        free(os);

        return optind;
}

char *
Args_Summarize(const struct Args_Info args[])
{
        char *out = strdup("");
        size_t len = 0;

        for (int i = 0; args[i].name; ++i) {
                char *show = args[i].type->show(&args[i]);
                char *ap;
                if (show)
                        asprintf(&ap, "--%s=%s ", args[i].name, show);
                else
                        asprintf(&ap, "--%s ", args[i].name);
                out = realloc(out, len + strlen(ap) + 1);
                strcpy(out + len, ap);
                len += strlen(ap);
                free(ap);
        }
        out[len] = 0;
        return out;
}

static bool
ArgsBoolParse(const struct Args_Info *arg, const char *str)
{
        if (!str)
                *arg->dest.dBool = true;
        else if (strcasecmp(str, "true") == 0)
                *arg->dest.dBool = true;
        else if (strcasecmp(str, "false") == 0)
                *arg->dest.dBool = false;
        else {
                fprintf(stderr, "Option '%s' must be 'true' or 'false'\n", arg->name);
                return false;
        }
        return true;
}

static char *
ArgsBoolShow(const struct Args_Info *arg)
{
        if (*arg->dest.dBool)
                return NULL;
        return strdup("false");
}

struct Args_Type Args_TypeBool = {ArgsBoolParse, ArgsBoolShow,
                                  ARGS_ARGUMENT_OPTIONAL};

static bool
ArgsIntParse(const struct Args_Info *arg, const char *str)
{
        char *end;
        int val = strtol(str, &end, 0);
        if (*end) {
                fprintf(stderr, "Option '%s' requires an integer\n", arg->name);
                return false;
        }
        *arg->dest.dInt = val;
        return true;
}

static char *
ArgsIntShow(const struct Args_Info *arg)
{
        char *out;
        asprintf(&out, "%d", *arg->dest.dInt);
        return out;
}

struct Args_Type Args_TypeInt = {ArgsIntParse, ArgsIntShow,
                                 ARGS_ARGUMENT_REQUIRED};

static const char ArgsSizePost[] = "KMGTPEZY";

static bool
ArgsSizeParse(const struct Args_Info *arg, const char *str)
{
        char *end;
        long long ll = strtoll(str, &end, 0);
        size_t val = ll;
        if (*end) {
                if (val != ll) {
                        fprintf(stderr, "Size '%lld' too large", ll);
                        return false;
                }
                char *post = strchr(ArgsSizePost, *end);
                if (*(end+1) || !post) {
                        fprintf(stderr, "Option '%s' requires a size\n",
                                arg->name);
                        return false;
                }
                val <<= 10 * (post - ArgsSizePost + 1);
        }
        *arg->dest.dSize = val;
        return true;
}

static char *
ArgsSizeShow(const struct Args_Info *arg)
{
        size_t val = *arg->dest.dSize;
        int post;
        for (post = 0; val && (val & 1023) == 0; ++post)
                val >>= 10;
        char *out;
        if (post)
                asprintf(&out, "%d%c", (int)val, ArgsSizePost[post - 1]);
        else
                asprintf(&out, "%d", (int)*arg->dest.dSize);
        return out;
}

struct Args_Type Args_TypeSize = {ArgsSizeParse, ArgsSizeShow,
                                  ARGS_ARGUMENT_REQUIRED};

static bool
ArgsStringParse(const struct Args_Info *arg, const char *str)
{
        *arg->dest.dString = str;
        return true;
}

static char *
ArgsStringShow(const struct Args_Info *arg)
{
        char *out = malloc(1 + 2 * strlen(*arg->dest.dString)), *o = out;
        const char *i = *arg->dest.dString;
        *(o++) = '"';
        for (; *i; ++i) {
                if (strchr("$`\\!\n\"", *i))
                        *(o++) = '\\';
                *(o++) = *i;
        }
        *(o++) = '"';
        *o = '\0';
        return out;
}

struct Args_Type Args_TypeString = {ArgsStringParse, ArgsStringShow,
                                    ARGS_ARGUMENT_REQUIRED};

static bool
ArgsChoiceParse(const struct Args_Info *arg, const char *str)
{
        const struct Args_Choice *choice;
        for (choice = arg->choices; choice->name; ++choice) {
                if (strcasecmp(str, choice->name) == 0) {
                        *arg->dest.dInt = choice->val;
                        return true;
                }
        }
        fprintf(stderr, "Option '%s' must be one of:\n", arg->name);
        for (choice = arg->choices; choice->name; ++choice)
                fprintf(stderr, "  %s", choice->name);
        fprintf(stderr, "\n");
        return false;
}

static char *
ArgsChoiceShow(const struct Args_Info *arg)
{
        const struct Args_Choice *choice;
        for (choice = arg->choices; choice->name; ++choice) {
                if (choice->val == *arg->dest.dInt)
                        return strdup(choice->name);
        }
        return strdup("???");
}

struct Args_Type Args_TypeChoice = {ArgsChoiceParse, ArgsChoiceShow,
                                    ARGS_ARGUMENT_REQUIRED};
