/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "Framework.h"
#include "BinaryModelExporter.h"
#include "../Model.h"
#include "../Mesh.h"
#include "Core/VAO.h"
#include "BinaryModelSpec.h"
#include "Core/Buffer.h"
#include "core/Texture.h"
#include "BinaryImage.hpp"
#include "Data/VertexAttrib.h"

namespace Falcor
{
    static FW::ImageFormat::ID getBinaryFormatID(ResourceFormat format)
    {
        switch(format)
        {
        case ResourceFormat::RGBA8Unorm:
        case ResourceFormat::RGBA8UnormSrgb:
            return FW::ImageFormat::R8_G8_B8_A8;
        case ResourceFormat::Alpha8Unorm:
            return FW::ImageFormat::A8;
        case ResourceFormat::RGBX8Unorm:
            return FW::ImageFormat::XBGR_8888;
        case ResourceFormat::R5G6B5Unorm:
            return FW::ImageFormat::RGB_565;
        case ResourceFormat::RGB5A1Unorm:
            return FW::ImageFormat::RGBA_5551;
        case ResourceFormat::RGB32Float:
            return FW::ImageFormat::RGB_Vec3f;
        case ResourceFormat::RGBA32Float:
            return FW::ImageFormat::RGBA_Vec4f;
        case ResourceFormat::Alpha32Float:
            return FW::ImageFormat::A_F32;
        
        case ResourceFormat::BGRA8Unorm:
        case ResourceFormat::BGRA8UnormSrgb:
            return FW::ImageFormat::BGRA_8888;
        case ResourceFormat::RG8Unorm:
            return FW::ImageFormat::RG_88;
        case ResourceFormat::R8Unorm:
            return FW::ImageFormat::R8;

        case ResourceFormat::BC1Unorm:
        case ResourceFormat::BC1UnormSrgb:
            return FW::ImageFormat::S3TC_DXT1;
        case ResourceFormat::BC2Unorm:
        case ResourceFormat::BC2UnormSrgb:
            return FW::ImageFormat::S3TC_DXT3;
        case ResourceFormat::BC3Unorm:
        case ResourceFormat::BC3UnormSrgb:
            return FW::ImageFormat::S3TC_DXT5;
        case ResourceFormat::BC4Unorm:
            return FW::ImageFormat::RGTC_R;
        case ResourceFormat::BC5Unorm:
            return FW::ImageFormat::RGTC_RG;
        default:
            should_not_get_here();
            return FW::ImageFormat::ID_Max;
        }
    }

    static BasicMaterial::MapType getFalcorMapType(TextureType map)
    {
        switch(map)
        {
        case TextureType_Diffuse:
            return BasicMaterial::MapType::DiffuseMap;
        case TextureType_Alpha:
            return BasicMaterial::MapType::AlphaMap;
        case TextureType_Normal:
            return BasicMaterial::MapType::NormalMap;
        case TextureType_Specular:
            return BasicMaterial::MapType::SpecularMap;
        case TextureType_Glossiness:
            return BasicMaterial::MapType::ShininessMap;
		case TextureType_Displacement:
			return BasicMaterial::MapType::HeightMap;
        default:
            return BasicMaterial::MapType::Count;
        }
    }

    static AttribType getBinaryAttribType(const std::string& name)
    {
        if(name == VERTEX_POSITION_NAME)
            return AttribType_Position;
        else if(name == VERTEX_NORMAL_NAME)
            return AttribType_Normal;
        else if(name == VERTEX_TANGENT_NAME)
            return AttribType_Tangent;
        else if(name == VERTEX_BITANGENT_NAME)
            return AttribType_Bitangent;
        else if(name == VERTEX_DIFFUSE_COLOR_NAME)
            return AttribType_Color;
        else if(name == VERTEX_TEXCOORD_NAME)
            return AttribType_TexCoord;

        should_not_get_here();
        return AttribType_Max;
    }

    static AttribFormat GetBinaryAttribFormat(ResourceFormat format)
    {
        switch(format)
        {
        case ResourceFormat::R8Unorm:
        case ResourceFormat::RG8Unorm:
        case ResourceFormat::RGBA8Unorm:
            return AttribFormat_U8;
        case ResourceFormat::R32Int:
        case ResourceFormat::RG32Int:
        case ResourceFormat::RGB32Int:
        case ResourceFormat::RGBA32Int:
            return AttribFormat_S32;
        case ResourceFormat::R32Float:
        case ResourceFormat::RG32Float:
        case ResourceFormat::RGB32Float:
        case ResourceFormat::RGBA32Float:
            return AttribFormat_F32;
        default:
            should_not_get_here(); // Format not supported by the binary file
            return AttribFormat_Max;
        }
    }

    void writeString(BinaryFileStream& stream, const std::string& str)
    {
        stream << (int32_t)str.size();
        stream.write(str.c_str(), str.size());;
    }

    void BinaryModelExporter::exportToFile(const std::string& filename, const Model* pModel)
    {
        BinaryModelExporter(filename, pModel);
    }

    void BinaryModelExporter::error(const std::string& msg)
    {
        Logger::log(Logger::Level::Error, "Error when exporting model \"" + mFilename + "\".\n" + msg);
        mStream.remove();
    }

    void BinaryModelExporter::warning(const std::string& Msg)
    {
        Logger::log(Logger::Level::Error, "Warning when exporting model \"" + mFilename + "\".\n" + Msg);
    }

    BinaryModelExporter::BinaryModelExporter(const std::string& filename, const Model* pModel) : mFilename(filename)
    {
        mStream.open(filename.c_str(), BinaryFileStream::Mode::Write);
        mpModel = pModel;

        if(mpModel->hasBones())
        {
            error("Binary format doesn't support model with bones");
            return;
        }

        if(mpModel->hasAnimations())
        {
            error("Binary format doesn't support model with animations");
            return;
        }

        if(prepareSubmeshes() == false) return;
        if(writeHeader()      == false) return;
        if(writeTextures()    == false) return;
        if(writeMeshes()      == false) return;
        if(writeInstances()   == false) return;
    }

    bool BinaryModelExporter::prepareSubmeshes()
    {
        // The binary format has a concept of submeshes, that share the same vertex buffer, but have different materials and index buffers.
        // Model works in a similar way (meshes can share VB), but only stores the meshes vector. We need to process that vector to identify submeshes.
        for(uint32_t i = 0; i < mpModel->getMeshCount(); i++)
        {
            auto pMesh = mpModel->getMesh(i);
            if(pMesh->getTopology() != RenderContext::Topology::TriangleList)
            {
                warning("Binary format doesn't support topologies other than triangles.");
                continue;
            }

            auto pVao = pMesh->getVao();
            auto& submesh = mMeshes[pVao.get()];
            submesh.push_back(pMesh);
        }

        // Calculate the number of mesh instances
        for(const auto& m : mMeshes)
        {
            mInstanceCount += m.second[0]->getInstanceCount();
        }

        return true;
    }

    bool BinaryModelExporter::writeHeader()
    {
        mStream.write("BinScene", 8);
        mStream << (int32_t)8 << (int32_t)mpModel->getTextureCount() << (int32_t)mMeshes.size() << (int32_t)mInstanceCount;
        return true;
    }

    bool BinaryModelExporter::writeTextures()
    {
        mTextureHash[nullptr] = -1;

        for(uint32_t i = 0; i < mpModel->getTextureCount(); i++)
        {
            auto pTex = mpModel->getTexture(i).get();
            mTextureHash[pTex] = i;
            if(exportBinaryImage(pTex) == false)
            {
                return false;
            }
        }
        return true;
    }

    bool BinaryModelExporter::writeCommonMeshData(const Mesh::SharedPtr& pMesh, uint32_t submeshCount)
    {
        auto pVao = pMesh->getVao();
		const uint32_t vertexBufferCount = pMesh->getVao()->getVertexBuffersCount();
		mStream << (int32_t)vertexBufferCount << (int32_t)pMesh->getVertexCount() << (int32_t)submeshCount;

        struct vertexBufferInfo 
        {
            Buffer::SharedPtr pBuffer;
            size_t            pData; //this had the p flag because it represents the pointer to the vertex buffers data
            uint32_t          stride;
        };
            
        std::vector<vertexBufferInfo> vbInfo(vertexBufferCount);

		for (uint32_t i = 0; i < vertexBufferCount; i++)
        {
			const VertexLayout* pLayout = pVao->getVertexBufferLayout(i).get();
            assert(pLayout->getElementCount() == 1);
            AttribType type = getBinaryAttribType(pLayout->getElementName(0));
            AttribFormat format = GetBinaryAttribFormat(pLayout->getElementFormat(0));
            uint32_t channels = getFormatChannelCount(pLayout->getElementFormat(0));

            if(type == AttribType_Max)
            {
                error("Unsupported attribute Type");
                return false;
            }

            if(format == AttribFormat_Max)
            {
                error("Unsupported attribute format");
                return false;
            }
            mStream << (int32_t)type << (int32_t)format << (int32_t)channels;

            // Most of the buffers we use were created without any access flags, so can't be mapped.
            // We create a temporary staging buffer to overcome this.
			const Buffer* pVB = pVao->getVertexBuffer(i).get();
			vbInfo[i].pBuffer = Buffer::create(pVB->getSize(), Buffer::BindFlags::None, Buffer::AccessFlags::MapRead, nullptr);
            pVB->copy(vbInfo[i].pBuffer.get());

            vbInfo[i].pData = (size_t)vbInfo[i].pBuffer->map(Buffer::MapType::Read);


            vbInfo[i].stride = pLayout->getTotalStride();
        }

        // Write the vertex buffer
		for (uint32_t i = 0; i < pMesh->getVertexCount(); ++i)
		{
            for (auto& a : vbInfo)
			{ 			
				mStream.write((void*)a.pData, a.stride);
				a.pData += a.stride;
			}
		}

        for (auto& a : vbInfo)
		{
			a.pBuffer->unmap();
		}

        return true;
    }

    bool BinaryModelExporter::writeSubmesh(const Mesh::SharedPtr& pMesh)
    {
        const auto pMaterial = pMesh->getMaterial();

        BasicMaterial basicMaterial;
        basicMaterial.initializeFromMaterial(pMaterial.get());

        glm::vec3 ambient(0,0,0);
        glm::vec4 diffuse = glm::vec4(basicMaterial.diffuseColor, basicMaterial.opacity);
        glm::vec3 specular = basicMaterial.specularColor;
        float glossiness = basicMaterial.shininess;

        mStream << ambient << diffuse << specular << glossiness;

        float displacementCoeff = basicMaterial.bumpScale;
        float displacementBias = basicMaterial.bumpOffset;

        mStream << displacementCoeff << displacementBias;
        
        for(uint32_t i = 0; i < TextureType_Max; i++)
        {
            BasicMaterial::MapType falcorType = getFalcorMapType(TextureType(i));
			int32_t index = -1;
			if(BasicMaterial::MapType::Count != falcorType)
            {
                index = mTextureHash[basicMaterial.pTextures[falcorType].get()];
            }

            mStream << index;
        }

        uint32_t indexCount = pMesh->getIndexCount();
        assert(indexCount % 3 == 0);
        uint32_t primCount = indexCount / 3;

        mStream << (int32_t)primCount;

        // Output the index buffer
        // Most of the buffers we use were created without any access flags, so can't be mapped.
        // We create a temporary staging buffer to overcome this.
        auto pStaging = Buffer::create(pMesh->getVao()->getIndexBuffer()->getSize(), Buffer::BindFlags::None, Buffer::AccessFlags::MapRead, nullptr);
        pMesh->getVao()->getIndexBuffer()->copy(pStaging.get());

        const void* pIndices = pStaging->map(Buffer::MapType::Read);
        mStream.write(pIndices, indexCount * sizeof(uint32_t));

        pStaging->unmap();

        return true;
    }

    bool BinaryModelExporter::writeMeshes()
    {
        uint32_t inst = 0;
        for(const auto& mesh : mMeshes)
        {
            const auto& submeshes = mesh.second;

            for(const Mesh::SharedPtr& pMesh : submeshes)
            {
                if(pMesh == submeshes[0])
                {
                    // All submeshes share the same VB and same layout. We use the first submesh for that.
                    if(writeCommonMeshData(pMesh, (uint32_t)submeshes.size()) == false)
                    {
                        return false;
                    }
                }

                if(writeSubmesh(pMesh) == false)
                {
                    return false;
                }
                inst += pMesh->getInstanceCount();
            }
        }

        return true;
    }

    bool BinaryModelExporter::writeInstances()
    {
        int32_t meshIdx = 0;
        int32_t enabled = 1;
        for(const auto& mesh : mMeshes)
        {
            const Mesh::SharedPtr pMesh = mesh.second[0];

            for(uint32_t i = 0; i < pMesh->getInstanceCount(); i++)
            {
                glm::mat4 transformation = pMesh->getInstanceMatrix(i);
                mStream << meshIdx << enabled << transformation;
                writeString(mStream, "");   // Name
                writeString(mStream, "");   // Meta-data
            }

            meshIdx++;
        }
        return true;
    }

    bool BinaryModelExporter::exportBinaryImage(const Texture* pTexture)
    {
        if(pTexture->getArraySize() > 1)
        {
            error("Binary file format doesn't support texture arrays.");
            return false;
        }

        if(pTexture->getType() != Texture::Type::Texture2D)
        {
            error("Binary file format only supports 2D textures.");
            return false;
        }

        uint32_t width = pTexture->getWidth();
        uint32_t height = pTexture->getHeight();
        ResourceFormat format = pTexture->getFormat();
        uint32_t bpp = getFormatBytesPerBlock(format);

        uint32_t dataSize = pTexture->getMipLevelDataSize(0);
        int32_t formatID = getBinaryFormatID(pTexture->getFormat());

        writeString(mStream, pTexture->getSourceFilename());
        mStream.write("BinImage", 8);
        // Version, width, height, bytes-per-pixel, channel count, FormatID, DataSize
        mStream << (int32_t)2 << (int32_t)width << (int32_t)height << bpp << (int32_t)0 << formatID << (int32_t)dataSize;

        // Write the data
        std::vector<uint8_t> data(dataSize);
        pTexture->readSubresourceData(data.data(), dataSize, 0, 0);
        mStream.write(data.data(), dataSize);
        return true;
    }
}