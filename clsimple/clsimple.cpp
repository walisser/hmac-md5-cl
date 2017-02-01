

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <cassert>
#include <unistd.h> // getcwd
#include <inttypes.h>

#include <QtCore/QtCore>

#include "clsimple.h"
#include "clutil.h"

/*============================================================================*/
/*========== DIRECT WRAPPERS =================================================*/
/*============================================================================*/


CLSimple::CLSimple(const char * kernel_name, int deviceIndex)
{
    int numDevices = deviceIndex+1;
    cl_device_id devices[numDevices] = {nullptr};
    
   initGpuDevice(devices, numDevices);
   assert(devices[deviceIndex] != nullptr);
   this->device_id = devices[deviceIndex];
     
   createContext(&this->context, this->device_id);
   createCommandQueue(&this->command_queue);
   createKernel(&this->kernel, kernel_name);
   
   clGetDeviceInfo(this->device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, NULL);
   clGetDeviceInfo(this->device_id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(maxBufferSize), &maxBufferSize, NULL);
   clGetDeviceInfo(this->device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(maxLocalMem), &maxLocalMem, NULL);
   
   printf("max workgroup size=%" PRIi64 "\n", maxWorkGroupSize);
   printf("max local mem     =%" PRIi64 "\n", maxLocalMem);
   printf("max buffer size   =%" PRIi64 "\n", maxBufferSize);

   maxBufferSize = 256*1024*1024;
}

CLSimple::CLSimple()
{
}

CLSimple::~CLSimple()
{
}

void CLSimple::createContext(cl_context * context, cl_device_id device_id)
{
   cl_int error;
   *context = clCreateContext(NULL, /* Properties */
                           1, /* Number of devices */
                           &device_id, /* Device pointer */
                           NULL, /* Callback for reporting errors */
                           NULL, /* User data to pass to error callback */
                           &error); /* Error code */

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clCreateContext() failed: %s\n",
                      CLUtil::errorString(error));
      assert(0);
   }
}

void CLSimple::createCommandQueue(cl_command_queue * command_queue)
{
   cl_int error;
   *command_queue = clCreateCommandQueue(this->context,
                                        this->device_id,
                                        0, /* Command queue properties */
                                        &error); /* Error code */

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clCreateCommandQueue() failed: %s\n",
                      CLUtil::errorString(error));
      assert(0);
   }
}

void CLSimple::kernelSetArg(cl_kernel kernel, cl_uint index, size_t size,
                         const void * value)
{
   cl_int error;

   error = clSetKernelArg(kernel, index, size, value);

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clSetKernelArg failed: %s\n", CLUtil::errorString(error));
      assert(0);
   }

   //fprintf(stderr, "clSetKernelArg() succeeded.\n");
}

void CLSimple::createBuffer(cl_mem * buffer, cl_mem_flags flags, size_t size, void* hostPtr)
{
   cl_int error;

    if (hostPtr)
        flags |= CL_MEM_USE_HOST_PTR;
        
   *buffer = clCreateBuffer(this->context,
                            flags, /* Flags */
                            size, /* Size of buffer */
                            hostPtr, /* Pointer to the data */
                            &error); /* error code */

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clCreateBuffer(%p, %p, %lu, %lu) failed: %s\n",
                       buffer, this->context, flags, size,
                       CLUtil::errorString(error));
      assert(0);
   }
}

void CLSimple::enqueueWriteBuffer(cl_command_queue command_queue,
      cl_mem buffer, size_t buffer_size, void * ptr)
{

   cl_int error = clEnqueueWriteBuffer(command_queue,
                                       buffer,
                                       CL_TRUE, /* Blocking write */
                                       0, /* Offset into buffer */
                                       buffer_size,
                                       ptr,
                                       0, /* Events in waiting list */
                                       NULL, /* Wait list */
                                       NULL); /* Event */
   assert(error == CL_SUCCESS);
}

void CLSimple::enqueueNDRangeKernel(cl_command_queue command_queue,
      cl_kernel kernel, cl_uint work_dim, const size_t * global_work_size,
      const size_t * local_work_size, bool wait)
{
   cl_int error = clEnqueueNDRangeKernel(command_queue,
                                  kernel,
                                  work_dim, /* Number of dimensions */
                                  NULL, /* Global work offset */
                                  global_work_size,
                                  local_work_size, /* local work size */
                                  0, /* Events in wait list */
                                  NULL, /* Wait list */
                                  NULL); /* Event object for this event */
   if (error != CL_SUCCESS) {
      fprintf(stderr, "clEnqueueNDRangeKernel() failed: %s\n",
                      CLUtil::errorString(error));
      assert(0);
   }

   if (wait)
   {
       error = clFinish(command_queue);

       if (error != CL_SUCCESS) {
          fprintf(stderr, "clFinish() failed: %s\n", CLUtil::errorString(error));
          assert(0);
       }
   }
}

/*============================================================================*/
/*========== CONVENIENCE WRAPPERS=============================================*/
/*============================================================================*/

void CLSimple::initGpuDevice(cl_device_id * device_ids, cl_uint maxDevices)
{
   cl_int error;

   cl_uint total_platforms;
   cl_platform_id platform_id;

   cl_uint total_gpu_devices;

   error = clGetPlatformIDs(
                           1, /* Max number of platform IDs to return */
                           &platform_id, /* Pointer to platform_id */
                           &total_platforms); /* Total number of platforms
                                               * found on the system */

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clGetPlatformIDs() failed: %s\n", CLUtil::errorString(error));
      assert(0);
   }

   fprintf(stderr, "There are %u platforms.\n", total_platforms);

   error = clGetDeviceIDs(platform_id,
                          CL_DEVICE_TYPE_GPU,
                          maxDevices,
                          device_ids,
                          &total_gpu_devices);

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clGetDeviceIDs() failed: %s\n", CLUtil::errorString(error));
      assert(0);
   }
   
   fprintf(stderr, "There are %u GPU devices.\n", total_gpu_devices);
}

void CLSimple::createKernel(cl_kernel * kernel, const char * kernel_name)
{
    QFile f(QString(kernel_name) + ".cl");
    f.open(QFile::ReadOnly);
    QByteArray code = f.readAll();   
    createKernelString(kernel, kernel_name, code.data(), code.length());
}

void CLSimple::createKernelString(cl_kernel * kernel, const char * kernel_name,
                                    const char * code, size_t code_len)
{
   cl_int error;
   cl_program program;

   /* Create program */
   program = clCreateProgramWithSource(context,
                                       1, /* Number of strings */
                                       &code,
                                       &code_len, /* String lengths, 0 means all the
                                              * strings are NULL terminated. */
                                       &error);

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clCreateProgramWithSource() failed: %s\n",
                      CLUtil::errorString(error));
      assert(0);
   }

   fprintf(stderr, "clCreateProgramWithSource() suceeded.\n");

   /* Build program */

   //char cwd[1024];
   //assert(cwd == getcwd(cwd, sizeof(cwd)-1));

   //QString options=QString("-cl-fast-relaxed-math -cl-mad-enable -I %1")
   //    .arg(cwd);

   const char* options = "-I /home/ghaxt/src/hash";

   error = clBuildProgram(program,
                          1, /* Number of devices */
                          &this->device_id,
                          options, /* options */
                          NULL, /* callback function when compile is complete */
                          NULL); /* user data for callback */


   if (error != CL_SUCCESS) {
      char build_str[10000];
      error = clGetProgramBuildInfo(program,
                                    this->device_id,
                                    CL_PROGRAM_BUILD_LOG,
                                    10000, /* Size of output string */
                                    build_str, /* pointer to write the log to */
                                    NULL); /* Number of bytes written to the log */
      if (error != CL_SUCCESS) {
         fprintf(stderr, "clGetProgramBuildInfo() failed: %s\n",
                          CLUtil::errorString(error));
      } else {
         fprintf(stderr, "Build Log: \n%s\n\n", build_str);
      }
      assert(0);
   }

   fprintf(stderr, "clBuildProgram() suceeded.\n");

   *kernel = clCreateKernel(program, kernel_name, &error);

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clCreateKernel() failed: %s\n", CLUtil::errorString(error));
      assert(0);
   }

   //fprintf(stderr, "clCreateKernel() suceeded.\n");
}

void CLSimple::setOutputBuffer(unsigned buffer_size)
{
    createBuffer(&this->out_buffer, CL_MEM_WRITE_ONLY, buffer_size);
                             
    kernelSetArg(this->kernel, 0, sizeof(cl_mem), &this->out_buffer);
}

void CLSimple::readOutput(void * data, size_t data_bytes)
{
   cl_int error = clEnqueueReadBuffer(this->command_queue,
                                      this->out_buffer,
                                      CL_TRUE, /* TRUE means it is a blocking read. */
                                      0, /* Buffer offset to read from. */
                                      data_bytes, /* Bytes to read */
                                      data, /* Pointer to store the data */
                                      0, /* Events in wait list */
                                      NULL, /* Wait list */
                                      NULL); /* Event object */

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clEnqueueReadBuffer() failed: %s\n",
                      CLUtil::errorString(error));
      assert(0);
   }
}

void CLSimple::enqueueReadBuffer(cl_command_queue command_queue,
              cl_mem buffer, size_t buffer_size, void * ptr, bool wait)
{
    cl_int error = clEnqueueReadBuffer(command_queue,
                                          buffer,
                                          wait, /* TRUE means it is a blocking read. */
                                          0, /* Buffer offset to read from. */
                                          buffer_size, /* Bytes to read */
                                          ptr, /* Pointer to store the data */
                                          0, /* Events in wait list */
                                          NULL, /* Wait list */
                                          NULL); /* Event object */

   if (error != CL_SUCCESS) {
      fprintf(stderr, "clEnqueueReadBuffer() failed: %s\n",
                      CLUtil::errorString(error));
      assert(0);
   }
}

