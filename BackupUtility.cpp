#include "Agent.h"

#include <unistd.h>
#include <iostream>
#include <assert.h>

const std::vector<std::string> devices = {
	"/dev/sdb"
};

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
			agent.Backup(optarg, devices);
			return 0;
		}

		default:
		{
			std::printf("Usage: %s [-r archive_name | -b logical_volume {e.g. /dev/SomeVolumeGroup/LV1}]\n"
						"-r: restore lv from the archive.\n"
						"-b: backup lv to the archive.\n", argv[0]);
			return -1;
		}
	}
}
