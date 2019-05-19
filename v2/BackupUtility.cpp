#include "Agent.h"

#include <unistd.h>
#include <iostream>

int main(int argc, char** argv)
{
	Lvm::Backup::Agent agent;

	const int opt = getopt(argc, argv, "r:b:");
	switch(opt)
	{
		case 'r':
		{
			agent.Restore(optarg);		
			return 0;
		}

		case 'b':
		{	
			agent.Backup({optarg});
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
