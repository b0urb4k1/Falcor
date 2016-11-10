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
#include "API/Sampler.h"
#include "API/D3D/D3DState.h"
#include "API/Device.h"

namespace Falcor
{
    Sampler::~Sampler() = default;

    uint32_t Sampler::getApiMaxAnisotropy()
    {
        return D3D12_MAX_MAXANISOTROPY;
    }

    Sampler::SharedPtr Sampler::create(const Desc& desc)
    {
        SharedPtr pSampler = SharedPtr(new Sampler(desc));
        D3D12_SAMPLER_DESC d3dDesc;
        initD3DSamplerDesc(pSampler.get(), d3dDesc);
        DescriptorHeap* pHeap = gpDevice->getSamplerDescriptorHeap().get();
        uint32_t index = pHeap->allocateHandle();

        gpDevice->getApiHandle()->CreateSampler(&d3dDesc, pHeap->getCpuHandle(index));
        pSampler->mApiHandle = pHeap->getGpuHandle(index);

        return pSampler;
    }
}
