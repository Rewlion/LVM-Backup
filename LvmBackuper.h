#pragma once

#include "Utility.h"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <optional>

namespace Lvm::Backup
{
	struct SegmentDescription
	{
		uint64_t StartExtent;
		uint64_t ExtentCount;		
	};

    struct SegmentDescriptor
    {
        uint64_t StartExtent;
        uint64_t DataSize;
    } __attribute__((packed));

	struct PVSegmentsDescription
	{
		std::vector<SegmentDescription> Segments;
	};

	typedef std::unordered_map<std::string, PVSegmentsDescription> PVSegmentsMap;


    struct PhysicalVolumeLabelHeader
    {
        uint64_t Signature;
        uint64_t SectorNumber;
        uint32_t CheckSum;
        uint32_t DataOffset; 
        uint64_t TypeIndicator;
    } __attribute__((packed));

    struct DataAreaDescriptor
    {
        uint64_t DataAreaOffset = 0;
        uint64_t DataAreaSize = 0;
    }__attribute__((packed));

    struct PhysicalVolumeHeader
    {
        char     ID[32];
        uint64_t VolumeSize;
        std::vector<DataAreaDescriptor> DataDescriptors;
        std::vector<DataAreaDescriptor> MetadataDescriptors;
        char     Rest[128];
    };

    struct LocationDescriptor
    {
    	uint64_t DataAreaOffset = 0;
    	uint64_t DataAreaSize = 0;
    	uint32_t CheckSum = 0;
    	uint32_t Flags = 0;
    }__attribute__((packed));

    struct MtdHeader
    {
        uint32_t CheckSum;
        char     Signature[32];
        uint32_t Version;
    }__attribute__((packed));

	struct PvInformation
	{
        std::string                     PvName;
        std::string                     Device;
		PhysicalVolumeLabelHeader 		Label;
		PhysicalVolumeHeader 			PvHeader;
		MtdHeader                       m_MtdHeader;
        std::vector<LocationDescriptor> LocationDescriptors;
		std::vector<SegmentDescription> Segments;	
	};

    typedef std::unordered_map<std::string, PvInformation> PvInformationMap;

    struct VgInformation
    {
        std::string Name;
        std::vector<PvInformation> PvInfos;
        std::vector<char> RawMtd;
        uint64_t ExtentSize;
    };

    typedef std::unordered_map<std::string, Lvm::Metadata> VgMtdMap;

	struct Settings
	{
		std::string BackupsFolder;
	};

	class Backuper
	{
	public:
		void Backup(const Settings& settings,const RawMetadata& rawMtd, const Lvm::Metadata& mtd) const;
        void DumpMtd(const std::string& dev) const;

        void Backup(const Settings& settings, const std::vector<std::string>& devices) const;
        void Restore(const std::string& acrhive) const;
	private:
        void ConstructArchive(const VgInformation& v, const std::string& f) const;

        VgMtdMap CollectVgMtds(const std::vector<std::string>& devices) const;
        std::optional<Metadata> ReadVgMtd(const std::string& dev) const;
        std::optional<std::vector<char>> ReadRawMtd(std::ifstream& f) const; 

        std::optional<PhysicalVolumeLabelHeader> ReadPvLabel(std::ifstream& f) const;
        std::vector<DataAreaDescriptor> ReadDataArea(std::ifstream& f) const;
        PhysicalVolumeHeader ReadPvHeader(std::ifstream& f) const;
        MtdHeader ReadMtdHeader(std::ifstream& f) const;
        std::vector<LocationDescriptor> ReadLocationsArea(std::ifstream& f) const;

		std::optional<PvInformation> ReadPvInformation(const std::string& dev) const;
		PvInformationMap CollectPvsInformation(const Lvm::Metadata& mtd) const;
		PVSegmentsMap CollectSegmentsDescription(const Lvm::Metadata& mtd) const;
	};
}