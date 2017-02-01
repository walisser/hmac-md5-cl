#ifndef __CLUTIL_H__
#define __CLUTIL_H__

#include <CL/cl.h>

class CLUtil {
public:
    static const char* errorString(cl_int error);
};

#endif

