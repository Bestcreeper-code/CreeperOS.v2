#include "debug/Logger.h"
#include "memory/memory.h"
#include "vfs/vfs.h"


#include "defines/container_of.h"
#include "defines/err_codes.h"
#include "string/string.h"
#include "memops.h"
struct dentry* kpath_lookup(struct inode* start, const char* path) {
    if (!path || !start) return NULL;

    char* path_copy = strdup(path);
    if (!path_copy) return NULL;

    struct dentry* curr = start->i_dentry;
    char* save  = NULL;
    char* token = strtok_r(path_copy, "/", &save);

    while (token) {

        if (strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &save);
            continue;
        }

        if (strcmp(token, "..") == 0) {
            curr  = curr->parent ? curr->parent : curr;
            token = strtok_r(NULL, "/", &save);
            continue;
        }

        struct dentry next_tmp = { .name = token, .inode = NULL, .parent = curr };

        struct dentry* result = curr->inode->i_op->lookup(curr->inode, &next_tmp, 0);
        if (!result) {
            kfree(path_copy);
            return NULL;
        }

        curr  = result;
        token = strtok_r(NULL, "/", &save);
    }

    kfree(path_copy);
    curr->inode->i_count++;
    return curr;
}



int kpath_create(struct inode* start, const char* path, umode_t mode, bool excl)
{
    if (!start || !path)
        return -E_INVAL;

    char* copy = strdup(path);
    if (!copy)
        return -E_NOMEM;

    char* last_slash = strrchr(copy, '/');
    char* parent_path;
    char* name;

    if (last_slash) {
        if (last_slash == copy) {
            parent_path = "/";
        } else {
            *last_slash = '\0';
            parent_path = copy;
        }
        name = last_slash + 1;
    } else {
        parent_path = ".";
        name = copy;
    }

    struct dentry* dir = kpath_lookup(start, parent_path);
    if (!dir) {
        kfree(copy);
        return -E_NOENT;
    }

    struct dentry* newnode = kmalloc(sizeof(struct dentry));
    if (!newnode) {
        kfree(copy);
        return -E_NOMEM;
    }

    memset(newnode, 0, sizeof(struct dentry));

    newnode->name   = strdup(name);
    newnode->parent = dir;

    if (!newnode->name) {
        kfree(newnode);
        kfree(copy);
        return -E_NOMEM;
    }

    int ret = dir->inode->i_op->create(
        dir->inode,
        newnode,
        mode,
        excl
    );

    if (ret < 0) {
        kfree(newnode->name);
        kfree(newnode);
    }
    

    kfree(copy);
    return ret;
}


int kpath_mkdir(struct inode* start, const char* path, umode_t mode) {
    return kpath_create(start, path, mode | S_IFDIR, true);
}

int kpath_create_force(struct inode* start, const char* path, umode_t mode, bool excl)
{
    if (!start || !path)
        return -E_INVAL;

    struct dentry* curr;

    if (path[0] == '/')
        curr = root_dentry;
    else
        curr = start->i_dentry;

    char* copy = strdup(path);
    if (!copy)
        return -E_NOMEM;

    char* save = NULL;
    char* token = strtok_r(copy, "/", &save);

    while (token) {

        if (strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &save);
            continue;
        }

        if (strcmp(token, "..") == 0) {
            if (curr->parent)
                curr = curr->parent;
            token = strtok_r(NULL, "/", &save);
            continue;
        }

        char* next_token = strtok_r(NULL, "/", &save);
        bool is_last = (next_token == NULL);

        struct dentry tmp = {
            .name = token,
            .inode = NULL,
            .parent = curr
        };

        struct dentry* next =
            curr->inode->i_op->lookup(curr->inode, &tmp, 0);

        if (!next) {
            struct dentry* newnode = kmalloc(sizeof(struct dentry));
            if (!newnode) {
                kfree(copy);
                return -E_NOMEM;
            }

            memset(newnode, 0, sizeof(struct dentry));

            newnode->name   = strdup(token);
            newnode->parent = curr;

            if (!newnode->name) {
                kfree(newnode);
                kfree(copy);
                return -E_NOMEM;
            }

            int ret;
            if (is_last) {

                ret = curr->inode->i_op->create(
                    curr->inode,
                    newnode,
                    mode,
                    excl
                );
            } else {

                ret = curr->inode->i_op->mkdir(
                    curr->inode,
                    newnode,
                    S_IFDIR | 0755
                );
            }

            if (ret < 0) {
                kfree(newnode->name);
                kfree(newnode);
                kfree(copy);
                return ret;
            }

            next = newnode;
        }

        curr  = next;
        token = next_token;
    }

    kfree(copy);
    return 0;
}

int kpath_mkdir_force(struct inode* start, const char* path, umode_t mode) {
    kpath_create_force(start, path, S_IFDIR | mode, true);
    return 0;
}

int kpath_rmdir(struct inode* start, const char* path, char* name) {
    struct dentry* dir = kpath_lookup(start, path);

    if (!dir) RET_ERR(E_NOENT);

    struct dentry* target = (struct dentry*)kmalloc(sizeof(struct dentry));
    if (!target) RET_ERR(E_NOMEM);

    target->name   = name;
    target->parent = dir;

    return dir->inode->i_op->rmdir(dir->inode, target);
}

int path_unlink(struct inode* start, const char* path, char* name) {
    struct dentry* dir = kpath_lookup(start, path);

    if (!dir) RET_ERR(E_NOENT);

    struct dentry* target = (struct dentry*)kmalloc(sizeof(struct dentry));
    if (!target) RET_ERR(E_NOMEM);

    target->name   = name;
    target->parent = dir;

    return dir->inode->i_op->unlink(dir->inode, target);
}

void tree(struct dentry* d, int depth){
    if(!d) return;
    
    for(int i=0;i<depth;i++) Sys_log_NoPos("  ");
    
    
    Sys_log_NoPos(d->name);
    if(S_ISDIR(d->inode->i_mode)) Sys_log_NoPos("/");
    Sys_log_NoPos("\n");

    struct hlist_node* pos;
    struct dentry* child;

    hlist_for_each(pos, &d->d_children){
        child = container_of(pos, struct dentry, d_sib);
        tree(child, depth+1);
    }
}



/*
int (*mkdir)(struct inode *, struct dentry *, umode_t);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*create)(struct inode *, struct dentry *, umode_t, bool);
    int (*unlink)(struct inode *, struct dentry *);
*/