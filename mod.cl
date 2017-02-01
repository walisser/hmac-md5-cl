
__kernel void mod(__global int * out, int arg1, int arg2, __global const int* in)
{
  uint gid = get_global_id(0);
  out[gid] = in[gid] + gid;//(gid * arg1) % arg2;
}
