/*
Copyright (c) 2015, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "device/api/nn_primitives_api_0.h"
#include "device/cpu/api_internal/cpu_device_internal.h"
#include "device/common/nn_workload_data.h"
#include "device/cpu/api_internal/data_helper.h"
#include "device/cpu/core/helper_zxyn_f32.h"
#include "device/cpu/core/layer_arithmetic_operation.h"

nn_device_t *NN_API_CALL_CONVENTION create_device_with_thread_count(size_t num_threads, NN_API_STATUS *status) {
    SET_STATUS(NN_API_STATUS_OK);
    return new nn_device_internal(num_threads);
}

NN_API_STATUS NN_API_CALL_CONVENTION delete_device(nn_device_t *device){
    delete static_cast<nn_device_internal*>(device);
    return NN_API_STATUS_OK;
}

NN_API_STATUS NN_API_CALL_CONVENTION delete_opaque_data(nn_opaque_data_t *opaque_data){
   delete reinterpret_cast<nn::workload_data<nn::layout_f32> *>(opaque_data);
   return NN_API_STATUS_OK;
}

/* Delete primitive handle and free its resources
Returns: NN_API_STATUS_OK on success
*/
NN_API_STATUS NN_API_CALL_CONVENTION delete_primitive(nn_primitive_handle_t primitive){
    delete primitive;
    return NN_API_STATUS_OK;
}
NN_API_STATUS NN_API_CALL_CONVENTION delete_event(nn_event_t event){
    return NN_API_STATUS_OK;
}

NN_API_STATUS NN_API_CALL_CONVENTION wait(size_t dependencies_count, nn_event_t *dependencies){
    return NN_API_STATUS_OK;
}

extern nn_primitives_0_t nn_primitives_0;

NN_API_CALL int32_t NN_API_CALL_CONVENTION
nn_device_get_primitives(uint32_t version,      /* version of interface to create */
                         void *const primitives /* pointer to interface structure */
                         ) {
    if(!primitives) return -2;
    if (version == 0) {
        *reinterpret_cast<nn_primitives_0_t *>(primitives) = nn_primitives_0;
        return 0;
    } else {
        return -3;
    }
}

NN_API_CALL int32_t NN_API_CALL_CONVENTION
nn_device_get_primitives_description(nn_device_primitives_description_t *const description /* pointer to description structure */
                                     ) {
    if(!description) return -2;
    else {
        *description = nn_device_primitives_description_t{
            NN_DEVICE_PRIMITIVES_TYPE_CPU, // type
            0,                             // version_first
            0,                             // version_last
            "CPU device",                  // name
            "floating point CPU device\n"  // description
            "[TBD]"                        // TODO: fill description
        };

        return 0;
    }
    return -1;
}

//template <typename T> struct nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIO, T> {
//    static const nn_workload_data_layout_t layout;
//};
//template <typename T>
//const nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIXYO, T>::layout = {
//    {0, 0, 0, 0, 0, 0}, // tile in log2(size)
//    {0, 0, 0, 0, 0, 0}, // alignment
//    {NN_DATA_COORD_p, NN_DATA_COORD_z, NN_DATA_COORD_x, NN_DATA_COORD_y, NN_DATA_COORD_q, NN_DATA_COORD_n}, // ordering
//    nn::type_to_datatype<T>::value};

void copy_data(nn_device_internal *device, nn_data_t *destination, const nn_workload_data_t *source) {
    switch (source->parent->layout.data_type) {
    case NN_DATATYPE_FLOAT:
        switch (source->parent->tag) {
        case NN_WORKLOAD_DATA_TAG_ZXYN:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_ZXYN, nn::layout_zxyn_f32>::copy(
                device,
                nn::data_cast<float, 4>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_zxyn_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_NX:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_NX, nn::layout_nx_f32>::copy(
                device,
                nn::data_cast<float, 2>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_nx_f32> *>(source));
            return;

        case NN_WORKLOAD_DATA_TAG_OBLOCKIXYO:  /* weights layout for convolution layer */
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIXYO, nn::layout_pzxyqn_f32>::copy<false>(
                device,
                nn::data_cast<float, 4>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_pzxyqn_f32> *>(source));
            return;

        case NN_WORKLOAD_DATA_TAG_O:          /* bias layout */
            nn::data_helper<NN_WORKLOAD_DATA_TAG_O, nn::layout_nxyzpq_f32>::copy<false>(
                device,
                nn::data_cast<float, 1>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_nxyzpq_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OI:         /* weights layout for fully connected layer, no batching */
            if (destination->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_OI, nn::layout_yxzpqn_f32>::copy<false>(
                    device,
                    nn::data_cast<float, 4>(destination),
                    reinterpret_cast<const nn::workload_data<nn::layout_yxzpqn_f32> *>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OI, nn::layout_yxzpqn_f32>::copy<false>(
                device,
                nn::data_cast<float, 2>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_yxzpqn_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OBLOCKIO:   /* weights layout for fully connected layer, with batching */
            if (destination->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIO, nn::layout_pzxyqn_f32>::copy<false>(
                    device,
                    nn::data_cast<float, 4>(destination),
                    reinterpret_cast<const nn::workload_data<nn::layout_pzxyqn_f32> *>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIO, nn::layout_pzxyqn_f32>::copy<false>(
                device,
                nn::data_cast<float, 2>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_pzxyqn_f32> *>(source));
            return;
        }
        break;
    case NN_DATATYPE_INT16:
        /*case NN_WORKLOAD_DATA_TAG_ZBLOCKXYZN:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_ZBLOCKXYZN, int16_t>::copy(
                device,
                nn::data_cast<int16_t, 4>(destination),
                reinterpret_cast<nn::workload_data<int16_t> *>(source));
            return;
        break;*/
    case NN_DATATYPE_INT32:
        break;
    }

    throw std::runtime_error("workload data tag support not implemented (" + std::to_string(source->parent->tag) + ")");
}

void copy_delta(nn_device_internal *device, nn_data_t *destination, const nn_workload_data_t *source) {

    assert(source->parent->delta_buffer != nullptr);

    switch (source->parent->layout.data_type) {
    case NN_DATATYPE_FLOAT:
        switch (source->parent->tag) {
        case NN_WORKLOAD_DATA_TAG_ZXYN:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_ZXYN, nn::layout_zxyn_f32>::copy<true>(
                device,
                nn::data_cast<float, 4>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_zxyn_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_NX:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_NX, nn::layout_nx_f32>::copy<true>(
                device,
                nn::data_cast<float, 2>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_nx_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OBLOCKIXYO:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIXYO, nn::layout_oblockixyo_f32>::copy<true>(
                device,
                nn::data_cast<float, 4>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_oblockixyo_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_O:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_O, nn::layout_o_f32>::copy<true>(
                device,
                nn::data_cast<float, 1>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_o_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OI:
            if(destination->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_OI, nn::layout_oi_f32>::copy<true>(
                    device,
                    nn::data_cast<float, 4>(destination),
                    reinterpret_cast<const nn::workload_data<nn::layout_oi_f32> *>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OI, nn::layout_oi_f32>::copy<true>(
                device,
                nn::data_cast<float, 2>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_oi_f32> *>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OBLOCKIO:
            if(destination->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIO, nn::layout_oblockio_f32>::copy<true>(
                    device,
                    nn::data_cast<float, 4>(destination),
                    reinterpret_cast<const nn::workload_data<nn::layout_oblockio_f32> *>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIO, nn::layout_oblockio_f32>::copy<true>(
                device,
                nn::data_cast<float, 2>(destination),
                reinterpret_cast<const nn::workload_data<nn::layout_oblockio_f32> *>(source));
            return;
        }
        break;
    case NN_DATATYPE_INT16:
        break;
    case NN_DATATYPE_INT32:
        break;
    }

    throw std::runtime_error("workload data tag support not implemented ("
        + std::to_string(source->parent->layout.data_type) + ")");
}

template <bool copy_deltas>
void copy_data_or_delta(nn_device_internal *device, nn_workload_data_t *destination, const nn_data_t *source) {
    switch (destination->parent->layout.data_type) {
    case NN_DATATYPE_FLOAT:
        switch (destination->parent->tag) {
        case NN_WORKLOAD_DATA_TAG_NBLOCKZXYN:/* layout for asmjit convolution layer */
            nn::data_helper<NN_WORKLOAD_DATA_TAG_NBLOCKZXYN, nn::layout_nblockzxyn_f32> ::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_nblockzxyn_f32> *>(destination),
                nn::data_cast<float, 4>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OBLOCKIOXY:/* weights layout for asmjit convolution layer */
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIOXY, nn::layout_pzqxyn_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_pzqxyn_f32> *>(destination),
                nn::data_cast<float, 4>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_ZXYN:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_ZXYN, nn::layout_zxyn_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_zxyn_f32> *>(destination),
                nn::data_cast<float, 4>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_ZXY:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_ZXY, nn::layout_zxy_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_zxy_f32> *>(destination),
                nn::data_cast<float, 3>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_NX:
            if (source->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_NX, nn::layout_nx_f32>::copy<copy_deltas>(
                    device,
                    reinterpret_cast<nn::workload_data<nn::layout_nx_f32> *>(destination),
                    nn::data_cast<float, 4>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_NX, nn::layout_nx_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_nx_f32> *>(destination),
                nn::data_cast<float, 2>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_O:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_O, nn::layout_o_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_o_f32> *>(destination),
                nn::data_cast<float, 1>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OI:
            if (source->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_OI, nn::layout_oi_f32>::copy<copy_deltas>(
                    device,
                    reinterpret_cast<nn::workload_data<nn::layout_oi_f32> *>(destination),
                    nn::data_cast<float, 4>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OI, nn::layout_oi_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_oi_f32> *>(destination),
                nn::data_cast<float, 2>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OBLOCKIO:
            if (source->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIO, nn::layout_oblockio_f32>::copy<copy_deltas>(
                    device,
                    reinterpret_cast<nn::workload_data<nn::layout_oblockio_f32> *>(destination),
                    nn::data_cast<float, 4>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIO, nn::layout_oblockio_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_oblockio_f32> *>(destination),
                nn::data_cast<float, 2>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_OBLOCKIXYO:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_OBLOCKIXYO, nn::layout_oblockixyo_f32>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<nn::layout_oblockixyo_f32> *>(destination),
                nn::data_cast<float, 4>(source));
            return;
        }
        break;
    case NN_DATATYPE_INT16:
        switch (destination->parent->tag) {
        case NN_WORKLOAD_DATA_TAG_I2O32IXYO:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_I2O32IXYO, int16_t>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<int16_t> *>(destination),
                nn::data_cast<int16_t, 4>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_O:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_O, int16_t>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<int16_t> *>(destination),
                nn::data_cast<int16_t, 1>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_I2O8IO:
            if (source->dimension == 4) {
                nn::data_helper<NN_WORKLOAD_DATA_TAG_I2O8IO, int16_t>::copy<copy_deltas>(
                    device,
                    reinterpret_cast<nn::workload_data<int16_t> *>(destination),
                    nn::data_cast<int16_t, 4>(source));
                return;
            }
            nn::data_helper<NN_WORKLOAD_DATA_TAG_I2O8IO, int16_t>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<int16_t> *>(destination),
                nn::data_cast<int16_t, 2>(source));
            return;
        case NN_WORKLOAD_DATA_TAG_ZBLOCKXYZN:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_ZBLOCKXYZN, int16_t>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<int16_t> *>(destination),
                nn::data_cast<int16_t, 4>(source));
            return;
        }
        break;
    case NN_DATATYPE_INT32:
        switch (destination->parent->tag) {
        case NN_WORKLOAD_DATA_TAG_O:
            nn::data_helper<NN_WORKLOAD_DATA_TAG_O, int32_t>::copy<copy_deltas>(
                device,
                reinterpret_cast<nn::workload_data<int32_t> *>(destination),
                nn::data_cast<int32_t, 1>(source));
            return;
        }
        break;
    }

    throw std::runtime_error("workload data tag support not implemented (" + std::to_string(destination->parent->tag) + ")");
}

void copy_data(nn_device_internal *device, nn_workload_data_t *destination, const nn_data_t *source)
{
    copy_data_or_delta<false>(device, destination, source);
}

void copy_delta(nn_device_internal *device, nn_workload_data_t *destination, const nn_data_t *source)
{
    copy_data_or_delta<true>(device, destination, source);
}


nn_event_t NN_API_CALL_CONVENTION copy_to_opaque_async(
    nn_device_t *device,
    nn_opaque_data_t *destination, /* public data storage to copy data into */
    nn_data_t *source,             /* internal data storage to copy from */
    size_t dependency_count,       /* size of dependencies array */
    nn_event_t dependency_array[], /* array of nn_event_t objects for tasks that need to be
                                   completed before the copy is started */
    NN_API_STATUS *status          /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    copy_data(static_cast<nn_device_internal *>(device), reinterpret_cast<nn_workload_data_t *>(destination), source);

    SET_STATUS(NN_API_STATUS_OK);
    return {};
}

nn_event_t NN_API_CALL_CONVENTION copy_from_opaque_async(
    nn_device_t *device,
    nn_data_t *destination,        /* public data storage to copy data into */
    nn_opaque_data_t *source,      /* internal data storage to copy from */
    size_t dependency_count,       /* size of dependencies array */
    nn_event_t dependency_array[], /* array of nn_event_t objects for tasks that need to be
                                   completed before the copy is started */
    NN_API_STATUS *status          /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    copy_data(static_cast<nn_device_internal *>(device), destination, reinterpret_cast<nn_workload_data_t *>(source));

    SET_STATUS(NN_API_STATUS_OK);
    return {};
}

nn_event_t NN_API_CALL_CONVENTION copy_delta_to_opaque_async(
    nn_device_t *device,
    nn_opaque_data_t *destination, /* public data storage to copy data into */
    nn_data_t *source,             /* internal data storage to copy from */
    size_t dependency_count,       /* size of dependencies array */
    nn_event_t dependency_array[], /* array of nn_event_t objects for tasks that need to be
                                   completed before the copy is started */
    NN_API_STATUS *status          /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    copy_delta(static_cast<nn_device_internal *>(device), reinterpret_cast<nn_workload_data_t *>(destination), source);

    SET_STATUS(NN_API_STATUS_OK);
    return {};
}

nn_event_t NN_API_CALL_CONVENTION copy_delta_from_opaque_async(
    nn_device_t *device,
    nn_data_t *destination,        /* public data storage to copy data into */
    nn_opaque_data_t *source,      /* internal data storage to copy from */
    size_t dependency_count,       /* size of dependencies array */
    nn_event_t dependency_array[], /* array of nn_event_t objects for tasks that need to be
                                   completed before the copy is started */
    NN_API_STATUS *status          /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    copy_delta(static_cast<nn_device_internal *>(device), destination, reinterpret_cast<nn_workload_data_t *>(source));

    SET_STATUS(NN_API_STATUS_OK);
    return {};
}

nn_event_t NN_API_CALL_CONVENTION copy_opaque_to_opaque_async(
    nn_device_t *device,
    NN_DATA_OR_DELTA destination_data_or_delta, /* specifies if data or delta buffer is used as destination */
    nn_opaque_data_t *destination,              /* internal data storage to copy data into */
    NN_DATA_OR_DELTA source_data_or_delta,      /* specifies if data or delta buffer is used as source */
    nn_opaque_data_t *source,                   /* internal data storage to copy from */
    size_t dependency_count,                    /* size of dependencies array */
    nn_event_t dependency_array[],              /* array of nn_event_t objects for tasks that need to be
                                                   completed before the copy is started */
    NN_API_STATUS *status                       /* set to NN_API_STATUS_OK on scheduling success */
    ) {

    auto src_data = reinterpret_cast<nn_workload_data_t *>(source);
    auto dst_data = reinterpret_cast<nn_workload_data_t *>(destination);

    // Currently we only support simple memcpy.
    assert(src_data->parent->layout == dst_data->parent->layout);
    assert(src_data->parent->lengths == dst_data->parent->lengths);
    assert(src_data != dst_data);

    auto src_buffer = src_data->parent->data_buffer;
    auto dst_buffer = dst_data->parent->data_buffer;

	if (source_data_or_delta == USE_DELTA)
        src_buffer = src_data->parent->delta_buffer;
    
	if (destination_data_or_delta == USE_DELTA)
        dst_buffer = dst_data->parent->delta_buffer;

    assert(src_buffer != dst_buffer);
    memcpy(dst_buffer, src_buffer, dst_data->parent->buffer_size);

    SET_STATUS(NN_API_STATUS_OK);
    return{};
}

nn_event_t NN_API_CALL_CONVENTION axpby_async(
    nn_device_t *device,
    NN_DATA_OR_DELTA X_data_or_delta,          /* specifies if data or delta buffer is used for X */
	float alpha,                               /* alpha parameter in Y=alpha*X+beta*Y */
    nn_opaque_data_t *X,                       /* X data in private data storage */
    NN_DATA_OR_DELTA Y_data_or_delta,          /* specifies if data or delta buffer is used for Y */
    float beta,                                /* beta parameter in Y=alpha*X+beta*Y */
    nn_opaque_data_t *Y,                       /* Y data in private data storage */
    size_t dependency_count,                   /* size of dependencies array */
    nn_event_t dependency_array[],             /* array of nn_event_t objects for tasks that need to be
                                                  completed before the execution is started */
    NN_API_STATUS *status                      /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    auto x_input = reinterpret_cast<nn::workload_data<>*>(X);
    auto y_input = reinterpret_cast<nn::workload_data<>*>(Y);

    assert(x_input->parent->layout == y_input->parent->layout);
    assert(x_input->parent->lengths == y_input->parent->lengths);

    nn::workload_data<> x(
        (X_data_or_delta == USE_DELTA) ? x_input->parent->delta_buffer : x_input->parent->data_buffer,
        x_input->parent->lengths, x_input->parent->layout);
    
    nn::workload_data<> y(
        (Y_data_or_delta == USE_DELTA) ? y_input->parent->delta_buffer : y_input->parent->data_buffer,
        y_input->parent->lengths, y_input->parent->layout);

    // TODO: This probably should be optimized.
    layer::arithmetic_f32 arithmetic(0, 0, 0, // not used internally
                                     NN_ARITHMETIC_FUNCTION_ADDITION,
                                     0,       // not used internally
                                     alpha, beta, 1,
                                     static_cast<nn_device_internal *>(device));

    arithmetic.forward({ reinterpret_cast<nn_workload_data_t *const >(&x) },
                       { reinterpret_cast<nn_workload_data_t *const >(&y) }, 
                       { reinterpret_cast<nn_workload_data_t *>(&y) });

    SET_STATUS(NN_API_STATUS_OK);
    return{};
}

nn_event_t NN_API_CALL_CONVENTION forward_async(
    nn_primitive_handle_t handle,              /* primitive handle */
    size_t input_count,                        /* */
    nn_opaque_data_t *const input_array[],     /* internal data storage with inputs */
    size_t parameter_count,                    /* */
    nn_opaque_data_t *const parameter_array[], /* internal data storage with weights */
    size_t output_count,                       /* */
    nn_opaque_data_t *output_array[],          /* internal data storage to store outputs */
    size_t dependency_count,                   /* size of dependencies array */
    nn_event_t dependency_array[],             /* array of nn_event_t objects for tasks that need to be
                                                 completed before the execution is started */
    NN_API_STATUS *status                      /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    handle->forward({reinterpret_cast<nn_workload_data_t *const *>(&input_array[0]),
                     reinterpret_cast<nn_workload_data_t *const *>(&input_array[input_count])},
                    {reinterpret_cast<nn_workload_data_t *const *>(&parameter_array[0]),
                     reinterpret_cast<nn_workload_data_t *const *>(&parameter_array[parameter_count])},
                    {reinterpret_cast<nn_workload_data_t **>(&output_array[0]),
                     reinterpret_cast<nn_workload_data_t **>(&output_array[output_count])});

    SET_STATUS(NN_API_STATUS_OK);
    return {};
}

nn_event_t NN_API_CALL_CONVENTION backward_async(
    nn_primitive_handle_t handle,              /* primitive handle */
    size_t input_count,                        /* */
    nn_opaque_data_t *input_array[],     /* internal data storage with inputs */
    size_t parameter_count,                    /* */
    nn_opaque_data_t *const parameter_array[], /* internal data storage with weights */
    size_t output_count,                       /* */
    nn_opaque_data_t *const output_array[],    /* internal data storage to store outputs */
    size_t dependency_count,                   /* size of dependencies array */
    nn_event_t dependency_array[],             /* array of nn_event_t objects for tasks that need to be
                                                 completed before the execution is started */
    NN_API_STATUS *status                      /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    handle->backward({reinterpret_cast<nn_workload_data_t **>(&input_array[0]),
                      reinterpret_cast<nn_workload_data_t **>(&input_array[input_count])},
                     {reinterpret_cast<nn_workload_data_t *const *>(&parameter_array[0]),
                      reinterpret_cast<nn_workload_data_t *const *>(&parameter_array[parameter_count])},
                     {reinterpret_cast<nn_workload_data_t *const *>(&output_array[0]),
                      reinterpret_cast<nn_workload_data_t *const *>(&output_array[output_count])});

    SET_STATUS(NN_API_STATUS_OK);
    return {};
}

nn_event_t NN_API_CALL_CONVENTION backward_parameter_async(
    nn_primitive_handle_t handle,              /* primitive handle */
    size_t data_index,                         /* optional index in primitive storage lists */
    size_t input_count,                        /* */
    nn_opaque_data_t *const input_array[],     /* internal data storage with inputs */
    size_t parameter_count,                    /* */
    nn_opaque_data_t *parameter_array[], /* internal data storage with weights */
    size_t output_count,                       /* */
    nn_opaque_data_t *const output_array[],    /* internal data storage to store outputs */
    size_t dependency_count,                   /* size of dependencies array */
    nn_event_t dependency_array[],             /* array of nn_event_t objects for tasks that need to be
                                                 completed before the execution is started */
    NN_API_STATUS *status                      /* set to NN_API_STATUS_OK on scheduling success */
    ) {
    handle->backward_parameter(data_index,
                               {reinterpret_cast<nn_workload_data_t *const *>(&input_array[0]),
                                reinterpret_cast<nn_workload_data_t *const *>(&input_array[input_count])},
                               {reinterpret_cast<nn_workload_data_t **>(&parameter_array[0]),
                                reinterpret_cast<nn_workload_data_t **>(&parameter_array[parameter_count]) },
                               {reinterpret_cast<nn_workload_data_t *const *>(&output_array[0]),
                                reinterpret_cast<nn_workload_data_t *const *>(&output_array[output_count])});

    SET_STATUS(NN_API_STATUS_OK);
    return {};
}


void NN_API_CALL_CONVENTION create_parameters(
    nn_primitive_handle_t handle,      /* primitive handle */
    const size_t storage_count,        /* number of handles to create */
    nn_opaque_data_t *storage_array[], /* array of pointers to create, must hold storage_count pointers */
    uint32_t flags,                    /* optional flags - e.g. to request allocation of buffer for deltas */
    NN_API_STATUS *status              /* set to NN_API_STATUS_OK on success */
    ) {
    auto parameters = handle->create_parameters(flags & NN_OPAQUE_DATA_FLAGS_ALLOC_DELTA);
    if (parameters.size() != storage_count) {
        SET_STATUS(NN_API_STATUS_ERROR_OTHER);
        return;
    }

    for (size_t i = 0; i < storage_count; ++i)
        storage_array[i] = reinterpret_cast<nn_opaque_data_t *>(parameters[i]);

    SET_STATUS(NN_API_STATUS_OK);
    return;
}

void NN_API_CALL_CONVENTION create_inputs(
    nn_primitive_handle_t handle,      /* primitive handle */
    const size_t storage_count,        /* number of handles to create */
    nn_opaque_data_t *storage_array[], /* array of pointers to create, must hold storage_count pointers */
    uint32_t flags,                    /* optional flags - e.g. to request allocation of buffer for deltas */
    NN_API_STATUS *status              /* set to NN_API_STATUS_OK on success */
    ) {
    auto parameters = handle->create_inputs(flags & NN_OPAQUE_DATA_FLAGS_ALLOC_DELTA);
    if (parameters.size() != storage_count) {
        SET_STATUS(NN_API_STATUS_ERROR_OTHER);
        return;
    }

    for (size_t i = 0; i < storage_count; ++i)
        storage_array[i] = reinterpret_cast<nn_opaque_data_t *>(parameters[i]);

    SET_STATUS(NN_API_STATUS_OK);
    return;
}


int NN_API_CALL_CONVENTION validate_input(
    nn_primitive_handle_t handle, /* primitive handle */
    size_t data_index,            /* optional index in primitive storage lists */
    nn_opaque_data_t *opaque_data /* internal data storage handle to validate */
    ) {
    return handle->validate_input(data_index, reinterpret_cast<nn_workload_data_t*>(opaque_data));
}

nn_opaque_data_t *NN_API_CALL_CONVENTION map_input(nn_primitive_handle_t handle, /* primitive handle */
                                                   size_t data_index, /* optional index in primitive storage lists */
                                                   const nn_data_t *source, /* source data in public data storage */
                                                   NN_API_STATUS *status    /* set to NN_API_STATUS_OK on success */
                                                   ) {
    auto helper = dynamic_cast<layer::helper_zxyn_f32::primitive_zxyn_f32_base*>(handle);
    assert(helper != nullptr);
    assert(data_index == 0);

    auto result = helper->map_input(*nn::data_cast<float, 4>(source));

    SET_STATUS(NN_API_STATUS_OK);
    return reinterpret_cast<nn_opaque_data_t *>(result);
}

void NN_API_CALL_CONVENTION create_outputs(
    nn_primitive_handle_t handle,      /* primitive handle */
    const size_t storage_count,        /* number of handles to create */
    nn_opaque_data_t *storage_array[], /* array of pointers to create, must hold storage_count pointers */
    uint32_t flags,                    /* optional flags - e.g. to request also allocation of buffer for deltas */
    NN_API_STATUS *status              /* set to NN_API_STATUS_OK on success */
    )
{
    auto parameters = handle->create_outputs(flags & NN_OPAQUE_DATA_FLAGS_ALLOC_DELTA);
    if (parameters.size() != storage_count) {
        SET_STATUS(NN_API_STATUS_ERROR_OTHER);
        return;
    }

    for (size_t i = 0; i < storage_count; ++i)
        storage_array[i] = reinterpret_cast<nn_opaque_data_t *>(parameters[i]);

    SET_STATUS(NN_API_STATUS_OK);
    return;
}

namespace
{
template <typename T>
nn_opaque_data_t* create_split_z_view(
    nn_workload_data_t* psource,
    uint32_t begin_of_z,
    uint32_t end_of_z)
{
    auto source = static_cast<nn::workload_data<T>*>(psource);
    nn_workload_data_coords_t view_begin(0, 0, 0, begin_of_z, 0, 0);
    nn_workload_data_coords_t view_end(
        source->get_length(NN_DATA_COORD_n) - 1,
        source->get_length(NN_DATA_COORD_x) - 1,
        source->get_length(NN_DATA_COORD_y) - 1,
        end_of_z,
        source->get_length(NN_DATA_COORD_p) - 1,
        source->get_length(NN_DATA_COORD_q) - 1);
    return reinterpret_cast<nn_opaque_data_t*>(new nn::workload_data<T>(*source, view_begin, view_end));
}

} //namespace

NN_API_STATUS NN_API_CALL_CONVENTION split_z(
    const size_t part_count,        /* number of partitions to create */
    nn_opaque_data_t *part_array[], /* array of pointers to create, must hold partition_count pointers */
    nn_opaque_data_t *psource        /* source data in public data storage */
    )
{
    const auto source = reinterpret_cast<nn_workload_data_t*>(psource);

    auto z_length = source->view_end.t[NN_DATA_COORD_z] - source->view_begin.t[NN_DATA_COORD_z] + 1;
    assert(z_length % part_count == 0);
    uint32_t z_view_size = static_cast<uint32_t>(z_length / part_count);

    typedef nn_workload_data_coords_t Coords;
    auto create = [&](uint32_t begin_of_z, uint32_t end_of_z) {
        switch (source->parent->layout.data_type)
        {
            case NN_DATATYPE_FLOAT:
                return create_split_z_view<nn::layout_f32>(source, begin_of_z, end_of_z);
            case NN_DATATYPE_INT16:
                return create_split_z_view<int16_t>(source, begin_of_z, end_of_z);
            case NN_DATATYPE_INT32:
                return create_split_z_view<int32_t>(source, begin_of_z, end_of_z);
            default:
                assert(0);
                return static_cast<nn_opaque_data_t *>(nullptr);
        }
    };

    for (uint32_t i = 0; i < part_count; ++i)
        part_array[i] = create(i * z_view_size, (i + 1) * z_view_size - 1);
    return NN_API_STATUS_OK;
}

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convolution_f32_create_0(
    nn_device_t *device,    /* IDLF device handle */
    size_t kernel_w,        /* kernel width */
    size_t kernel_h,        /* kernel height */
    size_t num_input,       /* number of input feature maps */
    size_t num_output,      /* number of output feature maps */
    size_t output_w,        /* output width */
    size_t output_h,        /* output height */
    size_t center_offset_x, /* horizontal offset of kernel's center point w/ relation to top left corner */
    size_t center_offset_y, /* vertical offset of kernel's center point w/ relation to top left corner */
    size_t stride_x,        /* horizontal stride */
    size_t stride_y,        /* vertical stride */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    const nn_primitives_convolution_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convolution_i16_create_0(
    nn_device_t *device,    /* IDLF device handle */
    size_t kernel_w,        /* kernel width */
    size_t kernel_h,        /* kernel height */
    size_t num_input,       /* number of input feature maps */
    size_t num_output,      /* number of output feature maps */
    size_t output_w,        /* output width */
    size_t output_h,        /* output height */
    size_t center_offset_x, /* horizontal offset of kernel's center point w/ relation to top left corner */
    size_t center_offset_y, /* vertical offset of kernel's center point w/ relation to top left corner */
    size_t stride_x,        /* horizontal stride */
    size_t stride_y,        /* vertical stride */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    const nn_primitives_convolution_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION
nn_primitives_arithmetic_f32_create_0(nn_device_t *device,
                                      size_t image_size_x,
                                      size_t image_size_y,
                                      size_t image_size_z,
                                      NN_ARITHMETIC_FUNCTION arithmetic_function,
                                      size_t batch_size,
                                      NN_API_STATUS *status);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_softmax_f32_create_0(
    nn_device_t *device,  /* IDLF device handle */
    size_t num_features,  /* number of input feature maps */
    size_t batch_size,    /* size of input batch */
    const nn_primitives_softmax_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_softmax_i32_create_0(
    nn_device_t *device,  /* IDLF device handle */
    size_t num_features,  /* number of input feature maps */
    size_t batch_size,    /* size of input batch */
    const nn_primitives_softmax_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convert_float_to_i16_create_0(
    nn_device_t *device,  /* IDLF device handle */
    size_t image_size_x,  /* image width */
    size_t image_size_y,  /* image height */
    size_t image_size_z,  /* number of feature maps */
    size_t batch_size,    /* size of input batch */
    const nn_primitives_convert_float_to_i16_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION
nn_primitives_relu_f32_create_0(nn_device_t *device,
                                size_t image_size_x,
                                size_t image_size_y,
                                size_t image_size_z,
                                size_t batch_size,
                                const nn_primitives_relu_hints_t* hints,
                                NN_API_STATUS *status);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION
nn_primitives_pooling_f32_create_0(nn_device_t *device,
                                   NN_POOLING_MODE pooling_mode,
                                   size_t pool_size_x,
                                   size_t pool_size_y,
                                   size_t pool_stride_x,
                                   size_t pool_stride_y,
                                   size_t num_feature_maps,
                                   size_t output_w,
                                   size_t output_h,
                                   const int32_t center_offset_x,
                                   const int32_t center_offset_y,
                                   size_t batch_size,
                                   const nn_primitives_pooling_hints_t* hints,
                                   NN_API_STATUS *status);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION
nn_primitives_pooling_i16_create_0(nn_device_t *device,
                                   NN_POOLING_MODE pooling_mode,
                                   size_t pool_size_x,
                                   size_t pool_size_y,
                                   size_t pool_stride_x,
                                   size_t pool_stride_y,
                                   size_t num_feature_maps,
                                   size_t output_w,
                                   size_t output_h,
                                   const int32_t center_offset_x,
                                   const int32_t center_offset_y,
                                   size_t batch_size,
                                   const nn_primitives_pooling_hints_t* hints,
                                   NN_API_STATUS *status);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION
nn_primitives_normalization_elementwise_linear_f32_create_0(nn_device_t *device, /* IDLF device handle */
                                                            float alpha,         /* multiplier */
                                                            float beta,          /* offset */
                                                            size_t image_size_x, /* image width */
                                                            size_t image_size_y, /* image height */
                                                            size_t image_size_z, /* number of feature maps */
                                                            size_t batch_size,   /* size of input batch */
                                                            NN_API_STATUS *status /* NN_API_STATUS_OK on success */);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_normalization_response_across_maps_f32_create_0(
    nn_device_t *device, /* IDLF device handle */
    float alpha,         /* sum scale */
    float beta,          /* sum power */
    uint32_t k,          /* square sum weight */
    uint32_t n,          /* size of moving window on the feature maps */
    size_t image_size_x, /* image width */
    size_t image_size_y, /* image height */
    size_t image_size_z, /* number of feature maps */
    size_t batch_size,   /* size of input batch */
    const nn_primitives_normalization_response_across_maps_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_normalization_response_across_maps_i16_create_0(
    nn_device_t *device, /* IDLF device handle */
    float alpha,         /* sum scale */
    float beta,          /* sum power */
    uint32_t k,          /* square sum weight */
    uint32_t n,          /* size of moving window on the feature maps */
    size_t image_size_x, /* image width */
    size_t image_size_y, /* image height */
    size_t image_size_z, /* number of feature maps */
    size_t batch_size,   /* size of input batch */
    const nn_primitives_normalization_response_across_maps_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_fully_connected_f32_create_0(
    nn_device_t *device,                        /* IDLF device handle */
    size_t num_input,                           /* number of input feature maps */
    size_t num_output,                          /* number of output feature maps */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    const nn_primitives_fully_connected_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convolution_pooling_f32_create_0(
    nn_device_t *device,    /* IDLF device handle */
    size_t kernel_w,        /* convolution kernel width */
    size_t kernel_h,        /* convolution kernel height */
    size_t num_input,       /* number of input feature maps */
    size_t num_output,      /* number of output feature maps */
    size_t output_w,        /* output width */
    size_t output_h,        /* output height */
    size_t center_offset_x, /* horizontal offset of kernel's center point w/ relation to top left corner */
    size_t center_offset_y, /* vertical offset of kernel's center point w/ relation to top left corner */
    size_t stride_x,        /* convolution horizontal stride */
    size_t stride_y,        /* convolution vertical stride */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    size_t pooling_kernel_w,                    /* width of pooling kernel */
    size_t pooling_kernel_h,                    /* height of pooling kernel */
    size_t pooling_stride_x,                    /* horizontal pooling stride */
    size_t pooling_stride_y,                    /* vertical pooling stride */
    const nn_primitives_convolution_pooling_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convolution_pooling_i16_create_0(
    nn_device_t *device,    /* IDLF device handle */
    size_t kernel_w,        /* convolution kernel width */
    size_t kernel_h,        /* convolution kernel height */
    size_t num_input,       /* number of input feature maps */
    size_t num_output,      /* number of output feature maps */
    size_t output_w,        /* output width */
    size_t output_h,        /* output height */
    size_t center_offset_x, /* horizontal offset of kernel's center point w/ relation to top left corner */
    size_t center_offset_y, /* vertical offset of kernel's center point w/ relation to top left corner */
    size_t stride_x,        /* convolution horizontal stride */
    size_t stride_y,        /* convolution vertical stride */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    size_t pooling_kernel_w,                    /* width of pooling kernel */
    size_t pooling_kernel_h,                    /* height of pooling kernel */
    size_t pooling_stride_x,                    /* horizontal pooling stride */
    size_t pooling_stride_y,                    /* vertical pooling stride */
    const nn_primitives_convolution_pooling_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convert_zxyn_nx_f32_create_0(
    nn_device_t *device, /* IDLF device handle */
    size_t input_size_x,
    size_t input_size_y,
    size_t input_size_z,
    size_t batch_size,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convert_z_block_xyz_x2nx_i16_create_0(
    nn_device_t *device, /* IDLF device handle */
    size_t input_size_x,
    size_t input_size_y,
    size_t input_size_z,
    size_t batch_size,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_convert_z2nz_n8xn_create_0(
    nn_device_t *device, /* IDLF device handle */
    size_t input_size_x,
    size_t batch_size,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION
    nn_primitives_convert_from_zxyn_to_batch_block_format_nzxyn_create(
        nn_device_t *device,
        size_t input_size_x,
        size_t input_size_y,
        size_t input_size_z,
        size_t batch_size);
extern nn_primitive_handle_t NN_API_CALL_CONVENTION
    nn_primitives_convert_from_batch_block_format_to_zxyn_create(
        nn_device_t *device,
        size_t input_size_x,
        size_t input_size_y,
        size_t input_size_z,
        size_t batch_size);

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_loss_f32_create_0(
    nn_device_t *device, /* IDLF device handle */
    NN_LOSS_FUNCTION function,
    size_t input_size_x,
    size_t input_size_y,
    size_t input_size_z,
    size_t batch_size,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_dropout_f32_create_0(
    nn_device_t *device,  /* IDLF device handle */
    size_t image_size_x,  /* image width */
    size_t image_size_y,  /* image height */
    size_t image_size_z,  /* number of feature maps */
    size_t batch_size,    /* size of input batch */
    float drop_rate,      /* drop rate */
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_fully_connected_i16_create_0(
    nn_device_t *device,                        /* IDLF device handle */
    size_t num_input,                           /* number of input feature maps */
    size_t num_output,                          /* number of output feature maps */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    const nn_primitives_fully_connected_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_fully_connected_i16_i32_create_0(
    nn_device_t *device,                        /* IDLF device handle */
    size_t num_input,                           /* number of input feature maps */
    size_t num_output,                          /* number of output feature maps */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    const nn_primitives_fully_connected_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

extern nn_primitive_handle_t NN_API_CALL_CONVENTION nn_primitives_fully_connected_i16_i32_create_0(
    nn_device_t *device,                        /* IDLF device handle */
    size_t num_input,                           /* number of input feature maps */
    size_t num_output,                          /* number of output feature maps */
    const nn_argument_activation_t *activation, /* struct parameterizing optional activation function */
    size_t batch_size,                          /* size of input batch */
    const nn_primitives_fully_connected_hints_t* hints,
    NN_API_STATUS *status /* NN_API_STATUS_OK on success */
    );

nn_primitives_0_t nn_primitives_0 {
    /* Creates nn_device_t handle which can be then used to instantiate primitives */
    create_device_with_thread_count,

    /* Deletes nn_device_t to free resources */
    delete_device,

    /* Delete internal storage handle and free its resources */
    delete_opaque_data,

    /* Delete primitive handle and free its resources */
    delete_primitive,

    /* Delete event handle and free its resources */
    delete_event,

    /* Wait until dependencies are ready */
    wait,

    /* storage allocation methods
        ************************************************************************************/

    /* allocate input buffer for a primitive */
    create_inputs,

    /* allocate output buffer for a primitive */
    create_outputs,

    /* allocate vector of buffers for primitive parameters, see primitive description for details */
    create_parameters,

    /* storage transfer methods **************************************************************************************/

    /* Copy data to internal storage */
    copy_to_opaque_async,

    /* Copy data from internal storage */
    copy_from_opaque_async,

    /* Copy delta aka diffs to internal storage */
    copy_delta_to_opaque_async,

    /* Copy delta aka diffs from internal storage */
    copy_delta_from_opaque_async,

    /* Copy from one internal storage to another */
    copy_opaque_to_opaque_async,

    /* storage manipulation methods **********************************************************************************/

    /* create opaque handle directly from public data store (requires exact memory layout) */
    map_input,

    /* validate input buffer is correct for the primitive */
    validate_input,

    /* can be used to transform internal output storage of another primitive to use as input */
    nullptr,

    /* splits internal container along Z (features) axis and creates N views into its parts */
    split_z,

    /* arithmetic methods *********************************************************************************************/
    axpby_async,

    /* execution methods *********************************************************************************************/

    /* forward pass */
    forward_async,

    /* backward pass */
    backward_async,
    backward_parameter_async,

    {
        /* convolution

        nn_data_t input/output format: ZXYN

        parameters:
            [0] - weights handle
                    weights nn_data_t dimensions:
        [kernel_width][kernel_height][number_of_input_feature_maps][number_of_output_feature_maps]
            [1] - bias handle
                    bias nn_data_t dimensions: [number_of_output_feature_maps]
        */
        nn_primitives_convolution_f32_create_0,
        nn_primitives_convolution_i16_create_0,

        /* pooling

        nn_data_t input/output format: ZXYN
        */
        nn_primitives_pooling_f32_create_0,
        nn_primitives_pooling_i16_create_0,

        /* convolution with pooling

        nn_data_t input/output format: ZXYN

        parameters:
            [0] - weights handle
                    weights nn_data_t dimensions:
        [kernel_width][kernel_height][number_of_input_feature_maps][number_of_output_feature_maps]
            [1] - bias handle
                    bias nn_data_t dimensions: [number_of_output_feature_maps]
        */
        nn_primitives_convolution_pooling_f32_create_0,
        nn_primitives_convolution_pooling_i16_create_0,

        /* fully connected

        nn_data_t input format: NXYZ or NX
        nn_data_t output format: NX

        parameters:
            [0] - weights handle
                    weights nn_data_t dimensions:
                        [number_of_input_feature_maps][number_of_output_feature_maps]
                        or
                        [input_width][input_height][number_of_input_feature_maps][number_of_output_feature_maps]
            [1] - bias handle
                    bias nn_data_t dimensions: [number_of_output_feature_maps]
        */
        nn_primitives_fully_connected_f32_create_0,
        nn_primitives_fully_connected_i16_create_0,
        nn_primitives_fully_connected_i16_i32_create_0,

        /* convert layout from zxyn to nz
            use between convolution_f32 and fully_connected_f32

        nn_data_t input format: ZXYN
        nn_data_t output format: NX
        */
        nn_primitives_convert_zxyn_nx_f32_create_0,

        /* convert layout from z_block_xyz to z2nz
        use between convolution_i16 and fully_connected_i16

        nn_data_t input format: ZXYN
        nn_data_t output format: NX
        */
        nn_primitives_convert_z_block_xyz_x2nx_i16_create_0,

        /* convert layout from z2zn to nx
        use between convolution_i16 and fully_connected_i16

        nn_data_t input format: ZXYN
        nn_data_t output format: NX
        */
        nn_primitives_convert_z2nz_n8xn_create_0,

        nn_primitives_convert_from_zxyn_to_batch_block_format_nzxyn_create,
        nn_primitives_convert_from_batch_block_format_to_zxyn_create,

        /* arithmetic operation

        nn_data_t input/output format: ZXYN

        parameters:
            [0] - factor handle
                    factor nn_data_t dimensions: [input_width][input_height][number_of_input_feature_maps]
        */
        nn_primitives_arithmetic_f32_create_0,

        /* element wise linear normalization

        out = alpha * in + beta

        nn_data_t input/output format: ZXYN
        */
        nn_primitives_normalization_elementwise_linear_f32_create_0,

        /* response normalization across maps

        nn_data_t input/output format: ZXYN
        */
        nn_primitives_normalization_response_across_maps_f32_create_0,

        /* response normalization across maps

        nn_data_t input/output format: ZXYN
        */
        nn_primitives_normalization_response_across_maps_i16_create_0,

        /* softmax

        nn_data_t input/output format: NX
        */
        nn_primitives_softmax_f32_create_0,

        /* softmax

        nn_data_t input/output format: NX
        */
        nn_primitives_softmax_i32_create_0,

        /* convert float to int16

        nn_data_t input/output format: ZXYN
        */
        nn_primitives_convert_float_to_i16_create_0,

        /* rectified linear unit activation

        nn_data_t input/output format: ZXYN
        */
        nn_primitives_relu_f32_create_0,

        /* loss function
        */
        nn_primitives_loss_f32_create_0,

        /* dropout
        */
        nn_primitives_dropout_f32_create_0
    }
};
