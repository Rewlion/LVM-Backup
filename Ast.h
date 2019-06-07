#pragma once

#include "Utility.h"
#include <string>

namespace Lvm::Ast
{
	class AstVisitor;

	class Node
	{
	public:
		virtual void Accept(AstVisitor* v);
		virtual ~Node() {}
	};

	class NameNode : public Node
	{
	public:
		NameNode(const char* name);
		virtual void Accept(AstVisitor* v);
		virtual ~NameNode() {}

	public:
		std::string Name;
	};

    class ValueNode : public Node
    {
    public:
        ValueNode(const ::Lvm::Value& value);
        virtual void Accept(AstVisitor* v);
        virtual ~ValueNode(){}

    public:
        ::Lvm::Value Value;
    };

	class ParameterNode : public Node
	{
	public:
        ParameterNode(NameNode* name, ValueNode* value, ParameterNode* next = nullptr);
		virtual void Accept(AstVisitor* v);
		virtual ~ParameterNode();

	public:
        NameNode* Name;
        ValueNode* Value;

		ParameterNode* Next;
	};

    class SubSectionNode : public Node
    {
    public:
        SubSectionNode(NameNode* name, ParameterNode* parameters, SubSectionNode* next = nullptr);
        virtual void Accept(AstVisitor* v);
        virtual ~SubSectionNode();

    public:
        NameNode* Name;
        ParameterNode* Parameters;

        SubSectionNode* Next;
    };

    class PhysicalVolumeSectionNode : public Node
    {
    public:
        PhysicalVolumeSectionNode(SubSectionNode* subsections);
        virtual void Accept(AstVisitor* v);
        virtual ~PhysicalVolumeSectionNode();

    public:
        SubSectionNode* SubSections;
    };

    class LogicalVolumeNode : public Node
    {
    public:
        LogicalVolumeNode(NameNode* name, ParameterNode* parameters, SubSectionNode* subsections, LogicalVolumeNode* next = nullptr);
        virtual void Accept(AstVisitor* v);
        virtual ~LogicalVolumeNode();

    public:
        NameNode* Name;
        ParameterNode* Parameters;
        SubSectionNode* SubSections;

        LogicalVolumeNode* Next;
    };

    class LogicalVolumeSectionNode : public Node
    {
    public:
        LogicalVolumeSectionNode(LogicalVolumeNode* logicalVolumes);
        virtual void Accept(AstVisitor* v);
        virtual ~LogicalVolumeSectionNode();

    public:
        LogicalVolumeNode* LogicalVolumes;
    };

    class VolumeGroupSectionsNode: public Node
    {
    public:
        VolumeGroupSectionsNode(PhysicalVolumeSectionNode* physicalvs, LogicalVolumeSectionNode* logicalvs = nullptr);
        virtual void Accept(AstVisitor* v);
        virtual ~VolumeGroupSectionsNode();

    public:
        PhysicalVolumeSectionNode* PhysicalVolumeSection;
        LogicalVolumeSectionNode* LogicalVolumeSection;
    };

	class VolumeGroupNode : public Node
	{
	public:
        VolumeGroupNode(NameNode* name, ParameterNode* parameters, VolumeGroupSectionsNode* vgs);
		virtual void Accept(AstVisitor* v);
		virtual ~VolumeGroupNode();

    public:
        NameNode* Name;
        ParameterNode* Parameters;
        VolumeGroupSectionsNode* VolumeGroupSections;
	};

	class MetadataNode : public Node
	{
	public:
        virtual void Accept(AstVisitor* v);
		virtual ~MetadataNode();

    public:
        ParameterNode* Parameters = nullptr;
        VolumeGroupNode* VolumeGroup = nullptr;
	};

    class AstVisitor
    {
    public:
        virtual void Visit(Node* n) {}
        virtual void Visit(MetadataNode* n) {}
        virtual void Visit(VolumeGroupNode* n) {}
        virtual void Visit(VolumeGroupSectionsNode* n) {}
        virtual void Visit(LogicalVolumeSectionNode* n) {}
        virtual void Visit(LogicalVolumeNode* n) {}
        virtual void Visit(PhysicalVolumeSectionNode* n) {}
        virtual void Visit(SubSectionNode* n) {}
        virtual void Visit(ParameterNode* n) {}
        virtual void Visit(ValueNode* n) {}
        virtual void Visit(NameNode* n) {}
    };

    class AstToMetaConverter : public AstVisitor
    {
    public:
        virtual void Visit(MetadataNode* n);
        virtual void Visit(VolumeGroupNode* n);

        inline void SetMetadata(::Lvm::Metadata* mtd)
        {
            Metadata = mtd;
        }

    private:
        ::Lvm::ParameterMap               ConvertParameterNode(const ParameterNode* n) const;
        std::vector<::Lvm::SubSection>    ConvertSubSectionNode(const SubSectionNode* n) const;
        std::vector<::Lvm::LogicalVolume> ConvertLogicalVolumeNode(const LogicalVolumeNode* n) const;

    private:
        ::Lvm::Metadata* Metadata = nullptr;
    };
}