// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "modelbin.h"

#include <stdio.h>
#include <string.h>
#include <vector>
#include "config.h"

namespace mercury {

#if USE_STDIO
static const unsigned char* _null_mem = 0;

ModelBin::ModelBin(const Tensor<float>* _weights) : weights(_weights), binfp(0), mem(_null_mem)
{
}

ModelBin::ModelBin(FILE* _binfp) : weights(0), binfp(_binfp), mem(_null_mem)
{
}

ModelBin::ModelBin(const unsigned char*& _mem) : weights(0), binfp(0), mem(_mem)
{
}
#else
ModelBin::ModelBin(const unsigned char*& _mem) : weights(0), mem(_mem)
{
}
#endif // USE_STDIO

Tensor<float> ModelBin::load(int w, int type) const
{
    if (weights)
    {
		Tensor<float> m = weights[0];
        weights++;
        return m;
    }

#if USE_STDIO
    if (binfp)
    {
		
        if (type == 0)
        {
            int nread;

            union
            {
                struct
                {
                    unsigned char f0;
                    unsigned char f1;
                    unsigned char f2;
                    unsigned char f3;
                };
                unsigned int tag;
            } flag_struct;

            nread = fread(&flag_struct, sizeof(flag_struct), 1, binfp);
            if (nread != 1)
            {
                fprintf(stderr, "ModelBin read flag_struct failed %d\n", nread);
                return Tensor<float>();
            }

            unsigned int flag = flag_struct.f0 + flag_struct.f1 + flag_struct.f2 + flag_struct.f3;
			/*
            if (flag_struct.tag == 0x01306B47)
            {
                // half-precision data
                int align_data_size = alignSize(w * sizeof(unsigned short), 4);
                std::vector<unsigned short> float16_weights;
                float16_weights.resize(align_data_size);
                nread = fread(float16_weights.data(), align_data_size, 1, binfp);
                if (nread != 1)
                {
                    fprintf(stderr, "ModelBin read float16_weights failed %d\n", nread);
                    return Tensor<float>();
                }

                return Tensor<float>::from_float16(float16_weights.data(), w);
            }
			*/
			std::vector<int> shape = { w };
			Tensor<float> m(shape);
            if (m.empty())
                return m;
			
            if (flag != 0)
            {
                // quantized data
                float quantization_value[256];
                nread = fread(quantization_value, 256 * sizeof(float), 1, binfp);
                if (nread != 1)
                {
                    fprintf(stderr, "ModelBin read quantization_value failed %d\n", nread);
                    return Tensor<float>();
                }

                int align_weight_data_size = alignSize(w * sizeof(unsigned char), 4);
                std::vector<unsigned char> index_array;
                index_array.resize(align_weight_data_size);
                nread = fread(index_array.data(), align_weight_data_size, 1, binfp);
                if (nread != 1)
                {
                    fprintf(stderr, "ModelBin read index_array failed %d\n", nread);
                    return Tensor<float>();
                }

                float* ptr = (float*)m.get_cpu_data_mutable();
                for (int i = 0; i < w; i++)
                {
                    ptr[i] = quantization_value[ index_array[i] ];
                }
            }
            else if (flag_struct.f0 == 0)
            {
                // raw data
                nread = fread((float*)m.get_cpu_data_mutable(), w * sizeof(float), 1, binfp);
                if (nread != 1)
                {
                    fprintf(stderr, "ModelBin read weight_data failed %d\n", nread);
                    return Tensor<float>();
                }
            }

            return m;
        }
        else if (type == 1)
        {
			std::vector<int> shape = { w };
			Tensor<float> m(shape);
            if (m.empty())
                return m;

            // raw data
            int nread = fread((float*)m.get_cpu_data_mutable(), w * sizeof(float), 1, binfp);
            if (nread != 1)
            {
                fprintf(stderr, "ModelBin read weight_data failed %d\n", nread);
                return Tensor<float>();
            }

            return m;
        }
        else
        {
            fprintf(stderr, "ModelBin load type %d not implemented\n", type);
            return Tensor<float>();
        }
    }
#endif // USE_STDIO

    if (type == 0)
    {
        union
        {
            struct
            {
                unsigned char f0;
                unsigned char f1;
                unsigned char f2;
                unsigned char f3;
            };
            unsigned int tag;
        } flag_struct;

        memcpy(&flag_struct, mem, sizeof(flag_struct));
        mem += sizeof(flag_struct);

        unsigned int flag = flag_struct.f0 + flag_struct.f1 + flag_struct.f2 + flag_struct.f3;
		/*
        if (flag_struct.tag == 0x01306B47)
        {
            // half-precision data
            Mat m = Mat::from_float16((unsigned short*)mem, w);
            mem += alignSize(w * sizeof(unsigned short), 4);
            return m;
        }
		*/
        if (flag != 0)
        {
            // quantized data
            const float* quantization_value = (const float*)mem;
            mem += 256 * sizeof(float);

            const unsigned char* index_array = (const unsigned char*)mem;
            mem += alignSize(w * sizeof(unsigned char), 4);

			std::vector<int> shape = { w };
			Tensor<float> m(shape);
            if (m.empty())
                return m;

            float* ptr = (float*)m.get_cpu_data_mutable();
            for (int i = 0; i < w; i++)
            {
                ptr[i] = quantization_value[ index_array[i] ];
            }

            return m;
        }
        else if (flag_struct.f0 == 0)
        {
            // raw data
			std::vector<int> shape = { w };
			Tensor<float> m((float*)mem, nullptr, shape, sizeof(float) * w);
            //Mat m = Mat(w, (float*)mem);
            mem += w * sizeof(float);
            return m;
        }
    }
    else if (type == 1)
    {
        // raw data
		std::vector<int> shape = { w };
		Tensor<float> m((float*)mem, nullptr, shape, sizeof(float) * w);
        //Mat m = Mat(w, (float*)mem);
        mem += w * sizeof(float);
        return m;
    }
    else
    {
        fprintf(stderr, "ModelBin load type %d not implemented\n", type);
        return Tensor<float>();
    }

    return Tensor<float>();
}

Tensor<float> ModelBin::load(int w, int h, int type) const
{
	Tensor<float> m = load(w * h, type);
    if (m.empty())
        return m;

    return m;
}

Tensor<float> ModelBin::load(int w, int h, int c, int type) const
{
	Tensor<float> m = load(w * h * c, type);
    if (m.empty())
        return m;

    return m;
}

} // namespace mercury
