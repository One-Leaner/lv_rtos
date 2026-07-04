#include "lv_port_fs.h"
#include "lvgl.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>

static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br);
static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw);
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);

static void *fs_dir_open(lv_fs_drv_t *drv, const char *path);
static lv_fs_res_t fs_dir_read(lv_fs_drv_t *drv, void *rddir_p, char *fn, uint32_t fn_len);
static lv_fs_res_t fs_dir_close(lv_fs_drv_t *drv, void *rddir_p);

void lv_port_fs_init(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    /*Set up fields...*/
    fs_drv.letter = 'S';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;

    fs_drv.dir_close_cb = fs_dir_close;
    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;

    lv_fs_drv_register(&fs_drv);
}

/**
 * Open a file
 * @param drv       pointer to a driver where this function belongs
 * @param path      path to the file beginning with the driver letter (e.g. S:/folder/file.txt)
 * @param mode      read: FS_MODE_RD, write: FS_MODE_WR, both: FS_MODE_RD | FS_MODE_WR
 * @return          a file descriptor or NULL on error
 */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);
    printf("fopen\n");
    // lv_fs_res_t res = LV_FS_RES_NOT_IMP;
    BYTE fatfs_mode = 0;
    /*把LVGL的打开文件的模式和FatFs一一对应好*/
    if (mode == LV_FS_MODE_WR)
        fatfs_mode = FA_WRITE | FA_OPEN_ALWAYS;
    else if (mode == LV_FS_MODE_RD)
        fatfs_mode = FA_READ;
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD))
        fatfs_mode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;
    else
        return NULL;
    /* LVGL 传进来的path已经没有盘符，比如 "test.txt" ,我们需要把盘符补充上去，然后才能给f_open使用*/
    char *full_path = lv_malloc(256);
    if (full_path == NULL)
        return NULL;

    lv_snprintf(full_path, 256, "0:%s", path); // 注意 path 已经是 "/xxx"
    printf("fs_open full_path=%s\n", full_path);

    FIL *f = lv_malloc(sizeof(FIL)); // 为每个文件分配空间
    if (f == NULL)
    {
        lv_free(full_path);
        printf("lv_malloc failed ! (f == NULL)\r\n");
        return NULL;
    }
    FRESULT fr = f_open(f, full_path, fatfs_mode);
    if (fr == FR_OK)
    {
        printf("fs_open full_path=%s  OK !\n", full_path);
        return f;
    }
    else
    {
        printf("f_open failed: %d, full_path=%s\n", fr, full_path);
        lv_free(f);
        return NULL;
    }
}

/**
 * Close an opened file
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable. (opened with fs_open)
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    LV_UNUSED(drv);
    if (FR_OK == f_close((FIL *)file_p))
    {
        lv_free(file_p);
        return LV_FS_RES_OK;
    }
    else
    {
        lv_free(file_p);
        return LV_FS_RES_UNKNOWN;
    }
}

/**
 * Read data from an opened file
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable.
 * @param buf       pointer to a memory block where to store the read data
 * @param btr       number of Bytes To Read
 * @param br        the real number of read bytes (Byte Read)
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    /*Add your code here*/
    LV_UNUSED(drv);
    FRESULT res = f_read(file_p, buf, btr, (UINT *)br);
    if (res == FR_OK)
        return LV_FS_RES_OK;
    else
        return LV_FS_RES_UNKNOWN;
}

/**
 * Write into a file
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable
 * @param buf       pointer to a buffer with the bytes to write
 * @param btw       Bytes To Write
 * @param bw        the number of real written bytes (Bytes Written). NULL if unused.
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    /*Add your code here*/
    LV_UNUSED(drv);
    FRESULT res = f_write(file_p, buf, btw, (UINT *)bw);
    if (res == FR_OK)
        return LV_FS_RES_OK;
    else
        return LV_FS_RES_UNKNOWN;
}

/**
 * Set the read write pointer. Also expand the file size if necessary.
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable. (opened with fs_open )
 * @param pos       the new position of read write pointer
 * @param whence    tells from where to interpret the `pos`. See @lv_fs_whence_t
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    /*Add your code here*/
    LV_UNUSED(drv);
    switch (whence)
    {
    case LV_FS_SEEK_SET:
        f_lseek(file_p, pos);
        break;
    case LV_FS_SEEK_CUR:
        f_lseek(file_p, f_tell((FIL *)file_p) + pos);
        break;
    case LV_FS_SEEK_END:
        f_lseek(file_p, f_size((FIL *)file_p) + pos);
        break;
    default:
        break;
    }
    return LV_FS_RES_OK;
}
/**
 * Give the position of the read write pointer
 * @param drv       pointer to a driver where this function belongs
 * @param file_p    pointer to a file_t variable
 * @param pos_p     pointer to store the result
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    /*Add your code here*/
    LV_UNUSED(drv);
    *pos_p = f_tell((FIL *)file_p);
    return LV_FS_RES_OK;
}

/**
 * Initialize a 'lv_fs_dir_t' variable for directory reading
 * @param drv       pointer to a driver where this function belongs
 * @param path      path to a directory
 * @return          pointer to the directory read descriptor or NULL on error
 */
static void *fs_dir_open(lv_fs_drv_t *drv, const char *path)
{
    LV_UNUSED(drv);

    DIR *d = lv_malloc(sizeof(DIR));
    if (d == NULL)
        return NULL;

    /* SD卡挂载在 "0:"，与 fs_open 保持一致 */
    char *real_path = lv_malloc(256);
    if (real_path == NULL)
    {
        lv_free(d); /* 修复内存泄漏 */
        return NULL;
    }

    lv_snprintf(real_path, 256, "0:%s", path);
    FRESULT res = f_opendir(d, real_path);
    lv_free(real_path); /* 释放临时路径缓冲区 */

    if (res != FR_OK)
    {
        printf("fs_dir_open failed: %d, path=%s\n", res, path);
        lv_free(d);
        d = NULL;
    }
    return d;
}

/**
 * Read the next filename form a directory.
 * The name of the directories will begin with '/'
 * @param drv       pointer to a driver where this function belongs
 * @param rddir_p   pointer to an initialized 'lv_fs_dir_t' variable
 * @param fn        pointer to a buffer to store the filename
 * @param fn_len    length of the buffer to store the filename
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_dir_read(lv_fs_drv_t *drv, void *rddir_p, char *fn, uint32_t fn_len)
{
    LV_UNUSED(drv);

    if (fn_len == 0) /* 增加参数检查 */
        return LV_FS_RES_INV_PARAM;

    FRESULT res;
    FILINFO fno;
    fn[0] = '\0';

    do
    {
        res = f_readdir(rddir_p, &fno);
        if (res != FR_OK)
            return LV_FS_RES_UNKNOWN;

        if (fno.fname[0] == 0) /* 目录结束 */
            break;

        if (fno.fattrib & AM_DIR)
        {
            lv_snprintf(fn, fn_len, "/%s", fno.fname); /* 使用 lv_snprintf 防止缓冲区溢出 */
        }
        else
        {
            lv_strlcpy(fn, fno.fname, fn_len); /* 使用 lv_strlcpy 防止缓冲区溢出 */
        }
    } while (strcmp(fn, "/.") == 0 || strcmp(fn, "/..") == 0);

    return LV_FS_RES_OK;
}

/**
 * Close the directory reading
 * @param drv       pointer to a driver where this function belongs
 * @param rddir_p   pointer to an initialized 'lv_fs_dir_t' variable
 * @return          LV_FS_RES_OK: no error or  any error from @lv_fs_res_t enum
 */
static lv_fs_res_t fs_dir_close(lv_fs_drv_t *drv, void *rddir_p)
{
    LV_UNUSED(drv);
    if (FR_OK == f_closedir(rddir_p))
    {
        lv_free(rddir_p);
        return LV_FS_RES_OK;
    }
    else
    {
        lv_free(rddir_p);
        return LV_FS_RES_UNKNOWN;
    }
}