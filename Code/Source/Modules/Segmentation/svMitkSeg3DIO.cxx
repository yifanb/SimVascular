#include "svMitkSeg3DIO.h"
#include "svMitkSeg3D.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>

#include <tinyxml.h>

#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkErrorCode.h>

static mitk::CustomMimeType CreatesvSeg3DMimeType()
{
    mitk::CustomMimeType mimeType(mitk::IOMimeTypes::DEFAULT_BASE_NAME() + ".svseg3d");
    mimeType.SetCategory("SimVascular Files");
    mimeType.AddExtension("s3d");
    mimeType.SetComment("SimVascular 3D Segmentation");

    return mimeType;
}

svMitkSeg3DIO::svMitkSeg3DIO()
    : mitk::AbstractFileIO(svMitkSeg3D::GetStaticNameOfClass(), CreatesvSeg3DMimeType(), "SimVascular 3D Segmentation")
{
    this->RegisterService();
}

std::vector<mitk::BaseData::Pointer> svMitkSeg3DIO::Read()
{
    std::string fileName=GetInputLocation();

    return ReadFile(fileName);
}

std::vector<mitk::BaseData::Pointer> svMitkSeg3DIO::ReadFile(std::string fileName)
{
    std::vector<mitk::BaseData::Pointer> result;

    TiXmlDocument document;

    if (!document.LoadFile(fileName))
    {
        mitkThrow() << "Could not open/read/parse " << fileName;
        //        return result;
    }

    //    TiXmlElement* version = document.FirstChildElement("format");

    TiXmlElement* segElement = document.FirstChildElement("seg3d");

    if(!segElement){
        mitkThrow() << "No 3D seg data in "<< fileName;
        //        return result;
    }

    svMitkSeg3D::Pointer mitkSeg3D = svMitkSeg3D::New();
    svSeg3D* seg3D=new svSeg3D();
    svSeg3DParam param;

    //read parameters
    TiXmlElement* paramElement = segElement->FirstChildElement("param");
    if(paramElement)
    {
        paramElement->QueryStringAttribute("method",&param.method);
        if(param.method!="")
        {
            paramElement->QueryDoubleAttribute("lower_threshold",&param.lowerThreshold);
            paramElement->QueryDoubleAttribute("upper_threshold",&param.upperThreshold);

            //read seeds
            TiXmlElement* seedsElement = paramElement->FirstChildElement("seeds");
            if(seedsElement)
            {
                for( TiXmlElement* seedElement = seedsElement->FirstChildElement("seed");
                     seedElement != nullptr;
                     seedElement = seedElement->NextSiblingElement("seed") )
                {
                    if (seedsElement == nullptr)
                        continue;

                    svSeed seed;

                    seedElement->QueryIntAttribute("id", &seed.id);
                    seedElement->QueryStringAttribute("type", &seed.type);
                    seedElement->QueryDoubleAttribute("x", &seed.x);
                    seedElement->QueryDoubleAttribute("y", &seed.y);
                    seedElement->QueryDoubleAttribute("z", &seed.z);
                    seedElement->QueryDoubleAttribute("radius", &seed.radius);

                    param.AddSeed(seed);
                }
            }
        }
    }

    seg3D->SetParam(param);
    std::string dataFileName=fileName.substr(0,fileName.find_last_of("."))+".vtp";
    std::ifstream dataFile(dataFileName);
    if (dataFile) {
        vtkSmartPointer<vtkXMLPolyDataReader> reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();

        reader->SetFileName(dataFileName.c_str());
        reader->Update();
        vtkSmartPointer<vtkPolyData> vpd=reader->GetOutput();
        if(vpd)
            seg3D->SetVtkPolyData(vpd);
    }

    mitkSeg3D->SetSeg3D(seg3D);

    result.push_back(mitkSeg3D.GetPointer());
    return result;
}

mitk::IFileIO::ConfidenceLevel svMitkSeg3DIO::GetReaderConfidenceLevel() const
{
    if (mitk::AbstractFileIO::GetReaderConfidenceLevel() == mitk::IFileIO::Unsupported)
    {
        return mitk::IFileIO::Unsupported;
    }
    return Supported;
}

void svMitkSeg3DIO::Write()
{
    ValidateOutputLocation();

    std::string fileName=GetOutputLocation();

    const svMitkSeg3D* mitkSeg3D = dynamic_cast<const svMitkSeg3D*>(this->GetInput());
    if(!mitkSeg3D) return;

    TiXmlDocument document;
    auto  decl = new TiXmlDeclaration( "1.0", "UTF-8", "" );
    document.LinkEndChild( decl );

    auto  version = new TiXmlElement("format");
    version->SetAttribute("version",  "1.0" );
    document.LinkEndChild(version);

    svSeg3D* seg3D=mitkSeg3D->GetSeg3D();
    if(seg3D)
    {
        svSeg3DParam& param=seg3D->GetParam();

        auto  segElement = new TiXmlElement("seg3d");
        document.LinkEndChild(segElement);

        auto  paramElement = new TiXmlElement("param");
        segElement->LinkEndChild(paramElement);

        if(param.method!="")
        {
            paramElement->SetAttribute("method", param.method);

            paramElement->SetDoubleAttribute("lower_threshold", param.lowerThreshold);
            paramElement->SetDoubleAttribute("upper_threshold", param.upperThreshold);


            auto  seedsElement = new TiXmlElement("seeds");
            paramElement->LinkEndChild(seedsElement);

            std::map<int, svSeed>& seedMap=param.GetSeedMap();
            for(auto s:seedMap)
            {
                auto  seedElement = new TiXmlElement("seed");
                svSeed seed=s.second;
                seedsElement->LinkEndChild(seedElement);
                seedElement->SetAttribute("id",seed.id);
                seedElement->SetAttribute("type", seed.type);
                seedElement->SetDoubleAttribute("x", seed.x);
                seedElement->SetDoubleAttribute("y", seed.y);
                seedElement->SetDoubleAttribute("z", seed.z);
                seedElement->SetDoubleAttribute("radius", seed.radius);
            }
        }

        std::string dataFileName=fileName.substr(0,fileName.find_last_of("."))+".vtp";

        vtkPolyData* vpd=seg3D->GetVtkPolyData();
        if(vpd)
        {
            vtkSmartPointer<vtkXMLPolyDataWriter> writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
            writer->SetFileName(dataFileName.c_str());
            writer->SetInputData(vpd);
            if (writer->Write() == 0 || writer->GetErrorCode() != 0 )
            {
                std::cerr << "vtkXMLPolyDataWriter error: " << vtkErrorCode::GetStringFromErrorCode(writer->GetErrorCode())<<std::endl;
            }
        }
    }

    if (document.SaveFile(fileName) == false)
    {
        mitkThrow() << "Could not write model to " << fileName;

    }
}

mitk::IFileIO::ConfidenceLevel svMitkSeg3DIO::GetWriterConfidenceLevel() const
{
    if (mitk::AbstractFileIO::GetWriterConfidenceLevel() == mitk::IFileIO::Unsupported) return mitk::IFileIO::Unsupported;
    const svMitkSeg3D* input = dynamic_cast<const svMitkSeg3D*>(this->GetInput());
    if (input)
    {
        return Supported;
    }else{
        return Unsupported;
    }
}

svMitkSeg3DIO* svMitkSeg3DIO::IOClone() const
{
    return new svMitkSeg3DIO(*this);
}

