
#ifndef __CLSIMPLE_H__
#define __CLSIMPLE_H__

#include <CL/cl.h>

class CLSimple
{
public:
    cl_device_id     device_id;
    cl_context       context;
    cl_command_queue command_queue;
    cl_mem           out_buffer;
    cl_kernel        kernel;

    size_t maxWorkGroupSize;
    cl_ulong maxBufferSize;
    cl_ulong maxLocalMem;
    cl_ulong maxConstantMem;
    cl_ulong kernelGlobalWorkSize;
    cl_ulong kernelWorkGroupSize;
    cl_ulong kernelLocalMem;
    cl_ulong kernelPrivateMem;

    bool isNvidia=false;

private:
    static void initFirstPlatform(cl_device_type deviceTypes, cl_device_id * device_id, cl_uint maxDevices=1);
    static void createContext(cl_context * context, cl_device_id device_id);
    
public:
    CLSimple();
    CLSimple(const char * kernel_name, int deviceIndex=0);
    virtual ~CLSimple();

    void createCommandQueue(cl_command_queue * command_queue);
    
    void kernelSetArg(cl_kernel kernel, cl_uint index, size_t size,
                             const void * value);
    
    void createBuffer(cl_mem * buffer, cl_mem_flags flags, size_t size, void* hostPtr=nullptr);
    
    void enqueueWriteBuffer(cl_command_queue command_queue,
          cl_mem buffer, size_t buffer_size, void * ptr);
    
    void enqueueNDRangeKernel(cl_command_queue command_queue,
          cl_kernel kernel, cl_uint work_dim, const size_t * global_work_size,
          const size_t * local_work_size, bool wait=true);

    void createKernel(cl_kernel * kernel, const char * kernel_name);
    
    void createKernelString(cl_kernel * kernel, const char * kernel_name,
                                        const char * code, size_t code_len);
       
    void setOutputBuffer(size_t buffer_size);
    
    void enqueueReadBuffer(cl_command_queue command_queue,
              cl_mem buffer, size_t buffer_size, void * ptr, bool wait=CL_TRUE);

    void readOutput(void * data, size_t data_bytes);
};

#endif

