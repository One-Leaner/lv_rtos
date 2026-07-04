#include "ff.h"
#include "fs.h"
#include "w25qxx.h"
#include "bsp_driver_sd.h"
#include "heap.h"
#include "encoder.h"
#include <stdio.h>
#include <string.h>

// #define MAX_PATH_LEN 64 // 最大路径长度
// #define MAX_NAME_LEN 64 // 最大文件名长度
// #define MAX_CHILDREN 32 // 最大子项数量

// 文件/目录信息结构体
struct FS_NODE
{
    char *name;                // 文件名或目录名
    char *full_path;           // 完整路径
    uint32_t size;             // 文件大小（目录为0）
    FS_NODE_TYPE_t type;       // 节点类型
    struct FS_NODE *parent;    // 父目录节点
    struct FS_NODE **children; // 子项节点数组
    uint32_t child_count;      // 子项数量
};

// 文件系统树结构体
struct FS_TREE
{
    FS_NODE_t *root;         // 根节点
    FS_NODE_t *current_node; // 当前节点
    uint32_t total_files;    // 总文件数
    uint32_t total_dirs;     // 总目录数
};

static FS_NODE_t *fs_node_create(FS_NODE_t *parent, const char *name, FS_NODE_TYPE_t type, uint32_t size)
{
    FS_NODE_t *node = (FS_NODE_t *)heap.api->malloc(heap.ctrl, sizeof(FS_NODE_t));
    if (node == NULL)
    {
        printf("Failed to allocate memory for node\n");
        return NULL;
    }

    // strncpy(node->name, name, MAX_NAME_LEN);
    // node->name[MAX_NAME_LEN] = '\0';

    // 转换为UTF-8编码
    static char utf8_name[256];
    encoder_gbk_to_utf8(name, utf8_name, sizeof(utf8_name));

    uint16_t name_len = strlen(utf8_name) + 1;
    node->name = heap.api->malloc(heap.ctrl, name_len);
    if (node->name == NULL)
    {
        printf("Failed to allocate memory for node name\n");
        heap.api->free(heap.ctrl, node);
        return NULL;
    }
    strcpy(node->name, utf8_name);

    uint16_t path_len = 0;
    if (parent == NULL)
    {
        path_len = name_len;
        node->full_path = heap.api->malloc(heap.ctrl, path_len);
        if (node->full_path == NULL)
        {
            printf("Failed to allocate memory for node full path\n");
            heap.api->free(heap.ctrl, node);
            heap.api->free(heap.ctrl, node->name);
            return NULL;
        }
        snprintf(node->full_path, path_len, "%s", name);
    }
    else
    {
        path_len = strlen(parent->full_path) + 1 + name_len;
        node->full_path = heap.api->malloc(heap.ctrl, path_len);
        if (node->full_path == NULL)
        {
            printf("Failed to allocate memory for node full path\n");
            heap.api->free(heap.ctrl, node);
            heap.api->free(heap.ctrl, node->name);
            return NULL;
        }
        snprintf(node->full_path, path_len, "%s/%s", parent->full_path, name);
    }

    // node->full_path[MAX_PATH_LEN - 1] = '\0';

    node->size = size;
    node->type = type;
    node->parent = parent;
    node->children = NULL;
    node->child_count = 0;

    return node;
}

// 销毁文件节点
static void fs_node_destroy(FS_NODE_t *node)
{
    if (node == NULL)
        return;

    // 递归销毁子节点
    for (uint32_t i = 0; i < node->child_count; i++)
    {
        fs_node_destroy(node->children[i]);
    }

    heap.api->free(heap.ctrl, node->name);
    heap.api->free(heap.ctrl, node->full_path);
    heap.api->free(heap.ctrl, node->children);
    heap.api->free(heap.ctrl, node);
}

// 预处理
static uint8_t fs_node_reserve_children(FS_NODE_t *parent, uint32_t child_count)
{
    if (parent == NULL || parent->type == FS_NODE_FILE || child_count == 0)
        return 0;

    if (parent->children != NULL)
    {
        heap.api->free(heap.ctrl, parent->children);
        parent->children = NULL;
    }
    parent->children = (FS_NODE_t **)heap.api->malloc(heap.ctrl, child_count * sizeof(FS_NODE_t *));

    if (parent->children == NULL)
    {
        printf("Failed to allocate memory for children\n");
        return 0;
    }
    parent->child_count = 0; // 重置计数
    return 1;
}

// 添加子节点
static uint8_t fs_node_add_child(FS_NODE_t *parent, FS_NODE_t *child)
{
    if (parent == NULL || child == NULL || parent->type == FS_NODE_FILE)
    {
        printf("Invalid parent or child node\n");
        return 0;
    }

    // 扩展子节点数组
    // if (parent->child_count >= MAX_CHILDREN)
    // {
    //     printf("Max children reached\n");
    //     return 0;
    // }

    // 首次添加时分配数组
    // if (parent->children == NULL)
    // {
    //     parent->children = (FS_NODE_t **)heap.api->malloc(heap.ctrl, MAX_CHILDREN * sizeof(FS_NODE_t));
    //     if (parent->children == NULL)
    //     {
    //         printf("Failed to allocate memory for children\n");
    //         return 0;
    //     }
    // }

    parent->children[parent->child_count++] = child;
    child->parent = parent;

    return 1;
}

// 创建文件系统树
FS_TREE_t *fs_tree_create(const char *root_path)
{
    FS_TREE_t *tree = (FS_TREE_t *)heap.api->malloc(heap.ctrl, sizeof(FS_TREE_t));
    if (tree == NULL)
        return NULL;

    tree->root = fs_node_create(NULL, root_path, FS_NODE_DIR, 0);
    tree->current_node = tree->root;
    tree->total_files = 0;
    tree->total_dirs = 1;

    return tree;
}

// 销毁文件系统树
void fs_tree_destroy(FS_TREE_t *tree)
{
    if (tree == NULL)
        return;

    fs_node_destroy(tree->root);
    heap.api->free(heap.ctrl, tree);
}

// 递归扫描目录并构建树
void fs_tree_scan(FS_TREE_t *tree)
{
    if (tree == NULL || tree->current_node == NULL)
    {
        printf("Invalid tree or current node\n");
        return;
    }

    DIR dir;
    FILINFO fno;
    FRESULT res;
    FS_NODE_t *node = NULL;
    uint32_t child_count = 0;

    // 统计子项数量
    res = f_opendir(&dir, tree->current_node->full_path);
    if (res != FR_OK)
    {
        printf("Failed to open directory: %s\n", tree->current_node->full_path);
        return;
    }
    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;

        // 跳过特殊目录
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;
        if (fno.fattrib & (AM_HID | AM_SYS))
            continue;

        child_count++;
    }
    f_closedir(&dir);

    if (child_count == 0)
        return;
    // 预处理子节点数组
    if (!fs_node_reserve_children(tree->current_node, child_count))
    {
        printf("Failed to reserve children\n");
        return;
    }

    // 扫描目录
    res = f_opendir(&dir, tree->current_node->full_path);
    if (res != FR_OK)
    {
        printf("Failed to open directory: %s\n", tree->current_node->full_path);
        return;
    }
    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;

        // 跳过特殊目录
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;
        if (fno.fattrib & (AM_HID | AM_SYS))
            continue;

        if (fno.fattrib & AM_DIR)
        {
            node = fs_node_create(tree->current_node, fno.fname, FS_NODE_DIR, 0);
            tree->total_dirs++;
        }

        if (node != NULL)
        {
            fs_node_add_child(tree->current_node, node);
            tree->current_node = node;
            fs_tree_scan(tree); // 递归扫描子目录
            tree->current_node = node->parent;
        }
    }
    f_closedir(&dir);

    // 扫描文件
    res = f_opendir(&dir, tree->current_node->full_path);
    if (res != FR_OK)
    {
        printf("Failed to open directory: %s\n", tree->current_node->full_path);
        return;
    }
    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;

        // 跳过特殊目录
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;
        if (fno.fattrib & (AM_HID | AM_SYS))
            continue;
        if (fno.fattrib & AM_DIR)
            continue;

        node = fs_node_create(tree->current_node, fno.fname, FS_NODE_FILE, fno.fsize);
        tree->total_files++;
        if (node != NULL)
        {
            fs_node_add_child(tree->current_node, node);
        }
    }

    f_closedir(&dir);
}

FS_NODE_t *fs_tree_get_root_node(FS_TREE_t *tree)
{
    if (tree == NULL || tree->root == NULL)
        return NULL;

    return tree->root;
}

char *fs_node_get_full_path(FS_NODE_t *node)
{
    if (node == NULL)
        return NULL;

    return node->full_path;
}

char *fs_node_get_name(FS_NODE_t *node)
{
    if (node == NULL)
        return NULL;

    return node->name;
}

FS_NODE_t *fs_node_get_parent(FS_NODE_t *node)
{
    if (node == NULL)
        return NULL;

    return node->parent;
}

FS_NODE_t **fs_node_get_children(FS_NODE_t *node)
{
    if (node == NULL)
        return NULL;

    return node->children;
}

FS_NODE_TYPE_t fs_node_get_type(FS_NODE_t *node)
{
    if (node == NULL)
        return FS_NODE_FILE;

    return node->type;
}

uint32_t fs_node_get_child_count(FS_NODE_t *node)
{
    if (node == NULL)
        return 0;

    return node->child_count;
}

void fs_mount()
{
    if (BSP_SD_Init() == MSD_OK)
        printf("sd init success\n");
    else
        printf("sd init failed\n");

    static FATFS fs_sd;
    static uint8_t mount_buf[4096];
    FRESULT res;

    // 挂载SD卡文件系统
    res = f_mount(&fs_sd, "0:", 1);
    if (res == FR_NO_FILESYSTEM)
    {
        // 格式化
        res = f_mkfs("0:", FM_ANY, 0, mount_buf, sizeof(mount_buf));
        if (res == FR_OK)
        {
            printf("sd format ok\n");
            res = f_mount(&fs_sd, "0:", 1); // 格式化后重新挂载
            if (res == FR_OK)
                printf("sd remount ok\n");
            else
                printf("sd remount other error: %d\n", res);
        }
        else
            printf("sd format failed: %d\n", res);
    }
    else if (res == FR_OK)
        printf("sd mount ok\n");
    else
        printf("sd mount other error: %d\n", res);
}
