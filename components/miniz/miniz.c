#include <miniz.h>

int tinfl_decompress_mem_to_callback1(const void *pIn_buf, size_t *pIn_buf_size, tinfl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags)
{
    int result = 0;
    tinfl_decompressor decomp;
    mz_uint8 *pDict = (mz_uint8 *)MZ_MALLOC(TINFL_LZ_DICT_SIZE);
    size_t in_buf_ofs = 0, dict_ofs = 0;
    if (!pDict)
        return TINFL_STATUS_FAILED;
    memset(pDict,0,TINFL_LZ_DICT_SIZE);
    tinfl_init(&decomp);
    for (;;)
    {
        size_t in_buf_size = *pIn_buf_size - in_buf_ofs, dst_buf_size = TINFL_LZ_DICT_SIZE - dict_ofs;
        tinfl_status status = tinfl_decompress(&decomp, (const mz_uint8 *)pIn_buf + in_buf_ofs, &in_buf_size, pDict, pDict + dict_ofs, &dst_buf_size,
                                               (flags & ~(TINFL_FLAG_HAS_MORE_INPUT | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)));
        in_buf_ofs += in_buf_size;
        if ((dst_buf_size) && (!(*pPut_buf_func)(pDict + dict_ofs, (int)dst_buf_size, pPut_buf_user)))
            break;
        if (status != TINFL_STATUS_HAS_MORE_OUTPUT)
        {
            result = (status == TINFL_STATUS_DONE);
            break;
        }
        dict_ofs = (dict_ofs + dst_buf_size) & (TINFL_LZ_DICT_SIZE - 1);
    }
    MZ_FREE(pDict);
    *pIn_buf_size = in_buf_ofs;
    return result;
}
