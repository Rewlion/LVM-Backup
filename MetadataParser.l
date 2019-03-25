%{
    #include <cstdio>

    #include "MetadataParser.tab.h"  // to get the token types from Bison

    extern void yyerror(const char* yytext);
%}
%option noyywrap
%option yylineno
%x COMMENT
%%

#                     {BEGIN(COMMENT);}
<COMMENT>.            {}
<COMMENT>\n           {BEGIN(INITIAL);}

physical_volumes      {                              return TOKEN_PHYSICAL_VOLUMES;}
logical_volumes       {                              return TOKEN_LOGICAL_VOLUMES;}
\[                    {                              return TOKEN_LEFT_BRACKET;}
\]                    {                              return TOKEN_RIGHT_BRACKET;}
\{                    {                              return TOKEN_LEFT_BRACE;}
\}                    {                              return TOKEN_RIGHT_BRACE;}
\=                    {                              return TOKEN_EQUAL;}
\,                    {                              return TOKEN_COMMA;}

[0-9]+                { yylval.ival = atoi(yytext);  return TOKEN_INT_VALUE; }
[0-9]+\.[0-9]+        { yylval.fval = atoi(yytext);  return TOKEN_FLOAT_VALUE; }

\"[a-zA-Z0-9\-\_/]*\" {yylval.sval = strdup(yytext); return TOKEN_STRING_VALUE;}
[a-zA-Z0-9\-\_]+      {yylval.sval = strdup(yytext); return TOKEN_NAME;}


[ \t]  {}
\n     { ++yylineno; }

.      {
            char error[128] {};
            snprintf(error, 128, "line %d: lexer error, unknown lexem: `%s`", yylineno, yytext);
            yyerror(error);
        }
%%

