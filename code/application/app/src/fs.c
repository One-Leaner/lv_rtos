#include "ff.h"
#include "fs.h"
#include "bsp_driver_sd.h"
#include "heap.h"
#include "encoder.h"
#include <stdio.h>
#include <string.h>

struct FS_NODE
{
    char *name;
    char *full_path;
    uint32_t size;
    FS_NODE_TYPE_t type;
    struct FS_NODE *parent;
    struct FS_NODE **children;
    uint32_t child_count;
};

struct FS_TREE
{
    FS_NODE_t *root;
    FS_NODE_t *current_node;
    uint32_t total_files;
    uint32_t total_dirs;
};

static FS_NODE_t *fs_node_create(FS_NODE_t *parent, const char *name, FS_NODE_TYPE_t type, uint32_t size)
{
    FS_NODE_t *node = (FS_NODE_t *)heap.api->malloc(heap.ctrl, sizeof(FS_NODE_t));
    if (node == NULL)
        return NULL;

    static char utf8_name[256];
    encoder_gbk_to_utf8(name, utf8_name, sizeof(utf8_name));

    uint16_t name_len = strlen(utf8_name) + 1;
    node->name = heap.api->malloc(heap.ctrl, name_len);
    if (node->name == NULL)
    {
        heap.api->free(heap.ctrl, node);
        return NULL;
    }
    strcpy(node->name, utf8_name);

    uint16_t path_len;
    if (parent == NULL)
    {
        path_len = name_len;
        node->full_path = heap.api->malloc(heap.ctrl, path_len);
        if (node->full_path == NULL)
        {
            heap.api->free(heap.ctrl, node->name);
            heap.api->free(heap.ctrl, node);
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
            heap.api->free(heap.ctrl, node->name);
            heap.api->free(heap.ctrl, node);
            return NULL;
        }
        snprintf(node->full_path, path_len, "%s/%s", parent->full_path, name);
    }

    node->size = size;
    node->type = type;
    node->parent = parent;
    node->children = NULL;
    node->child_count = 0;

    return node;
}

static void fs_node_destroy(FS_NODE_t *node)
{
    if (node == NULL)
        return;

    for (uint32_t i = 0; i < node->child_count; i++)
    {
        fs_node_destroy(node->children[i]);
    }

    heap.api->free(heap.ctrl, node->name);
    heap.api->free(heap.ctrl, node->full_path);
    heap.api->free(heap.ctrl, node->children);
    heap.api->free(heap.ctrl, node);
}

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
        return 0;

    parent->child_count = 0;
    return 1;
}

static uint8_t fs_node_add_child(FS_NODE_t *parent, FS_NODE_t *child)
{
    if (parent == NULL || child == NULL || parent->type == FS_NODE_FILE)
        return 0;

    parent->children[parent->child_count++] = child;
    child->parent = parent;

    return 1;
}

FS_TREE_t *fs_tree_create(const char *root_path)
{
    FS_TREE_t *tree = (FS_TREE_t *)heap.api->malloc(heap.ctrl, sizeof(FS_TREE_t));
    if (tree == NULL)
        return NULL;

    tree->root = fs_node_create(NULL, root_path, FS_NODE_DIR, 0);
    if (tree->root == NULL)
    {
        heap.api->free(heap.ctrl, tree);
        return NULL;
    }

    tree->current_node = tree->root;
    tree->total_files = 0;
    tree->total_dirs = 1;

    return tree;
}

void fs_tree_destroy(FS_TREE_t *tree)
{
    if (tree == NULL)
        return;

    fs_node_destroy(tree->root);
    heap.api->free(heap.ctrl, tree);
}

static int fs_should_skip(const FILINFO *fno)
{
    if (fno->fname[0] == 0)
        return 1;
    if (strcmp(fno->fname, ".") == 0 || strcmp(fno->fname, "..") == 0)
        return 1;
    if (fno->fattrib & (AM_HID | AM_SYS))
        return 1;
    return 0;
}

void fs_tree_scan(FS_TREE_t *tree)
{
    if (tree == NULL || tree->current_node == NULL)
        return;

    DIR dir;
    FILINFO fno;
    FRESULT res;
    uint32_t child_count = 0;

    res = f_opendir(&dir, tree->current_node->full_path);
    if (res != FR_OK)
        return;

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0)
    {
        if (!fs_should_skip(&fno))
            child_count++;
    }

    if (child_count == 0)
    {
        f_closedir(&dir);
        return;
    }

    if (!fs_node_reserve_children(tree->current_node, child_count))
    {
        f_closedir(&dir);
        return;
    }

    f_rewinddir(&dir);

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0)
    {
        if (fs_should_skip(&fno))
            continue;

        FS_NODE_t *node = NULL;

        if (fno.fattrib & AM_DIR)
        {
            node = fs_node_create(tree->current_node, fno.fname, FS_NODE_DIR, 0);
            if (node != NULL)
            {
                tree->total_dirs++;
                fs_node_add_child(tree->current_node, node);
                tree->current_node = node;
                fs_tree_scan(tree);
                tree->current_node = node->parent;
            }
        }
        else
        {
            node = fs_node_create(tree->current_node, fno.fname, FS_NODE_FILE, fno.fsize);
            if (node != NULL)
            {
                tree->total_files++;
                fs_node_add_child(tree->current_node, node);
            }
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
    return (node != NULL) ? node->full_path : NULL;
}

char *fs_node_get_name(FS_NODE_t *node)
{
    return (node != NULL) ? node->name : NULL;
}

FS_NODE_t *fs_node_get_parent(FS_NODE_t *node)
{
    return (node != NULL) ? node->parent : NULL;
}

FS_NODE_t **fs_node_get_children(FS_NODE_t *node)
{
    return (node != NULL) ? node->children : NULL;
}

FS_NODE_TYPE_t fs_node_get_type(FS_NODE_t *node)
{
    return (node != NULL) ? node->type : FS_NODE_FILE;
}

uint32_t fs_node_get_child_count(FS_NODE_t *node)
{
    return (node != NULL) ? node->child_count : 0;
}

void fs_mount(void)
{
    if (BSP_SD_Init() != MSD_OK)
    {
        printf("sd init failed\n");
        return;
    }
    printf("sd init success\n");

    static FATFS fs_sd;
    FRESULT res = f_mount(&fs_sd, "0:", 1);

    if (res == FR_OK)
    {
        printf("sd mount ok\n");
    }
    else if (res == FR_NO_FILESYSTEM)
    {
        static uint8_t mount_buf[4096];
        res = f_mkfs("0:", FM_ANY, 0, mount_buf, sizeof(mount_buf));
        if (res == FR_OK)
        {
            printf("sd format ok\n");
            res = f_mount(&fs_sd, "0:", 1);
            if (res == FR_OK)
                printf("sd remount ok\n");
            else
                printf("sd remount error: %d\n", (int)res);
        }
        else
        {
            printf("sd format failed: %d\n", (int)res);
        }
    }
    else
    {
        printf("sd mount error: %d\n", (int)res);
    }
}