#pragma once

#include "Utility.h"
#include <string>

namespace Lvm
{
	class Backuper
	{
	public:
		void                   Backup(const BackupSettings& settings) const;
	private:
    	PhysicalVolumeMetadata ReadPhysicalVolumeMetadata(const std::string& dev);
	};
}