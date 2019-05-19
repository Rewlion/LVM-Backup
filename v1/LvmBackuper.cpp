#include "LvmBackuper.h"
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
    void Backuper::Restore(const std::string& archive) const
    {
        std::ifstream f(archive, std::ios::binary);
        assert(f.is_open());

        #define READ_BUF_TO(var, size)\
        std::vector<char> var;\
        var.resize(size);\
        f.read(var.data(), size);

        #define READ64_TO(var)\
        uint64_t var = 0;\
        f.read(reinterpret_cast<char*>(&var), sizeof(var))

        #define READ8_TO(var)\
        uint8_t var = 0;\
        f.read(reinterpret_cast<char*>(&var), sizeof(var))

        #define READ_NAME_TO(var)\
        char var[128];\
        f.read(reinterpret_cast<char*>(var), 128);

        #define READ_ID_TO(var)\
        char var[32];\
        f.read(reinterpret_cast<char*>(var), 32);

        #define READ_STRUCT_TO(type, var)\
        type var;\
        f.read(reinterpret_cast<char*>(&var), sizeof(var))

        READ64_TO(archiveMagic);
        assert(archiveMagic == ARCHIVE_MAGIC);

        READ8_TO(nVolumes);
        
        for(uint8_t nVolume = 0; nVolume < nVolumes; ++nVolume)
        {
            READ64_TO(vgMagic);
            assert(vgMagic == ARCHIVE_VG_MAGIC);

            READ_NAME_TO(vgName);

            READ64_TO(metadataSize);
            READ_BUF_TO(metadata, metadataSize);

            READ8_TO(nPVs);
            for(uint8_t nPV = 0; nPV < nPVs; ++nPV)
            {
                READ64_TO(pvMagic);
                assert(pvMagic == ARCHIVE_PV_MAGIC);

                READ_NAME_TO(pvName);
                READ_NAME_TO(pvDevice);

                std::ofstream dev(pvDevice, std::ios::binary);
                assert(dev.is_open());

                char* zeros = new char[1024*1024];
                dev.write(zeros, 1024*1024);
                delete[] zeros;
                dev.seekp(0, dev.beg);

                #define WRITE_FROM(var)\
                dev.write(reinterpret_cast<char*>(&var), sizeof(var));

                #define WRITE_FROM_BUF(var, size)\
                dev.write(reinterpret_cast<char*>(var), size);

                #define WRITE_ID_FROM(var)\
                dev.write(reinterpret_cast<char*>(&var), 32)                

                //LABEL
                READ_STRUCT_TO(PhysicalVolumeLabelHeader, pvLabel);
                const uint64_t labelOffset = pvLabel.SectorNumber * SECTOR_SIZE;
                dev.seekp(labelOffset, dev.beg);
                WRITE_FROM(pvLabel);

                //HEADER
                dev.seekp(labelOffset + pvLabel.DataOffset);
                READ_ID_TO(pvID);
                READ64_TO(volumeSize);
                READ64_TO(nDDAs);

                WRITE_ID_FROM(pvID);
                WRITE_FROM(volumeSize);

                for(uint64_t nDDA = 0; nDDA < nDDAs; ++nDDA)
                {
                    READ_STRUCT_TO(DataAreaDescriptor, DDA);
                    WRITE_FROM(DDA);
                }
                DataAreaDescriptor emptyDescriptor = {};
                WRITE_FROM(emptyDescriptor);

                READ64_TO(nMDAs);
                assert(nMDAs != 0);
                uint64_t mtdHeaderOffset = 0;
                for(uint64_t nMDA = 0; nMDA < nMDAs; ++nMDA)
                {
                    READ_STRUCT_TO(DataAreaDescriptor, MDA);
                    WRITE_FROM(MDA);

                    if(nMDA == 0)
                        mtdHeaderOffset = MDA.DataAreaOffset;
                }
                WRITE_FROM(emptyDescriptor);

                READ_BUF_TO(rest, 128);
                WRITE_FROM_BUF(rest.data(),128);

                //MTD HEADER
                dev.seekp(mtdHeaderOffset, dev.beg);

                READ_STRUCT_TO(MtdHeader, mtdHeader);
                WRITE_FROM(mtdHeader);

                READ64_TO(nLocations);
                assert(nLocations != 0);
                uint64_t metadataOffset = 0;
                for(uint64_t nLocation = 0; nLocation < nLocations; ++nLocation)
                {
                    READ_STRUCT_TO(LocationDescriptor, location);
                    WRITE_FROM(location);

                    if(nLocation == 0)
                        metadataOffset = location.DataAreaOffset;
                }
                LocationDescriptor emptyLocationDescriptor = {};
                WRITE_FROM(emptyLocationDescriptor);

                //SEGMENTS
                READ64_TO(nSegments);
                for(uint64_t nSegment = 0; nSegment < nSegments; ++nSegment)
                {
                    READ_STRUCT_TO(SegmentDescriptor, segment);
                    READ_BUF_TO(extents, segment.DataSize);

                    dev.seekp(segment.StartExtent, dev.beg);
                    WRITE_FROM_BUF(extents.data(), extents.size());
                }


                //METADATA
                dev.seekp(mtdHeaderOffset + metadataOffset, dev.beg);
                WRITE_FROM_BUF(metadata.data(), metadataSize);
            }
        }
    }

    void Backuper::Backup(const Settings& settings, const std::vector<std::string>& devices) const
    {
        VgMtdMap mtdMap = CollectVgMtds(devices);

        std::vector<VgInformation> vgs;

        for(const auto& p : mtdMap)
        {
            VgInformation v;
            v.Name = p.first;
            v.ExtentSize = std::get<int>(p.second.VG.Parameters.at("extent_size"));

            PvInformationMap pvMap = CollectPvsInformation(p.second);
            for(const auto& pv : pvMap)
                v.PvInfos.push_back(pv.second);
            
            vgs.push_back(v);
        }

        std::string archive = settings.BackupsFolder + std::string("LvmBackup");
        std::cout <<archive <<std::endl;
        std::ofstream f("archive", std::ios::binary);
        assert(f.is_open());

        uint64_t buf64;
        uint8_t buf8;

        buf64 = ARCHIVE_MAGIC;
        f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
        buf8 = vgs.size();
        f.write(reinterpret_cast<char*>(&buf8), sizeof(buf8));

        for(const auto& v: vgs)
        {
            buf64 = ARCHIVE_VG_MAGIC;
            f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
            
            char name[128];
            memset(name, 0, 128);
            std::snprintf(name, 128, "%s", v.Name.c_str());
            f.write(name, 128);

            Metadata& mtd = mtdMap[v.Name];
            buf64 = mtd.Raw.size();
            f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
            f.write(mtd.Raw.data(), mtd.Raw.size());

            buf8 = v.PvInfos.size();
            f.write(reinterpret_cast<char*>(&buf8), sizeof(buf8));

            for(const auto& pv : v.PvInfos)
            {
                buf64 = ARCHIVE_PV_MAGIC;
                f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));

                memset(name, 0, 128);
                std::snprintf(name, 128, "%s", pv.PvName.c_str());
                f.write(name, 128);

                memset(name, 0, 128);
                std::snprintf(name, 128, "%s", pv.Device.c_str());
                f.write(name, 128);

                f.write(reinterpret_cast<const char*>(&pv.Label), sizeof(pv.Label));

                f.write(pv.PvHeader.ID, 32);
                f.write(reinterpret_cast<const char*>(&pv.PvHeader.VolumeSize), sizeof(pv.PvHeader.VolumeSize) );

                buf64 = pv.PvHeader.DataDescriptors.size();
                f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
                for(const auto& dda : pv.PvHeader.DataDescriptors)
                    f.write(reinterpret_cast<const char*>(&dda), sizeof(dda));

                buf64 = pv.PvHeader.MetadataDescriptors.size();
                f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
                for(const auto& mda : pv.PvHeader.MetadataDescriptors)
                    f.write(reinterpret_cast<const char*>(&mda), sizeof(mda));

                f.write(pv.PvHeader.Rest, 128);

                f.write(reinterpret_cast<const char*>(&pv.m_MtdHeader), sizeof(pv.m_MtdHeader));
                
                buf64 = pv.LocationDescriptors.size();
                f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));
                for(const auto& ld : pv.LocationDescriptors)
                    f.write(reinterpret_cast<const char*>(&ld), sizeof(ld));

                //segments
                std::ifstream dev(pv.Device, std::ios::binary);
                assert(dev.is_open());

                buf64 = pv.Segments.size();
                f.write(reinterpret_cast<char*>(&buf64), sizeof(buf64));

                for(const SegmentDescription& sd : pv.Segments)
                {
                    SegmentDescriptor segment;
                    segment.StartExtent = sd.StartExtent * v.ExtentSize * SECTOR_SIZE;
                    segment.DataSize = sd.ExtentCount * v.ExtentSize * SECTOR_SIZE;

                    f.write(reinterpret_cast<char*>(&segment), sizeof(segment));
                    
                    std::vector<char> buf;                    
                    buf.resize(segment.DataSize);

                    dev.seekg(segment.StartExtent, dev.beg);
                    dev.read(buf.data(), segment.DataSize);

                    f.write(buf.data(), segment.DataSize);
                }
            }
        }
    }

    void Backuper::ConstructArchive(const VgInformation& v, const std::string& archive) const
    {


    }

    VgMtdMap Backuper::CollectVgMtds(const std::vector<std::string>& devices) const
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

    std::optional<Metadata> Backuper::ReadVgMtd(const std::string& dev) const
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

    std::optional<std::vector<char>> Backuper::ReadRawMtd(std::ifstream& f) const
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

    void Backuper::DumpMtd(const std::string& dev) const
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

    std::optional<PhysicalVolumeLabelHeader> Backuper::ReadPvLabel(std::ifstream& f) const
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

    std::vector<DataAreaDescriptor> Backuper::ReadDataArea(std::ifstream& f) const
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

    PhysicalVolumeHeader Backuper::ReadPvHeader(std::ifstream& f) const
    {
        PhysicalVolumeHeader header;

        f.read(header.ID, 32);
        f.read(reinterpret_cast<char*>(&header.VolumeSize), sizeof(header.VolumeSize));
        
        header.DataDescriptors = ReadDataArea(f);
        header.MetadataDescriptors = ReadDataArea(f);

        f.read(header.Rest, 128);

        return header;
    }

    MtdHeader Backuper::ReadMtdHeader(std::ifstream& f) const
    {
        MtdHeader header;
        f.read(reinterpret_cast<char*>(&header), sizeof(header));

        return header;
    }

    std::vector<LocationDescriptor> Backuper::ReadLocationsArea(std::ifstream& f) const
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

    std::optional<PvInformation> Backuper::ReadPvInformation(const std::string& devName) const
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

    PvInformationMap Backuper::CollectPvsInformation(const Lvm::Metadata& mtd) const
    {
        PVSegmentsMap sMap = CollectSegmentsDescription(mtd);
        PvInformationMap pvMap;

        for(const PhysicalVolume& pv : mtd.VG.PhysicalVolumes)
        {
            const std::string& device = std::get<std::string>(pv.Parameters.at("device"));
            std::optional<PvInformation> pvi = ReadPvInformation(device);
            if(pvi.has_value())
            {
                pvi.value().Device = device;
                auto it = sMap.find(pv.Name);
                if(it != sMap.end())
                    pvi.value().Segments = sMap[pv.Name].Segments;
                pvMap[pv.Name] = pvi.value();
            }
        }

        return pvMap;
    }

	PVSegmentsMap Backuper::CollectSegmentsDescription(const Lvm::Metadata& mtd) const
	{
        PVSegmentsMap sMap;

		for(const LogicalVolume& lv : mtd.VG.LogicalVolumes)
			for(const SubSection& segment : lv.Segments)
			{
				const Pair& stripes = std::get<Pair>(segment.Parameters.at("stripes"));
                SegmentDescription d;
                d.StartExtent = stripes.Number;
                d.ExtentCount = std::get<int>(segment.Parameters.at("extent_count"));

                auto it = sMap.find(stripes.Name);
                if(it != sMap.end())
                {
                    it->second.Segments.push_back(d);
                }
                else
                {
                    PVSegmentsDescription sd;
                    sd.Segments.push_back(d);
                    sMap[stripes.Name] = sd;
                }
			}

        return sMap;
	}
}