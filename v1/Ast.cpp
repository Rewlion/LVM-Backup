#include "Ast.h"

namespace Lvm::Ast
{
	void Node::Accept(AstVisitor* v)
	{
		v->Visit(this);
	}

	NameNode::NameNode(const char* str)
		: Name(str)
	{
	}

	void NameNode::Accept(AstVisitor* v)
	{
		v->Visit(this);
	}

	ValueNode::ValueNode(const ::Lvm::Value& value)
	   : Value(value)
	{
	}

    void ValueNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }    

    ParameterNode::ParameterNode(NameNode* name, ValueNode* value, ParameterNode* next)
        : Name(name)
        , Value(value)
        , Next(next)
    {
    }

    ParameterNode::~ParameterNode()
    {
        delete Name;
        delete Value;
        delete Next;
    }

    void ParameterNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    SubSectionNode::SubSectionNode(NameNode* name, ParameterNode* parameters, SubSectionNode* next)
        : Name(name)
        , Parameters(parameters)
        , Next(next)
    {
    }

    SubSectionNode::~SubSectionNode()
    {
        delete Name;
        delete Parameters;
        delete Next;
    }

    void SubSectionNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    PhysicalVolumeSectionNode::PhysicalVolumeSectionNode(SubSectionNode* subsections)
        : SubSections(subsections)
    {
    }

    PhysicalVolumeSectionNode::~PhysicalVolumeSectionNode()
    {
        delete SubSections;
    }

    void PhysicalVolumeSectionNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    LogicalVolumeNode::LogicalVolumeNode(NameNode* name, ParameterNode* parameters, SubSectionNode* subsections, LogicalVolumeNode* next)
        : Name(name)
        , Parameters(parameters)
        , SubSections(subsections)
        , Next(next)
    {
    }

    LogicalVolumeNode::~LogicalVolumeNode()
    {
        delete Name;
        delete Parameters;
        delete SubSections;
    }

    void LogicalVolumeNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    LogicalVolumeSectionNode::LogicalVolumeSectionNode(LogicalVolumeNode* logicalVolumes)
        : LogicalVolumes(logicalVolumes)
    {
    }

    LogicalVolumeSectionNode::~LogicalVolumeSectionNode()
    {
        delete LogicalVolumes;
    }

    void LogicalVolumeSectionNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    VolumeGroupSectionsNode::VolumeGroupSectionsNode(PhysicalVolumeSectionNode* physicalvs, LogicalVolumeSectionNode* logicalvs)
        : PhysicalVolumeSection(physicalvs)
        , LogicalVolumeSection(logicalvs)
    {
    }

    VolumeGroupSectionsNode::~VolumeGroupSectionsNode()
    {
        delete PhysicalVolumeSection;
        delete LogicalVolumeSection;
    }

    void VolumeGroupSectionsNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    VolumeGroupNode::VolumeGroupNode(NameNode* name, ParameterNode* parameters, VolumeGroupSectionsNode* vgs)
        : Name(name)
        , Parameters(parameters)
        , VolumeGroupSections(vgs)
    {
    }

    VolumeGroupNode::~VolumeGroupNode()
    {
        delete Name;
        delete Parameters;
        delete VolumeGroupSections;
    }

    void VolumeGroupNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    MetadataNode::~MetadataNode()
    {
        delete Parameters;
        delete VolumeGroup;
    }

    void MetadataNode::Accept(AstVisitor* v)
    {
        v->Visit(this);
    }

    void AstToMetaConverter::Visit(MetadataNode* n)
    {
        if(Metadata != nullptr)
        {
            Metadata->Parameters = ConvertParameterNode(n->Parameters);
            n->VolumeGroup->Accept(this);
        }
    }

    void AstToMetaConverter::Visit(VolumeGroupNode* n)
    {
        Metadata->VG.Name = n->Name->Name;
        Metadata->VG.Parameters = ConvertParameterNode(n->Parameters);

        Metadata->VG.PhysicalVolumes = ConvertSubSectionNode(n->VolumeGroupSections->PhysicalVolumeSection->SubSections);
           
        if(n->VolumeGroupSections->LogicalVolumeSection)
            Metadata->VG.LogicalVolumes = ConvertLogicalVolumeNode(n->VolumeGroupSections->LogicalVolumeSection->LogicalVolumes);
    }

    ::Lvm::ParameterMap AstToMetaConverter::ConvertParameterNode(const ParameterNode* n) const
    {
        ParameterMap map;
        for(auto node = n; node != nullptr; node = node->Next)
            map[node->Name->Name] = node->Value->Value;

        return map;
    }

    std::vector<::Lvm::SubSection> AstToMetaConverter::ConvertSubSectionNode(const SubSectionNode* n) const
    {
        std::vector<::Lvm::SubSection> subsections;

        for(auto node = n; node != nullptr; node = node->Next)
        {
            SubSection s;
            s.Name       = node->Name->Name;
            s.Parameters = ConvertParameterNode(node->Parameters);

            subsections.push_back(s);
        }

        return subsections;
    }

    std::vector<::Lvm::LogicalVolume> AstToMetaConverter::ConvertLogicalVolumeNode(const LogicalVolumeNode* n) const
    {
        std::vector<::Lvm::LogicalVolume> volumes;
        
        for(auto node = n; node != nullptr; node = node->Next)
        {
            ::Lvm::LogicalVolume lv;
            lv.Name       = node->Name->Name;
            lv.Parameters = ConvertParameterNode(node->Parameters);
            lv.Segments   = ConvertSubSectionNode(node->SubSections);

            volumes.push_back(lv);
        }

        return volumes;
    }
}