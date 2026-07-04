#include "ff.h"

int encoder_gbk_to_utf8(const char *gbk, char *utf8, int max_len)
{
    int i = 0, j = 0;

    while (gbk[i] != '\0' && j < max_len - 4)
    {
        unsigned char ch = (unsigned char)gbk[i];

        if (ch < 0x80)
        {
            // ASCII 字符
            utf8[j++] = (char)ch;
            i++;
        }
        else
        {
            // 使用 FatFs 内置的 ff_convert 函数
            // 参数1: GBK 双字节字符（高字节<<8 | 低字节）
            // 参数2: 转换方向（1 = OEM -> Unicode）
            uint32_t unicode = ff_convert(
                ((uint32_t)(unsigned char)gbk[i] << 8) | (unsigned char)gbk[i + 1],
                1 // 1 表示 GBK → Unicode
            );

            // Unicode → UTF-8
            if (unicode)
            {
                if (unicode <= 0x7F)
                {
                    utf8[j++] = (char)unicode;
                }
                else if (unicode <= 0x7FF)
                {
                    utf8[j++] = 0xC0 | ((unicode >> 6) & 0x1F);
                    utf8[j++] = 0x80 | (unicode & 0x3F);
                }
                else if (unicode <= 0xFFFF)
                {
                    utf8[j++] = 0xE0 | ((unicode >> 12) & 0x0F);
                    utf8[j++] = 0x80 | ((unicode >> 6) & 0x3F);
                    utf8[j++] = 0x80 | (unicode & 0x3F);
                }
            }
            else
            {
                utf8[j++] = '?'; // 无法转换
            }
            i += 2;
        }
    }
    utf8[j] = '\0';
    return j;
}

int encoder_utf8_to_gbk(const char *utf8, char *gbk, int max_len)
{
    int i = 0, j = 0;
    while (utf8[i] != '\0' && j < max_len - 2)
    {
        unsigned char ch = (unsigned char)utf8[i];
        if (ch < 0x80)
        {
            // ASCII 字符，直接复制
            gbk[j++] = (char)ch;
            i++;
        }
        else if ((ch & 0xE0) == 0xC0)
        {
            // 2 字节 UTF-8: 0xC0 0x80 - 0xDF 0xBF (U+0080 - U+07FF)
            uint32_t unicode = ((ch & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
            // 使用 FatFs 的 ff_convert（0 = Unicode -> OEM/GBK）
            uint32_t gbk_code = ff_convert(unicode, 0);
            if (gbk_code)
            {
                gbk[j++] = (gbk_code >> 8) & 0xFF;
                gbk[j++] = gbk_code & 0xFF;
            }
            else
            {
                gbk[j++] = '?'; // 无法转换
            }
            i += 2;
        }
        else if ((ch & 0xF0) == 0xE0)
        {
            // 3 字节 UTF-8: 0xE0 0x80 0x80 - 0xEF 0xBF 0xBF (U+0800 - U+FFFF)
            uint32_t unicode = ((ch & 0x0F) << 12) |
                               ((utf8[i + 1] & 0x3F) << 6) |
                               (utf8[i + 2] & 0x3F);
            // 使用 FatFs 的 ff_convert（0 = Unicode -> OEM/GBK）
            uint32_t gbk_code = ff_convert(unicode, 0);
            if (gbk_code)
            {
                gbk[j++] = (gbk_code >> 8) & 0xFF;
                gbk[j++] = gbk_code & 0xFF;
            }
            else
            {
                gbk[j++] = '?'; // 无法转换
            }
            i += 3;
        }
        else
        {
            // 无效或不支持的 UTF-8 序列
            gbk[j++] = '?';
            i++;
        }
    }
    gbk[j] = '\0';
    return j;
}
