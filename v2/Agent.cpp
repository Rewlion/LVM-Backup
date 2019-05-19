#include "Agent.h"

#include <iostream>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <assert.h>
////////////////////////////
#include "Ast.h"
struct yy_buffer_state;

extern int              yyparse(Lvm::Ast::MetadataNode* mtd);
extern yy_buffer_state* yy_scan_string(const char* str);
extern void             yy_delete_buffer(yy_buffer_state* buffer);
////////////////////////////

#define ARCHIVE_MAGIC (uint64_t)24900942
#define ARCHIVE_VG_MAGIC (uint64_t)35100153
#define ARCHIVE_PV_MAGIC (uint64_t)57100175

#define SECTOR_SIZE (uint64_t)512

#define LVM_BACKUP_SIGNATURE (uint64_t)24900249
#define LVM_PHYSICAL_SIGNATURE (uint64_t)54100145

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define LVM64_TO_CPU(x) ( x = __builtin_bswap64(x) )
    #define LVM32_TO_CPU(x) ( x = __builtin_bswap32(x) )
    #define LVM16_TO_CPU(x) ( x = __builtin_bswap16(x) )
#else
    #define LVM64_TO_CPU(x) ( x )
    #define LVM32_TO_CPU(x) ( x )
    #define LVM16_TO_CPU(x) ( x )
#endif

namespace
{
    const char PV_SIGNATURE[] = "LABELONE";
    const char PV_TYPEINDICATOR[] = "LVM2\x20001";

    void DumpSegmentsMap(const Lvm::Backup::PVSegmentsMap& sMap)
    {
        for(auto& p : sMap)
        {
            std::cout << "device:" << p.first << "segments: ";
            for(auto& s : p.second.Segments)
                std::cout << s.StartExtent << " " << s.ExtentCount << ", ";
            std::cout << std::endl;
        }
    }

    void ParseRawMetadata(const std::vector<char>& raw, Lvm::Metadata* mtd)
    {
        auto* ast = new Lvm::Ast::MetadataNode;
        yy_buffer_state* s = yy_scan_string(raw.data());
        yyparse(ast);
        yy_delete_buffer(s);

        Lvm::Ast::AstToMetaConverter converter;

        converter.SetMetadata(mtd);
        ast->Accept(&converter);
    }
}

namespace Lvm::Backup
{
    void Agent::Restore(const std::string& archive) const
    {
        
        
    }

    void Agent::Backup(const std::vector<std::string>& devices) const
    {
        VgMtdMap mtdMap = CollectVgMtds(devices);

    }

    VgMtdMap Agent::CollectVgMtds(const std::vector<std::string>& devices) const
    {
        VgMtdMap mtdMap;

        for(const auto& dev : devices)
        {
            std::optional<Metadata> mtd = ReadVgMtd(dev);
            if(mtd.has_value())
            {
                mtdMap[mtd.value().VG.Name] = mtd.value();
            }
        }

        return mtdMap;
    }

    std::optional<Metadata> Agent::ReadVgMtd(const std::string& dev) const
    {
        std::ifstream f(dev);
        assert(f.is_open());

        std::optional<std::vector<char>> rawMtd = ReadRawMtd(f);
        if(rawMtd.has_value())
        {
            Metadata mtd;
            ParseRawMetadata(rawMtd.value(), &mtd);
            mtd.Raw = rawMtd.value();

            return mtd;
        }

        return std::nullopt;
    }

    std::optional<std::vector<char>> Agent::ReadRawMtd(std::ifstream& f) const
    {
        f.seekg(0, f.beg);

        std::optional<PhysicalVolumeLabelHeader> label = ReadPvLabel(f);
        if(label.has_value() == false)
            return std::nullopt;

        PhysicalVolumeHeader pvHeader = ReadPvHeader(f);
        const size_t mdaOffset = pvHeader.MetadataDescriptors[0].DataAreaOffset;

        f.seekg(mdaOffset, f.beg);
        MtdHeader mtdHeader = ReadMtdHeader(f);

        const char* mtdHeaderMagic = " LVM2 x[5A%r0N*>";
        if(strncmp(mtdHeaderMagic, mtdHeader.Signature, 16) != 0)
            return std::nullopt;

        std::vector<LocationDescriptor> locationsArea = ReadLocationsArea(f);
        
        auto& la = locationsArea[0];
        std::vector<char> rawMtd(la.DataAreaSize + 1);
        f.seekg(mdaOffset + la.DataAreaOffset, f.beg);
        f.read(rawMtd.data(), la.DataAreaSize);
        rawMtd.push_back('\0');

        return rawMtd;        
    }

    void Agent::DumpMtd(const std::string& dev) const
    {
        std::ifstream f(dev);
        assert(f.is_open());

        std::optional<PhysicalVolumeLabelHeader> label = ReadPvLabel(f);

        assert(label.has_value());

        PhysicalVolumeHeader pvHeader = ReadPvHeader(f);
        
        f.seekg(pvHeader.MetadataDescriptors[0].DataAreaOffset, f.beg);
        MtdHeader mtdHeader = ReadMtdHeader(f);

        const char* mtdHeaderMagic = " LVM2 x[5A%r0N*>";
        assert(strncmp(mtdHeaderMagic, mtdHeader.Signature, 16) == 0);

        std::vector<LocationDescriptor> locationsArea = ReadLocationsArea(f);

        std::printf("LENGTH: %d\n", locationsArea.size());
        for(auto& la : locationsArea)
        {
            f.seekg(pvHeader.MetadataDescriptors[0].DataAreaOffset + la.DataAreaOffset, f.beg);

            char buf[4096];
            size_t n = 4096 > la.DataAreaSize ? la.DataAreaSize : 4096;
            f.read(buf, n);
            buf[4095] = '\0';

            std::printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
            std::printf("%s\nsize:%d\n", buf,n);
        }
        
    }

    std::optional<PhysicalVolumeLabelHeader> Agent::ReadPvLabel(std::ifstream& f) const
    {
        std::optional<PhysicalVolumeLabelHeader> label;

        char buf[SECTOR_SIZE * 4];
        f.read(buf, SECTOR_SIZE * 4);

        for(int i = 0 ; i < 4; ++i)
        {
            auto* l = reinterpret_cast<PhysicalVolumeLabelHeader*>(buf + SECTOR_SIZE * i);
            uint64_t signature = LVM64_TO_CPU(l->Signature);
            if(strncmp((char*)(&signature), PV_SIGNATURE, 8) == 0)
            {
                label = *l;
                f.seekg(SECTOR_SIZE * i + l->DataOffset, f.beg);
                break;
            }
        }

        return label;
    }

    std::vector<DataAreaDescriptor> Agent::ReadDataArea(std::ifstream& f) const
    {
        std::vector<DataAreaDescriptor> descriptors;

        while(true)
        {
            DataAreaDescriptor ad;

            f.read(reinterpret_cast<char*>(&ad), sizeof(ad));
            if(ad.DataAreaSize || ad.DataAreaOffset)
                descriptors.push_back(ad);
            else
                break;
        }

        return descriptors;
    }

    PhysicalVolumeHeader Agent::ReadPvHeader(std::ifstream& f) const
    {
        PhysicalVolumeHeader header;

        f.read(header.ID, 32);
        f.read(reinterpret_cast<char*>(&header.VolumeSize), sizeof(header.VolumeSize));
        
        header.DataDescriptors = ReadDataArea(f);
        header.MetadataDescriptors = ReadDataArea(f);

        f.read(header.Rest, 128);

        return header;
    }

    MtdHeader Agent::ReadMtdHeader(std::ifstream& f) const
    {
        MtdHeader header;
        f.read(reinterpret_cast<char*>(&header), sizeof(header));

        return header;
    }

    std::vector<LocationDescriptor> Agent::ReadLocationsArea(std::ifstream& f) const
    {
        std::vector<LocationDescriptor> locations;
        LocationDescriptor l;

        while(true)
        {
            f.read(reinterpret_cast<char*>(&l), sizeof(l));

            if(l.DataAreaOffset || l.DataAreaSize || l.CheckSum || l.Flags)
                locations.push_back(l);
            else
                break;
        }

        return locations;
    }

    std::optional<PvInformation> Agent::ReadPvInformation(const std::string& devName) const
    {
        PvInformation pvInfo;
        pvInfo.Device = devName;

        std::ifstream f(devName);
        assert(f.is_open());

        std::optional<PhysicalVolumeLabelHeader> label = ReadPvLabel(f);
        assert(label.has_value());
        pvInfo.Label = label.value();

        pvInfo.PvHeader = ReadPvHeader(f);
        const size_t mdaOffset = pvInfo.PvHeader.MetadataDescriptors[0].DataAreaOffset;

        f.seekg(mdaOffset, f.beg);
        pvInfo.m_MtdHeader = ReadMtdHeader(f);

        const char* mtdHeaderMagic = " LVM2 x[5A%r0N*>";
        assert(strncmp(mtdHeaderMagic, pvInfo.m_MtdHeader.Signature, 16) == 0);

        pvInfo.LocationDescriptors = ReadLocationsArea(f);

        return pvInfo;
    }
}