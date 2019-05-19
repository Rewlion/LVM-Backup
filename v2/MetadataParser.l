%{
    #include <cstdio>

    #include "MetadataParser.tab.h"  // to get the token types from Bison

    extern void yyerror(Lvm::Ast::MetadataNode* mtd, const char* yytext);

//#define DEBUG_FLEX
#ifndef DEBUG_FLEX
    #define YIELD_TOKEN(tok) \
        return TOKEN_##tok

    #define YIELD_UQ_STOKEN(tok) \
        do {\
            const size_t l = strlen(yytext);\
            yytext[l-1] = '\0';\
            yylval.sval = strdup(yytext+1);\
            return TOKEN_##tok;\
        } while(0)

    #define YIELD_STOKEN(tok) \
        yylval.sval = strdup(yytext);return TOKEN_##tok
#else
    #define YIELD_TOKEN(tok) \
        printf("TOKEN_%s\n", #tok);return TOKEN_##tok

    #define YIELD_UQ_STOKEN(tok) \
        do {\
            printf("TOKEN_%s\n", #tok);\
            const size_t l = strlen(yytext);\
            yytext[l-1] = '\0';\
            yylval.sval = strdup(yytext+1);\
            return TOKEN_##tok;\
        } while(0)

    #define YIELD_STOKEN(tok) \
        printf("TOKEN_%s\n", #tok);yylval.sval = strdup(yytext);return TOKEN_##tok
#endif


%}
%option noyywrap
%option yylineno
%x COMMENT
%%

#                        {BEGIN(COMMENT);}
<COMMENT>.               {}
<COMMENT>\n              {BEGIN(INITIAL);}

physical_volumes         { YIELD_TOKEN(PHYSICAL_VOLUMES);}
logical_volumes          { YIELD_TOKEN(LOGICAL_VOLUMES );}
\[                       { YIELD_TOKEN(LEFT_BRACKET    );}
\]                       { YIELD_TOKEN(RIGHT_BRACKET   );}
\{                       { YIELD_TOKEN(LEFT_BRACE      );}
\}                       { YIELD_TOKEN(RIGHT_BRACE     );}
\=                       { YIELD_TOKEN(EQUAL           );}
\,                       { YIELD_TOKEN(COMMA           );}

[0-9]+                   { YIELD_STOKEN(INT_VALUE      );}
[0-9]+\.[0-9]+           { YIELD_STOKEN(FLOAT_VALUE    );}

\"([\* a-zA-Z0-9\-\_/])*\" { YIELD_UQ_STOKEN(STRING_VALUE);}
[a-zA-Z0-9\-\_]+         { YIELD_STOKEN(NAME           );}


[ \t]  {}
\n     { ++yylineno; }

.      {
            char error[128] {};
            snprintf(error, 128, "line %d: lexer error, unknown lexem: `%s`", yylineno, yytext);
            yyerror(NULL, error);
        }
%%

