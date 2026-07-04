#ifndef __FS_H__
#define __FS_H__

typedef enum
{
    FS_NODE_FILE = 0,
    FS_NODE_DIR,
} FS_NODE_TYPE_t;

typedef struct FS_NODE FS_NODE_t;
typedef struct FS_TREE FS_TREE_t;

FS_TREE_t *fs_tree_create(const char *root_path);
void fs_tree_destroy(FS_TREE_t *tree);
void fs_tree_scan(FS_TREE_t *tree);
FS_NODE_t *fs_tree_get_root_node(FS_TREE_t *tree);

char *fs_node_get_full_path(FS_NODE_t *node);
char *fs_node_get_name(FS_NODE_t *node);
FS_NODE_t *fs_node_get_parent(FS_NODE_t *node);
FS_NODE_t **fs_node_get_children(FS_NODE_t *node);
FS_NODE_TYPE_t fs_node_get_type(FS_NODE_t *node);
uint32_t fs_node_get_child_count(FS_NODE_t *node);

void fs_mount();

#endif