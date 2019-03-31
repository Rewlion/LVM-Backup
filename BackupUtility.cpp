#include "LvmBackuper.h"
#include "Ast.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

struct yy_buffer_state;

extern int              yyparse(Lvm::Ast::MetadataNode* mtd);
extern yy_buffer_state* yy_scan_string(const char* str);
extern void             yy_delete_buffer(yy_buffer_state* buffer);

namespace
{
	Lvm::RawMetadata ReadRawMetadata(const char* path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
    	if (file.is_open() == false)
    	{
	    	std::cerr << "unable to read file `"<< path << "` with metadata.";
    		exit(-1);
    	}

    	const size_t size = static_cast<size_t>(file.tellg()) + 1;
    	file.seekg(std::ios::beg);

    	std::vector<char> raw(size);
    	file.read(raw.data(), size);
    	raw.push_back('\0');

    	return raw;
	}

	Lvm::Ast::MetadataNode* ParseRawMetadata(const Lvm::RawMetadata& raw)
	{
		auto mtd = new Lvm::Ast::MetadataNode;
		yy_buffer_state* s = yy_scan_string(raw.data());
		yyparse(mtd);
		yy_delete_buffer(s);

		return mtd;
	}
}

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		std::cerr << "argument: path to lvm's metadata file.\n";
		exit(-1);
	}

	Lvm::Metadata mtd;

	Lvm::Ast::MetadataNode* ast = ParseRawMetadata(ReadRawMetadata(argv[1]));
	Lvm::Ast::AstToMetaConverter converter;
	converter.SetMetadata(&mtd);
	ast->Accept(static_cast<Lvm::Ast::AstVisitor*>(&converter));

	Lvm::BackupSettings s;
	Lvm::Backuper b;
}
