#include "LvmBackuper.h"

#include <unistd.h>
#include <iostream>

int main(int argc, char** argv)
{
	Lvm::Backup::Settings s;
	s.BackupsFolder = "~/LvmBackups/"; //TODO: use
	Lvm::Backup::Backuper b;

	const int opt = getopt(argc, argv, "r:b:");
	switch(opt)
	{
		case 'r':
		{
			b.Restore(optarg);		
			return 0;
		}

		case 'b':
		{	
			b.Backup(s, {optarg});
			return 0;
		}

		default:
		{
			std::printf("Usage: %s [-r archive_name | -b physical_volume {e.g. /dev/sdb}]\n"
						"-r: restore lvm2 from the archive.\n"
						"-b: backup lvm2 to the archive.\n", argv[0]);
			return -1;
		}
	}
}
