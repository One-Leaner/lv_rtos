/*
 * Copyright PeakRacing
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "nes.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "lcd.h"

volatile uint8_t nes_frame_ok = 0;

void *lv_malloc(size_t size);
void lv_free(void *data);

/* memory */
void *nes_malloc(int num)
{
    void *ptr = lv_malloc(num);
    if (!ptr)
    {
        printf("nes_malloc malloc failed\n");
    }
    return ptr;
}

void nes_free(void *address)
{
    if (!address)
    {
        printf("nes_free free NULL\n");
        return;
    }
    lv_free(address);
}

void *nes_memcpy(void *str1, const void *str2, size_t n)
{
    return memcpy(str1, str2, n);
}

void *nes_memset(void *str, int c, size_t n)
{
    return memset(str, c, n);
}

int nes_memcmp(const void *str1, const void *str2, size_t n)
{
    return memcmp(str1, str2, n);
}

#if (NES_USE_FS == 1)
/* io */
FILE *nes_fopen(const char *filename, const char *mode)
{
    FIL *fp = ff_malloc(sizeof(FIL));
    if (!fp)
    {
        printf("fopen malloc failed\n");
        return NULL;
    }

    BYTE ff_mode = 0;
    if (strchr(mode, 'r'))
        ff_mode |= FA_READ;
    if (strchr(mode, 'w'))
        ff_mode |= FA_WRITE | FA_CREATE_ALWAYS;
    if (strchr(mode, 'a'))
        ff_mode |= FA_OPEN_APPEND | FA_WRITE;
    if (strchr(mode, '+'))
        ff_mode |= FA_READ | FA_WRITE;

    FRESULT res = f_open(fp, filename, ff_mode);
    if (res != FR_OK)
    {
        ff_free(fp);
        printf("fopen open failed\n");
        return NULL;
    }

    return (FILE *)fp;
}

size_t nes_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    UINT bytes_read;
    FRESULT res = f_read((FIL *)stream, ptr, size * nmemb, &bytes_read);
    if (res != FR_OK)
    {
        printf("fread read failed\n");
        return 0;
    }

    return bytes_read / size;
}

size_t nes_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    UINT bytes_written;
    FRESULT res = f_write((FIL *)stream, ptr, size * nmemb, &bytes_written);
    if (res != FR_OK)
    {
        printf("fwrite write failed\n");
        return 0;
    }

    return bytes_written / size;
}

int nes_fseek(FILE *stream, long int offset, int whence)
{
    DWORD new_pos;

    switch (whence)
    {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = f_tell((FIL *)stream) + offset;
        break;
    case SEEK_END:
        new_pos = f_size((FIL *)stream) + offset;
        break;
    default:
        return -1;
    }

    FRESULT res = f_lseek((FIL *)stream, new_pos);

    return (res == FR_OK) ? 0 : -1;
}

int nes_fclose(FILE *stream)
{
    if (!stream)
    {
        printf("fclose close NULL\n");
        return -1;
    }
    FRESULT res = f_close((FIL *)stream);
    ff_free(stream);

    return (res == FR_OK) ? 0 : -1;
}
#endif

#if (NES_ENABLE_SOUND == 1)

int nes_sound_output(uint8_t *buffer, size_t len)
{
    return 0;
}
#endif

int nes_initex(nes_t *nes)
{
    (void)nes;
    return 0;
}

int nes_deinitex(nes_t *nes)
{
    (void)nes;
    return 0;
}

int nes_draw(int x1, int y1, int x2, int y2, nes_color_t *color_data)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color_data;
    return 0;
}

#define FRAMES_PER_SECOND 1000 / 60

void nes_frame(nes_t *nes)
{
    (void)nes;
    nes_frame_ok = 1;
    vTaskDelay(8);
}
