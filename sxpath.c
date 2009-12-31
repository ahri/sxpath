/**********************************************************
 *
 * Title:       (shit)XPath
 *
 * Description: Grab content from XML files for further
 *              processing in other UNIX-like processes...
 *              e.g. piping into `awk -F': '` ;-)
 *
 * Version:     0.8
 *
 * Author:      Adam Piper
 *
 * Email:       adam@ahri.net
 *
 * Date:        2008-12-29
 *
 * License:     BSD
 *
 **********************************************************/

#include <stdlib.h>
#include  <stdio.h>
#include <string.h>
#include <stdarg.h>

#define PROGRAM "sxpath"
#define VERSION "0.8"

#ifndef true
#define true                    1
#endif
#ifndef false
#define false                   0
#endif

#define MAX_LINE                20480

typedef enum E_MODE {
        MODE_UNK,
        MODE_TAG,
        MODE_ATTR,
        MODE_ATTR_PRE_E,
        MODE_ATTR_POST_E,
        MODE_ATTR_VAL,
        MODE_ATTR_VAL_SQ,
        MODE_ATTR_VAL_DQ,
        MODE_TAG_END,
        MODE_TAG_XML_HEAD,
        MODE_TAG_COMMENT,
        MODE_TAG_FA,
        MODE_TAG_MARKED,
        MODE_TAG_M_TYPE,
        MODE_TAG_M_CONTENT,
        MODE_CDATA,
        MODE_CDATA_CONTENT,
        MODE_CDATA_PE
} MODE;

/* Rules for HTML4 found at http://htmlhelp.com/reference/html40/block.html and http://htmlhelp.com/reference/html40/inline.html */
#define HTML_BLOCK_LEVEL        ">address>blockquote>center>dir>div>dl>fieldset>form>h1>h2>h3>h4>h5>h6>hr>isindex>menu>noframes>noscript>ol>p>pre>table>ul>dd>dt>frameset>li>tbody>td>tfoot>th>thead>tr>"
#define HTML_EITHER             ">applet>button>del>iframe>ins>map>object>script>"
#define HTML_INLINE             ">a>abbr>acronym>b>basefont>bdo>big>br>cite>code>dfn>em>font>i>img>input>kbd>label>q>s>samp>select>small>span>strike>strong>sub>sup>textarea>tt>u>var>"

#define HTML_SELFCLOSE          ">meta>img>input>br>hr>"
#define HTML_RESTARTING         ">p>li>"

#define XML_INVALID(...)        xmlInvalid(__FILE__, __LINE__, line_no, c-line+1, false, html, in_match, *c, __VA_ARGS__)
#define XML_WARNING(...)        xmlInvalid(__FILE__, __LINE__, line_no, c-line+1, true, html, in_match, *c, __VA_ARGS__)
#define MODE_CHANGE(change)     modeChange(__FILE__, __LINE__, line_no, c-line+1, *c, &mode, change)
#define LOCN_SEARCH(loc)        searchLocation(argc, argv, verbose, &matches, &in_match, loc)

#define TAG_RESET()             *tag       = '\0'; delta    = 0
#define ATTR_RESET()            *attribute = '\0'; in_match = false
#define MTYPE_RESET()           *mtype     = '\0'

void xmlInvalid(const char *sfile, const int sline, const int line_no, const int char_no, const int warning, const int html, const int in_match, const char c, const char *message, ...);
void modeChange(const char *sfile, const int sline, const int line_no, const int char_no, const char c, MODE *mode, const MODE change);
void searchLocation(const int argc, const char **argv, const int verbose, int *matches, int *in_match, const char *location);
void strchrcat(char *str, const char c);
void locationSlice(char *s);
char *strrstr(const char *haystack, const char *needle);
int  validNameChar(char *c);

void version(void) {
        fprintf(stderr, "%s v%s\n", PROGRAM, VERSION);
        exit (1);
}

void help(void)
{
        fprintf(stderr, "***************\n"                                                      );
        fprintf(stderr, "* %s v%s *\n", PROGRAM, VERSION                                         );
        fprintf(stderr, "***************\n"                                                      );
        fprintf(stderr, "\n"                                                                     );
        fprintf(stderr, "Input (XML) is taken on stdin.\n"                                       );
        fprintf(stderr, "Match patterns should be provided as parameters.\n"                     );
        fprintf(stderr, "\n"                                                                     );
        fprintf(stderr, "Options:\n"                                                             );
        fprintf(stderr, "  -h --help    : This Help message.\n"                                  );
        fprintf(stderr, "  -v --version : Program name and version.\n"                           );
        fprintf(stderr, "  -V --verbose : Output (match) everything.\n"                          );
        fprintf(stderr, "  -l --html    : HTML parsing mode, previously called \"Lax\" mode...\n");
        exit (1);
}

int main(const int argc, const char **argv)
{
        int html    = false;
        int verbose = false;

        int i = 0;

        if (argc > 1) {
                for (i = 1; i < argc; ++i) {
                        if (strcmp(argv[i], "-l") == 0 ||  strcmp(argv[i], "--html") == 0) {
                                html = true;
                        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
                                verbose = true;
                        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                                help();
                        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
                                version();
                        }
                }
        } else {
                help();
        }

        MODE mode = MODE_UNK;

        int in_match    = false;
        int escaped     = false;
        int html_script = false;

        int matches = 0;
        int delta   = 0;
        int line_no = 0;

        char *c   = NULL;
        char *pos = NULL;

        char line[MAX_LINE+1];
        char tag[256];
        char tag_temp[sizeof(tag)];
        char tag_srch[sizeof(tag)+2];
        char mtype[sizeof(tag)];
        char attribute[sizeof(tag)];
        char attribute_temp[sizeof(attribute)];
        char location[MAX_LINE];
        char location_temp[MAX_LINE];

        *line           =
        *tag            =
        *tag_temp       =
        *mtype          =
        *attribute      =
        *attribute_temp =
        *location       =
        *location_temp  =
            '\0';

        char last_char = '\0';

        for (LOCN_SEARCH(location); fgets(line, MAX_LINE, stdin) != NULL;) {
                ++line_no;
                c = line;
                last_char = '\0';
                while (*c != '\0') {
                        switch (*c) {
                                case '\\':
                                        escaped = true;
                                        ++c;
                                        continue;
                                        break;
                                case ' ':
                                        if (last_char == ' ') {
                                                ++c;
                                                continue;
                                        }
                                        break;
                                case '\n':
                                case '\r':
                                case '\t':
                                        *c = ' ';
                                        break;
                        }

                        if (escaped && in_match) {
                                fputc(*c, stdout);
                        } else {
                                switch (mode) {
                                        case MODE_UNK:
                                                if (*c == '<') {
                                                        MODE_CHANGE(MODE_TAG);
                                                        TAG_RESET();
                                                        break;
                                                } else if (*c == '>') {
                                                        XML_WARNING("Character is not valid");
                                                        if (in_match) {
                                                                fputc(*c, stdout);
                                                        }
                                                } else if (in_match) {
                                                        fputc(*c, stdout);
                                                }
                                                break;

                                        case MODE_TAG:
                                                in_match = false;

                                                if (validNameChar(c)) {
                                                        /* Acceptable characters for tags */
                                                        strchrcat(tag, *c);
                                                } else if (*c == '<') {
                                                        XML_INVALID("New tag is starting within existing tag");
                                                } else if (*c == '>') {
                                                        sprintf(tag_srch, ">%s>", tag);
                                                        if (*tag == '\0') {
                                                                XML_INVALID("Tag is of length zero");
                                                        } else if ((c-line > 1 && *(c-1) == '/') || (html && strstr(HTML_SELFCLOSE, tag_srch) != NULL)) {
                                                                /* Self closing tag */
                                                                sprintf(location_temp, "%s/%s", location, tag);
                                                                LOCN_SEARCH(location_temp);
                                                                MODE_CHANGE(MODE_UNK);
                                                                TAG_RESET();
                                                                in_match = false;

                                                                #ifdef DEBUG
                                                                        fprintf(stderr, "DEBUG: %d,%d\tTag SelfClose: \"%s\"\t\tLocation: \"%s\"\n", line_no, c-line+1, tag, location);
                                                                #endif
                                                        } else {
                                                                pos = strrchr(location, '/');
                                                                /* Compensate for HTML4's block rules */
                                                                if (html && strstr(HTML_BLOCK_LEVEL, tag_srch) != NULL) {
                                                                        if (pos != NULL) {
                                                                                sprintf(tag_srch, ">%s>", pos+1);
                                                                                if (strstr(HTML_INLINE, tag_srch) != NULL) {
                                                                                        /* Block level items cannot start within inline elements */
                                                                                        #ifdef DEBUG
                                                                                                fprintf(stderr, "DEBUG: %d,%d block level elements (%s) cannot start within inline elements (%s)", line_no, c-line+1, tag, pos+1);
                                                                                        #endif
                                                                                        *pos = '\0';
                                                                                } else if (strstr(HTML_EITHER, tag_srch) != NULL) {
                                                                                        strcpy(location_temp, location);
                                                                                        pos = strrchr(location_temp, '/');
                                                                                        if (pos != NULL) {
                                                                                                *pos = '\0';
                                                                                                pos = strrchr(location_temp, '/');
                                                                                                sprintf(tag_srch, ">%s>", pos+1);
                                                                                                if (strstr(HTML_INLINE, tag_srch) != NULL) {
                                                                                                        /* Block level items cannot start within inline elements (and whilst the parent is either, it started inside an inline element and is therefore classed as inline) */
                                                                                                        pos = strrchr(location, '/');
                                                                                                        #ifdef DEBUG
                                                                                                                fprintf(stderr, "DEBUG: %d,%d block level elements (%s) cannot start within inline elements (%s)", line_no, c-line+1, tag, pos+1);
                                                                                                        #endif
                                                                                                        *pos = '\0';
                                                                                                }
                                                                                        }
                                                                                }
                                                                        }
                                                                }

                                                                if (html && pos != NULL && strcmp(pos+1, "script") == 0) {
                                                                        #ifdef DEBUG
                                                                                fprintf(stderr, "DEBUG: Entering HTML Script mode.\n");
                                                                        #endif
                                                                        html_script = true;
                                                                } else if (!html || !html_script) {
                                                                        sprintf(tag_srch, ">%s>", tag);
                                                                        if (html && strstr(HTML_RESTARTING, tag_srch) != NULL && strcmp(pos+1, tag) == 0) {
                                                                                /* Do nothing -- we don't want nesting of <li> et al */
                                                                        } else {
                                                                                /* update location */
                                                                                strcat(location, "/");
                                                                                strcat(location, tag);
                                                                        }
                                                                        LOCN_SEARCH(location);
                                                                        MODE_CHANGE(MODE_UNK);
                                                                        #ifdef DEBUG
                                                                                fprintf(stderr, "DEBUG: %d,%d\tTag Open: \"%s\"\t\tLocation: \"%s\"\n", line_no, c-line+1, tag, location);
                                                                        #endif
                                                                }
                                                                TAG_RESET();
                                                        }
                                                } else if (*c == '/' && delta == 1) {
                                                        MODE_CHANGE(MODE_TAG_END);
                                                } else if (html && html_script) {
                                                        MODE_CHANGE(MODE_UNK);
                                                } else if (*c == '?' && delta == 1) {
                                                        MODE_CHANGE(MODE_TAG_XML_HEAD);
                                                } else if (*c == '!' && delta == 1) {
                                                        MODE_CHANGE(MODE_TAG_COMMENT);
                                                } else if (delta == 0) {
                                                        XML_WARNING("Invalid tag name character");
                                                        if (in_match) {
                                                                fputc('<', stdout);
                                                        }
                                                } else {
                                                        MODE_CHANGE(MODE_ATTR);
                                                }
                                                break;

                                        case MODE_ATTR:
                                                if (validNameChar(c)) {
                                                        /* Acceptable characters for attrs */
                                                        strchrcat(attribute, *c);
                                                } else if (*c == '/') {
                                                        MODE_CHANGE(MODE_TAG);
                                                } else if (*c == '>') {
                                                        MODE_CHANGE(MODE_TAG);
                                                        continue; /* hop */
                                                } else if (*attribute != '\0') {
                                                        MODE_CHANGE(MODE_ATTR_PRE_E);
                                                        continue; /* hop */
                                                }
                                                break;

                                        case MODE_ATTR_PRE_E:
                                                if (*c == ' ') {
                                                        /* Do nothing */
                                                } else if (*c == '=') {
                                                        MODE_CHANGE(MODE_ATTR_POST_E);
                                                } else {
                                                        XML_INVALID("Unexpected character, expecting equals sign");
                                                }
                                                break;

                                        case MODE_ATTR_POST_E:
                                                if (*c == ' ') {
                                                        /* Do nothing */
                                                } else {
                                                        if (*c == '\'') {
                                                                MODE_CHANGE(MODE_ATTR_VAL_SQ);
                                                        } else if (*c == '"') {
                                                                MODE_CHANGE(MODE_ATTR_VAL_DQ);
                                                        } else {
                                                                MODE_CHANGE(MODE_ATTR_VAL);
                                                        }

                                                        sprintf(location_temp, "%s/%s@%s", location, tag, attribute);
                                                        LOCN_SEARCH(location_temp);

                                                        #ifdef DEBUG
                                                                fprintf(stderr, "DEBUG: %d,%d\tAttribute: \"%s\"\t\tLocation: \"%s\"\n", line_no, c-line+1, attribute, location_temp);
                                                        #endif
                                                }
                                                break;

                                        case MODE_ATTR_VAL:
                                                if (*c == '/') {
                                                        ATTR_RESET();
                                                        MODE_CHANGE(MODE_TAG);
                                                } else if (*c == '>') {
                                                        ATTR_RESET();
                                                        MODE_CHANGE(MODE_TAG);
                                                        continue; /* hop */
                                                } else if (*c == ' ') {
                                                        ATTR_RESET();
                                                        MODE_CHANGE(MODE_ATTR);
                                                } else {
                                                        if (in_match) {
                                                                fputc(*c, stdout);
                                                        }
                                                }
                                                break;

                                        case MODE_ATTR_VAL_SQ:
                                                if (*c == '\'') {
                                                        ATTR_RESET();
                                                        MODE_CHANGE(MODE_ATTR);
                                                } else {
                                                        if (in_match) {
                                                                fputc(*c, stdout);
                                                        }
                                                }
                                                break;

                                        case MODE_ATTR_VAL_DQ:
                                                if (*c == '"') {
                                                        ATTR_RESET();
                                                        MODE_CHANGE(MODE_ATTR);
                                                } else {
                                                        if (in_match) {
                                                                fputc(*c, stdout);
                                                        }
                                                }
                                                break;

                                        case MODE_TAG_END:
                                                if (validNameChar(c)) {
                                                        /* Acceptable characters for tags */
                                                        strchrcat(tag, *c);
                                                } else if (*c == '<') {
                                                        XML_INVALID("New tag is starting within existing tag");
                                                } else if (*c == '>') {
                                                        pos = strrchr(location, '/');
                                                        if(pos == NULL) {
                                                                XML_INVALID("Closing of tag \"%s\" before any tags have opened", tag);
                                                        } else if (strcmp(pos+1, tag) == 0) {
                                                                if (html && strcmp(pos+1, "script") == 0) {
                                                                        #ifdef DEBUG
                                                                                fprintf(stderr, "DEBUG: Leaving HTML Script mode.\n");
                                                                        #endif
                                                                        html_script = false;
                                                                }

                                                                *pos = '\0';
                                                                LOCN_SEARCH(location);

                                                                #ifdef DEBUG
                                                                        fprintf(stderr, "DEBUG: %d,%d\tTag Close: \"%s\"\t\tLocation: \"%s\"\n", line_no, c-line+1, tag, location);
                                                                #endif
                                                        } else {
                                                                XML_INVALID("Closing of incorrect tag encountered (expecting \"%s\", got \"%s\")", pos+1, tag);
                                                                #ifdef DEBUG
                                                                        if (html && strcmp(pos+1, "script") != 0) fprintf(stderr, "Suggestion: It may be worth adding the tag \"%s\" to the list of self closing tags\n", pos+1);
                                                                #endif
                                                                /* Search backwards through location and terminate the previous occurrance since some shitfuck clearly screwed up, then LOCN_SEARCH() */
                                                                sprintf(tag_srch, "/%s/", tag);
                                                                pos = strrstr(location, tag_srch);
                                                                if (pos != NULL) {
                                                                        locationSlice(pos);
                                                                        LOCN_SEARCH(location);
                                                                }
                                                        }
                                                        MODE_CHANGE(MODE_UNK);
                                                } else {
                                                        XML_INVALID("Properties detected in ending tag");
                                                }
                                                break;

                                        case MODE_TAG_XML_HEAD:
                                                if (*c == '>') {
                                                        MODE_CHANGE(MODE_UNK);
                                                }
                                                break;

                                        case MODE_TAG_COMMENT:
                                                if (delta == 2 && *c == '[') {
                                                        MODE_CHANGE(MODE_TAG_MARKED);
                                                } else if ((delta == 1 || delta == 2) && *c != '-') {
                                                        MODE_CHANGE(MODE_TAG_FA);
                                                } else if (*c == '>' && c-line > 1 && *(c-2) == '-' && *(c-1) == '-') {
                                                        MODE_CHANGE(MODE_UNK);
                                                }
                                                break;

                                        case MODE_TAG_FA:
                                                if (*c == '>') {
                                                        MODE_CHANGE(MODE_UNK);
                                                }
                                                break;

                                        case MODE_TAG_MARKED:
                                                if (validNameChar(c)) {
                                                        MODE_CHANGE(MODE_TAG_M_TYPE);
                                                        continue; /* hop */
                                                }
                                                break;

                                        case MODE_TAG_M_TYPE:
                                                if (validNameChar(c)) {
                                                        /* Acceptable characters for tags */
                                                        strchrcat(mtype, *c);
                                                } else if (strcmp(mtype, "cdata") == 0) {
                                                        MTYPE_RESET();
                                                        MODE_CHANGE(MODE_CDATA);
                                                        continue; /* hop */
                                                } else {
                                                        MTYPE_RESET();
                                                        MODE_CHANGE(MODE_TAG_M_CONTENT);
                                                }
                                                break;

                                        case MODE_TAG_M_CONTENT:
                                                if (*c == '>' && c-line > 0 && *c == ']') {
                                                        MODE_CHANGE(MODE_UNK);
                                                }
                                                break;

                                        case MODE_CDATA:
                                                if (*c == '[') {
                                                        MODE_CHANGE(MODE_CDATA_CONTENT);
                                                        LOCN_SEARCH(location);
                                                } else {
                                                        XML_INVALID("Invalid CDATA, expecting '['");
                                                }
                                                break;

                                        case MODE_CDATA_CONTENT:
                                                if (*c == ']') {
                                                        MODE_CHANGE(MODE_CDATA_PE);
                                                } else  if (*c == '>' && c-line > 1 && *(c-1) == ']' && *(c-2) == ']') {
                                                        MODE_CHANGE(MODE_UNK);
                                                } else if (in_match) {
                                                        fputc(*c, stdout);
                                                }
                                                break;

                                        case MODE_CDATA_PE:
                                                if (*c == ']') {
                                                        MODE_CHANGE(MODE_TAG_FA);
                                                } else {
                                                        if (in_match) {
                                                                fputc(*c, stdout);
                                                        }
                                                        MODE_CHANGE(MODE_CDATA_CONTENT);
                                                }
                                                break;

                                        default:
                                                fprintf(stderr, "%s:%d Error: An unknown mode was encountered at line %d, character %d ('%c').\n",
                                                        __FILE__, __LINE__, line_no, c-line+1, *c);
                                                exit (1);
                                                break;
                                }
                        }
                        last_char = *c;
                        ++c;
                        ++delta;
                        escaped = false;
                }
        }

        if (*location != '\0') {
                XML_INVALID("Input ended early; location still judged as \"%s\"", location);
        }

        if (matches > 0) fputc('\n', stdout);
        return 0;
}

void xmlInvalid(const char *sfile, const int sline, const int line_no, const int char_no, const int warning, const int html, const int in_match, const char c, const char *message, ...)
{
        if (html && in_match) {
                fputc(c, stdout);
        }

        #ifndef DEBUG
                if (html) return;
        #endif

        char msg_buf[MAX_LINE];
        *msg_buf = '\0';
        va_list argp;
        va_start(argp, message);
        vsnprintf(msg_buf, sizeof(msg_buf), message, argp);
        va_end(argp);

        #ifdef DEBUG
                fprintf(stderr, "%s:%d: Invalid XML at line %d, character %d ('%c'). %s.\n", sfile, sline, line_no, char_no, c, msg_buf);
        #else
                fprintf(stderr, "Invalid XML at line %d, character %d ('%c'). %s.\n", line_no, char_no, c, msg_buf);
        #endif

        #ifdef DEBUG
                if (html) return;
        #endif

        if (!warning) {
                exit (1);
        }
}

void modeChange(const char *sfile, const int sline, const int line_no, const int char_no, const char c, MODE *mode, const MODE change)
{
        #ifdef DEBUG
                fprintf(stderr, "%s:%d: Mode changing from [%d] to [%d] at line %d, character %d ('%c')\n", sfile, sline, *mode, change, line_no, char_no, c);
        #endif
        *mode = change;
}

void searchLocation(const int argc, const char **argv, const int verbose, int *matches, int *in_match, const char *location)
{
        char loc[MAX_LINE];
        if (*location == '\0') {
                strcpy(loc, "/");
        } else {
                strcpy(loc, location);
        }

        if (verbose) {
                if (*matches > 0) fputc('\n', stdout);
                *matches = 1;
                *in_match = true;

                printf("%s: ", loc);
        } else {
                int i;

                for (i = 1; i < argc; ++i) {
                        if (strcmp(loc, argv[i]) == 0) {
                                if (*matches > 0) fputc('\n', stdout);
                                ++(*matches);
                                *in_match = true;
                                printf("%s: ", loc);
                                break;
                        }
                }
        }
}

void strchrcat(char *str, const char c)
{
        int len    = strlen(str);
        str[len]   = c;
        str[len+1] = '\0';
}

int validNameChar(char *c)
{
        if ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == ':' || *c == '-') {
                if (*c >= 'A' && *c <= 'Z') {
                        *c = *c + ('a' - 'A');
                }
                return true;
        }
        return false;
}

char *strrstr(const char *haystack, const char *needle)
{
        char *h  = (char *)haystack;
        char *ne = (char *)needle;

        for (; *h  != '\0';  ++h) {
        }

        for (; *ne != '\0'; ++ne) {
        }

        if (h == (char *)haystack || ne == (char *)needle) {
                return NULL;
        }

        char *nt = ne;

        /* move back over haystack char by char */
        for (; h != haystack;) {
                --h;
                --nt;

                /* compare h to nt */
                if (*h != *nt) {
                        /* if they're not the same char reset nt to ne */
                        nt = ne;
                } else if (nt == needle) {
                        /* if nt == needle return h */
                        return h;
                }
        }

        return NULL;
}

void locationSlice(char *s)
{
        char *e = strchr(s+1, '/');
        if (e == NULL) {
                return;
        }

        for (++s, ++e;; ++s, ++e) {
                *s = *e;
                if (*e == '\0') {
                        return;
                }
        }
}
