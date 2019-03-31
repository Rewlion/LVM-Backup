%{
  #include "MetadataParser.tab.h"
  #include <cstdio>
  #include <iostream>
  using namespace std;
  // Declare stuff from Flex that Bison needs to know about:
  extern int yylex();
  extern int yyparse(Lvm::Ast::MetadataNode* mtd);
  extern FILE *yyin;
  extern int yylineno;
 
  void yyerror(Lvm::Ast::MetadataNode* mtd, const char *s);
%}

%union {
  int   ival;
  float fval;
  char *sval;
  
  Lvm::Ast::ValueNode*                  valueNode;
  Lvm::Ast::ParameterNode*              parameterNode;
  Lvm::Ast::SubSectionNode*             subSectionNode;
  Lvm::Ast::LogicalVolumeNode*          logicalVolumeNode;
  Lvm::Ast::LogicalVolumeSectionNode*   logicalVolumeSectionNode;
  Lvm::Ast::PhysicalVolumeSectionNode*  physicalVolumeSectionNode;
  Lvm::Ast::VolumeGroupSectionsNode*    volumeGroupSectionsNode;
  Lvm::Ast::VolumeGroupNode*            volumeGroupNode;
}

%parse-param { Lvm::Ast::MetadataNode* mtd }

%code requires {
    #include "Ast.h"

    namespace Lvm::Ast
    {
        class MetadataNode;
    }
}

%code {
    #include "Utility.h"
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

%type <valueNode>                 ARRAY
%type <valueNode>                 PAIR
%type <valueNode>                 VALUE
%type <valueNode>                 STRING_LIST
%type <parameterNode>             PARAMETER
%type <parameterNode>             PARAMETERS
%type <subSectionNode>            SUB_SECTION
%type <subSectionNode>            SUB_SECTIONS_LIST
%type <logicalVolumeNode>         LOGICAL_VOLUME
%type <logicalVolumeNode>         LOGICAL_VOLUMES_LIST
%type <logicalVolumeSectionNode>  LOGICAL_VOLUME_SECTION
%type <physicalVolumeSectionNode> PHYSICAL_VOLUME_SECTION
%type <volumeGroupSectionsNode>   VOLUME_GROUP_SECTIONS
%type <volumeGroupNode>           VOLUME_GROUP

%%

LVM_METADATA
    : PARAMETERS VOLUME_GROUP                                  { mtd->Parameters = $1; mtd->VolumeGroup = $2; }

VOLUME_GROUP
	: TOKEN_NAME "{" PARAMETERS VOLUME_GROUP_SECTIONS "}"      { $$ = new Lvm::Ast::VolumeGroupNode(new Lvm::Ast::NameNode($1), $3, $4); free($1);}

VOLUME_GROUP_SECTIONS
	: PHYSICAL_VOLUME_SECTION LOGICAL_VOLUME_SECTION           { $$ = new Lvm::Ast::VolumeGroupSectionsNode($1, $2);}
	| LOGICAL_VOLUME_SECTION PHYSICAL_VOLUME_SECTION           { $$ = new Lvm::Ast::VolumeGroupSectionsNode($2, $1); }
	| PHYSICAL_VOLUME_SECTION                                  { $$ = new Lvm::Ast::VolumeGroupSectionsNode($1); }

PHYSICAL_VOLUME_SECTION
	: "physical_volumes" "{" SUB_SECTIONS_LIST "}"             { $$ = new Lvm::Ast::PhysicalVolumeSectionNode($3);}

LOGICAL_VOLUME_SECTION
	:  "logical_volumes"  "{" LOGICAL_VOLUMES_LIST "}"         { $$ = new Lvm::Ast::LogicalVolumeSectionNode($3); }

LOGICAL_VOLUMES_LIST
	: LOGICAL_VOLUME LOGICAL_VOLUMES_LIST                      { $$ = $1; $$->Next = $2; }
	| LOGICAL_VOLUME                                           { $$ = $1; }

LOGICAL_VOLUME
	: TOKEN_NAME "{" PARAMETERS SUB_SECTIONS_LIST "}"          { $$ = new Lvm::Ast::LogicalVolumeNode(new Lvm::Ast::NameNode($1), $3, $4); free($1); }

SUB_SECTIONS_LIST
	: SUB_SECTIONS_LIST SUB_SECTION                            { $$ = $2; $$->Next = $1; }
	| SUB_SECTION                                              { $$ = $1; }

SUB_SECTION
	: TOKEN_NAME "{" PARAMETERS "}"                            { $$ = new Lvm::Ast::SubSectionNode(new Lvm::Ast::NameNode($1), $3); free($1);}

PARAMETERS
	: PARAMETERS PARAMETER                                     { $$ = $2; $$->Next = $1;}
	| PARAMETER                                                { $$ = $1;}

PARAMETER
	: TOKEN_NAME "=" VALUE                                     { $$ = new Lvm::Ast::ParameterNode(new Lvm::Ast::NameNode($1), $3); free($1); }

VALUE
	: TOKEN_INT_VALUE                                          { $$ = new Lvm::Ast::ValueNode( $1 ); }
	| TOKEN_FLOAT_VALUE                                        { $$ = new Lvm::Ast::ValueNode( $1 ); }
	| TOKEN_STRING_VALUE                                       { $$ = new Lvm::Ast::ValueNode( $1 ); free($1); }
	| ARRAY                                                    { $$ = $1; }
	| PAIR                                                     { $$ = $1; }

ARRAY
	: "[" STRING_LIST "]"                                      { $$ = $2; }
	| "[" "]"                                                  { $$ = new Lvm::Ast::ValueNode{ Lvm::Array{} };}

STRING_LIST
	: TOKEN_STRING_VALUE "," STRING_LIST                       { Lvm::Array& a = std::get<Lvm::Array>($3->Value); a.push_back($1); free($1); $$ = $3;}
	| TOKEN_STRING_VALUE                                       { Lvm::Array a; a.push_back($1); free($1); $$ = new Lvm::Ast::ValueNode{a}; }

PAIR
	: "[" TOKEN_STRING_VALUE "," TOKEN_INT_VALUE "]"           { $$ = new Lvm::Ast::ValueNode( Lvm::Pair{$2, $4} ); free($2); }

%%

void yyerror(Lvm::Ast::MetadataNode* mtd, const char *s) 
{
  cout << "EEK, parse error!  Message: " << s << endl;
  // might as well halt now:
  exit(-1);
}