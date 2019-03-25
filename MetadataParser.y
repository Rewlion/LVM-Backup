%{
  #include <cstdio>
  #include <iostream>
  using namespace std;
  // Declare stuff from Flex that Bison needs to know about:
  extern int yylex();
  extern int yyparse();
  extern FILE *yyin;
  extern int yylineno;
 
  void yyerror(const char *s);
%}

%union {
  int   ival;
  float fval;
  char *sval;
}

%token <ival> TOKEN_INT_VALUE
%token <fval> TOKEN_FLOAT_VALUE
%token <sval> TOKEN_STRING_VALUE
%token <sval> TOKEN_NAME

%token TOKEN_PHYSICAL_VOLUMES "physical_volumes"
%token TOKEN_LOGICAL_VOLUMES  "logical_volumes"

%token TOKEN_LEFT_BRACKET  "["
%token TOKEN_RIGHT_BRACKET "]"
%token TOKEN_LEFT_BRACE    "{"
%token TOKEN_RIGHT_BRACE   "}"
%token TOKEN_EQUAL         "="
%token TOKEN_COMMA         ","

%%

VOLUME_GROUP
	: TOKEN_NAME "{" PARAMETERS VOLUME_GROUP_SECTIONS "}" 

VOLUME_GROUP_SECTIONS
	: PHYSICAL_VOLUME_SECTION LOGICAL_VOLUME_SECTION 
	| LOGICAL_VOLUME_SECTION PHYSICAL_VOLUME_SECTION 
	| PHYSICAL_VOLUME_SECTION 
	| LOGICAL_VOLUME_SECTION 

PHYSICAL_VOLUME_SECTION
	: "physical_volumes" "{" SUB_SECTIONS_LIST "}" 

LOGICAL_VOLUME_SECTION
	:  "logical_volumes"  "{" LOGICAL_VOLUMES_LIST "}" 

LOGICAL_VOLUMES_LIST
	: LOGICAL_VOLUME LOGICAL_VOLUMES_LIST 
	| LOGICAL_VOLUME 

LOGICAL_VOLUME
	: TOKEN_NAME "{" PARAMETERS SUB_SECTIONS_LIST "}" 

SUB_SECTIONS_LIST
	: SUB_SECTIONS_LIST SUB_SECTION 
	| SUB_SECTION 

SUB_SECTION
	: TOKEN_NAME "{" PARAMETERS "}" 

PARAMETERS
	: PARAMETERS PARAMETER {}
	| PARAMETER {}

PARAMETER
	: TOKEN_NAME "=" VALUE 

VALUE
	: TOKEN_INT_VALUE
	| TOKEN_FLOAT_VALUE 
	| TOKEN_STRING_VALUE 
	| ARRAY
	| PAIR

ARRAY
	: "[" STRING_LIST "]" 
	| "[" "]" 

STRING_LIST
	: TOKEN_STRING_VALUE "," STRING_LIST 
	| TOKEN_STRING_VALUE 

PAIR
	: "[" TOKEN_STRING_VALUE "," TOKEN_INT_VALUE "]"

%%

int main(int, char**) 
{
  
}

void yyerror(const char *s) {
  cout << "EEK, parse error!  Message: " << s << endl;
  // might as well halt now:
  exit(-1);
}