#include "Agent.h"

#include <iostream>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <assert.h>
#include <algorithm>
#include <tuple>
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

    std::tuple<Lvm::Metadata, Lvm::LogicalVolume> FindMtdWithLV(const Lvm::Backup::VgMtdMap& mtdMap, const std::string& lvName)
    {
        for(auto&[vgName, mtd] : mtdMap)
        {
            const auto& lvs = mtd.VG.LogicalVolumes;
            const auto& it = std::find_if(lvs.begin(), 
                                          lvs.end(), 
                                          [&](auto& lv) { return lvName == lv.Name; }
                                        );
            if( it != lvs.end())
                return {mtd, *it};
        }

        throw "No LV in MTD.";
    }

    struct PhysicalVolumeInfo
    {
        std::string Device;
        uint64_t    PeStart;
    };

    typedef std::unordered_map<std::string, PhysicalVolumeInfo> PvInfoMap;

    PvInfoMap CollectPhysicalVolumesInfos(const Lvm::Metadata& mtd)
    {
        PvInfoMap infos;
        for(const auto& pv : mtd.VG.PhysicalVolumes)
        {
            PhysicalVolumeInfo info;
            info.Device = std::get<std::string>(pv.Parameters.at("device"));
            info.PeStart = std::stoi(std::get<std::string>(pv.Parameters.at("pe_start")));
            
            infos[pv.Name] = info;
            std::printf("%s : %s(pe_start:%lu)\n", pv.Name.c_str(), info.Device.c_str(), info.PeStart);
            
        }

        return infos;
    }

    std::vector<Lvm::Backup::LinearSegmentDescription> CollectSegmentDescriptions(const Lvm::LogicalVolume& lv, const PvInfoMap& pvInfos)
    {
        std::vector<Lvm::Backup::LinearSegmentDescription> segments;
        for(auto& segment : lv.Segments)
        {
            Lvm::Backup::LinearSegmentDescription sd;
            sd.StartExtent = std::stoi(std::get<std::string>(segment.Parameters.at("start_extent")));
            sd.ExtentCount = std::stoi(std::get<std::string>(segment.Parameters.at("extent_count")));

            Lvm::Array array = std::get<Lvm::Array>(segment.Parameters.at("stripes"));
            const std::string device = pvInfos.at(array[1]).Device;
            std::snprintf(sd.Device, 255, "%s", device.c_str());
            sd.Offset = std::stoi(array[0].c_str());
            sd.PeStart = pvInfos.at(array[1]).PeStart;

            std::printf("%s:\n  start_extent:%lu\n  extent_count:%lu\n  device:%s\n  offset:%lu\n", 
                segment.Name.c_str(), sd.StartExtent, sd.ExtentCount, sd.Device, sd.Offset );

            segments.push_back(sd);
        }

        return segments;
    }

    //TODO FIXME: Array order is reversed because of the grammar in the parser.
    void GrammarFix(Lvm::Metadata& mtd, Lvm::LogicalVolume& lv)
    {
        std::reverse(mtd.VG.PhysicalVolumes.begin(), mtd.VG.PhysicalVolumes.end());
        std::reverse(lv.Segments.begin(), lv.Segments.end());
    }


}

namespace Lvm::Backup
{
    void Agent::Restore(const std::string& archivePath) const
    {
        std::printf("[Restore demo, it is assumed that segments are linear only, LV is not big enough to be in memory]\n\n");   
        
        int answer = 0;
        do
        {
            std::printf("Restoring will erase some data on the physical volumes. \nContinue? (y,n):");
            answer = std::getchar();
        }while(answer != 'y' && answer != 'n');

        if(answer == 'n')
            exit(0);

        std::printf("Opening archive...\n");
        std::ifstream archive(archivePath, std::ios::binary);
        assert(archive.is_open());
        
        std::printf("Checking the Magic.\n");
        uint64_t buf64;
        archive.read(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        assert(buf64 == ARCHIVE_MAGIC);
        std::printf("Magic is correct.\n");

        std::printf("Restoring metadatas of physical volumes...\n");
        archive.read(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        std::printf("Number of Physical volumes that will be affected:%lu\n", buf64);

        const uint64_t pv_count = buf64;
        for(int i = 0; i < pv_count; ++i )
        {
            RawMtdInfo info;
            archive.read(reinterpret_cast<char*>(&info), sizeof(info));
            std::vector<char> rawMtd;
            rawMtd.resize(info.DataSize);
            archive.read(rawMtd.data(), info.DataSize);

            std::ofstream dev(info.Name, std::ios::binary);
            assert(dev.is_open());
            dev.write(rawMtd.data(), rawMtd.size());

            std::printf("%s's metadata is restored.\n", info.Name);
        }
        std::printf("Done.\n");

        std::printf("Restoring segments...\n");
        archive.read(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        const uint64_t segmentsCount = buf64;
        std::vector<LinearSegmentDescription> segments;
        segments.resize(segmentsCount);
        archive.read(reinterpret_cast<char*>(segments.data()), sizeof(LinearSegmentDescription) * segmentsCount);
        
        archive.read(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        const uint64_t extent_size = buf64;
        std::printf("extent_size:%lu\n", extent_size);

        for(int i = 0; i < segments.size(); ++i)
        {
            std::printf("Segment info:\n");

            const auto& segment = segments[i];
            std::printf(" segment%d\n  extent_start:%lu extent_count:%lu device:%s offset:%lu pe_start:%lu\n", 
                i+1, segment.StartExtent, segment.ExtentCount, segment.Device, segment.Offset, segment.PeStart);

            std::ofstream dev(segment.Device, std::ios::binary);
            assert(dev.is_open());

            const uint64_t offset = segment.Offset * extent_size * SECTOR_SIZE + segment.PeStart * SECTOR_SIZE;
            dev.seekp(offset, std::ios::beg);

            std::vector<char> buf;
            buf.resize(segment.ExtentCount * extent_size * SECTOR_SIZE);
            archive.read(buf.data(), buf.size());
            dev.write(buf.data(), buf.size());

            std::printf(" segment is restored.\n");
        }
    }

    void Agent::Backup(const std::string& lvName, const std::vector<std::string>& devices) const
    {
        std::printf("[Backup demo, it is assumed that segments are linear only, LV is not big enough to be in memory]\n\n");

        std::printf("Collecting VG metadatas...\n");
        const VgMtdMap mtdMap = CollectVgMtds(devices);
        std::printf("Done.\n");

        std::printf("Finding a metadata with requested LV (%s)...\n", lvName.c_str());
        auto&&[mtd, lv] = FindMtdWithLV(mtdMap, lvName);
        GrammarFix(mtd, lv);//tmp huyak i v prodakshen
        std::printf("Found. This LV belongs to VG(%s)\n", mtd.VG.Name.c_str());

        const uint64_t extent_size = std::stoi(std::get<std::string>(mtd.VG.Parameters.at("extent_size")));
        std::printf("extent_size:%lu\n", extent_size);

        std::printf("Collecting physical volumes information in VG...\n");
        const PvInfoMap pvInfoMap = CollectPhysicalVolumesInfos(mtd);
        std::printf("Done.\n");

        std::printf("Collecting all segments for the LV from VG metadata.\n");
        std::vector<LinearSegmentDescription> segments = CollectSegmentDescriptions(lv, pvInfoMap);
        std::printf("Done.\n");

        std::printf("Collecting metadata of Physical Volumes that are used in LV...\n");
        std::vector<RawMtd> rawMtds = CollectPhysicalVolumesRawMtd(segments);
        std::printf("Done.\n");

        std::printf("Reading whole LV into memory.\n");
        char lvPath[256];
        std::snprintf(lvPath, 255, "/dev/%s/%s", mtd.VG.Name.c_str(), lvName.c_str());
        std::printf("lv path: %s\n", lvPath);
        std::ifstream f(lvPath, std::ios::ate | std::ios::binary);
        assert(f.is_open());
        const size_t size = f.tellg();
        std::printf("size:%lu (%f Gb)\n", size, static_cast<float>(size) / (1024.0f*1024.0f * 1024.0f));
        std::vector<char> buf;
        buf.resize(size);
        f.seekg(std::ios::beg);
        f.read(buf.data(), size);
        std::printf("Done.\n");

        const char* archiveName = "./archive";
        std::printf("Creating archive...\nPath:%s\n", archiveName);
        std::ofstream archive(archiveName, std::ios::binary);

        uint64_t buf64;
        //MAGIC
        std::printf("Wrote a magic.\n");
        buf64 = ARCHIVE_MAGIC;
        archive.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        //Physical Volumes raw metadata
        std::printf("Writing a pv metadatas...\n");
        buf64 = rawMtds.size();
        archive.write(reinterpret_cast<char*>(&buf64), sizeof(buf64)); 
        for(const auto& rawMtd : rawMtds)
        {
            std::printf(" Device:%s\n DataSize:%lu\n", rawMtd.Info.Name, rawMtd.Info.DataSize);
            archive.write(reinterpret_cast<const char*>(&rawMtd.Info), sizeof(rawMtd.Info));
            archive.write(rawMtd.Data.data(), rawMtd.Data.size());
        }
        std::printf("Done.\n");
        //Segments
        buf64 = segments.size();
        archive.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        for(const auto& segment : segments)
            archive.write(reinterpret_cast<const char*>(&segment), sizeof(segment));
        //LV itself
        buf64 = extent_size;
        archive.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        archive.write(buf.data(), buf.size());

        std::printf("Archive is created now.\n");
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

    RawMtd Agent::CollectRawMtd(const char* device) const
    {
        std::ifstream f(device);
        assert(f.is_open());

        std::optional<PhysicalVolumeLabelHeader> label = ReadPvLabel(f);
        assert(label.has_value());
        
        RawMtd rawMtd;
        std::memset(rawMtd.Info.Name, 0, 256);
        std::snprintf(rawMtd.Info.Name, 255, "%s", device);

        PhysicalVolumeHeader pvHeader = ReadPvHeader(f);
        const size_t mdaOffset = pvHeader.MetadataDescriptors[0].DataAreaOffset;

        f.seekg(mdaOffset, f.beg);
        MtdHeader mtdHeader = ReadMtdHeader(f);

        const char* mtdHeaderMagic = " LVM2 x[5A%r0N*>";
        assert(strncmp(mtdHeaderMagic, mtdHeader.Signature, 16) == 0);
            
        std::vector<LocationDescriptor> locationsArea = ReadLocationsArea(f);
        
        auto& la = locationsArea[0];
        rawMtd.Info.DataSize = mdaOffset + la.DataAreaOffset + la.DataAreaSize + 1;
        rawMtd.Data.resize(rawMtd.Info.DataSize);
        f.seekg(0, std::ios::beg);
        f.read(rawMtd.Data.data(), rawMtd.Info.DataSize);

        return rawMtd;
    }

    std::vector<RawMtd> Agent::CollectPhysicalVolumesRawMtd(const std::vector<LinearSegmentDescription>& segments) const
    {
        std::vector<RawMtd> rawMtds;
        for(const auto& segment : segments)
        {
            RawMtd rawMtd = CollectRawMtd(segment.Device);
            rawMtds.push_back(rawMtd);
        }

        return rawMtds;
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

        std::printf("LENGTH: %lu\n", locationsArea.size());
        for(auto& la : locationsArea)
        {
            f.seekg(pvHeader.MetadataDescriptors[0].DataAreaOffset + la.DataAreaOffset, f.beg);

            char buf[4096];
            size_t n = 4096 > la.DataAreaSize ? la.DataAreaSize : 4096;
            f.read(buf, n);
            buf[4095] = '\0';

            std::printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
            std::printf("%s\nsize:%lu\n", buf,n);
        }
        
    }

    void Agent::DumpMTDs(const std::vector<std::string>& devices) const
    {
        for(auto& dev : devices)
            std::printf("dev:%s\n", dev.c_str());

        VgMtdMap map = CollectVgMtds(devices);
        for(auto&[name, mtd] : map)
        {
            std::printf("name:%s\n", name.c_str());
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