/*
 * Developed by Claudio André <claudio.andre at correios.net.br> in 2012
 *
 * Copyright (c) 2012 Claudio André <claudio.andre at correios.net.br>
 * This program comes with ABSOLUTELY NO WARRANTY; express or implied.
 *
 * This is free software, and you are welcome to redistribute it
 * under certain conditions; as expressed here
 * http://www.gnu.org/licenses/gpl-2.0.html
 */

#ifndef OPENCL_DEVICE_INFO_H
#define	OPENCL_DEVICE_INFO_H

//Copied from common-opencl.h
#define DEV_UNKNOWN                 0		//0
#define DEV_CPU                     (1 << 0)	//1
#define DEV_GPU                     (1 << 1)	//2
#define DEV_ACCELERATOR             (1 << 2)	//4
#define DEV_AMD                     (1 << 3)	//8
#define DEV_NVIDIA                  (1 << 4)	//16
#define DEV_INTEL                   (1 << 5)	//32
#define PLATFORM_APPLE              (1 << 6)	//64
#define DEV_AMD_GCN                 (1 << 7)	//128
#define DEV_AMD_VLIW4               (1 << 8)	//256
#define DEV_AMD_VLIW5               (1 << 9)	//512
#define DEV_NV_C2X		    (1 << 10)	//1024
#define DEV_NV_C30		    (1 << 11)	//2048
#define DEV_NV_C35		    (1 << 12)	//4096
#define DEV_NV_C5X		    (1 << 13)	//8192
#define DEV_USE_LOCAL               (1 << 16)	//65536
#define DEV_NO_BYTE_ADDRESSABLE     (1 << 17)	//131072

#define cpu(n)                      ((n & DEV_CPU) == (DEV_CPU))
#define gpu(n)                      ((n & DEV_GPU) == (DEV_GPU))
#define gpu_amd(n)                  ((n & DEV_AMD) && gpu(n))
#define gpu_nvidia(n)               ((n & DEV_NVIDIA) && gpu(n))
#define gpu_intel(n)                ((n & DEV_INTEL) && gpu(n))
#define cpu_amd(n)                  ((n & DEV_AMD) && cpu(n))
#define cpu_intel(n)                ((n & DEV_INTEL) && cpu(n))
#define amd_gcn(n)                  ((n & DEV_AMD_GCN) && gpu_amd(n))
#define amd_vliw4(n)                ((n & DEV_AMD_VLIW4) && gpu_amd(n))
#define amd_vliw5(n)                ((n & DEV_AMD_VLIW5) && gpu_amd(n))
#define nvidia_sm_2x(n)             ((n & DEV_NV_C2X) && gpu_nvidia(n))
#define nvidia_sm_3x(n)             (((n & DEV_NV_C30) || (n & DEV_NV_C35)) && gpu_nvidia(n))
#define nvidia_sm_5x(n)             ((n & DEV_NV_C5X) && gpu_nvidia(n))
#define no_byte_addressable(n)      ((n & DEV_NO_BYTE_ADDRESSABLE))
#define use_local(n)                ((n & DEV_USE_LOCAL))
#define platform_apple(p)           (get_platform_vendor_id(p) == PLATFORM_APPLE)

#endif	/* OPENCL_DEVICE_INFO_H */
