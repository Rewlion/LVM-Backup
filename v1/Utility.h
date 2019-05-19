#pragma once

#include <string>
#include <map>
#include <variant>
#include <vector>
#include <optional>

namespace Lvm
{	
	struct Pair
	{
		std::string Name;
		int Number;
	};

	typedef std::vector<char> RawMetadata;
	typedef std::vector<std::string> Array;
	typedef std::variant<Pair, Array, std::string, int, float> Value;

	typedef std::map<std::string, Value> ParameterMap;

	struct PhysicalVolumeMetadata
	{

	};

	struct SubSection
	{
		std::string  Name;
		ParameterMap Parameters;
	};
	typedef SubSection PhysicalVolume;

	struct LogicalVolume
	{
		std::string 			Name;
		ParameterMap 		    Parameters;
		std::vector<SubSection> Segments;
	};

	struct VolumeGroup
	{
		std::string 				Name;
		ParameterMap 		    	Parameters;
		std::vector<PhysicalVolume> PhysicalVolumes;
		std::vector<LogicalVolume>  LogicalVolumes;
	};

	struct Metadata
	{
		ParameterMap  Parameters;
		VolumeGroup   VG;

		std::vector<char> Raw;
	};

	class Archive
	{

	};
}