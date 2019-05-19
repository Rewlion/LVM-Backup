main:
	bison -d MetadataParser.y --report=state
	flex MetadataParser.l
	g++ -g Ast.cpp BackupUtility.cpp LvmBackuper.cpp MetadataParser.tab.c lex.yy.c -std=c++17 -lfl -o parser


clean:
	rm lex.yy.c MetadataParser.tab.*
