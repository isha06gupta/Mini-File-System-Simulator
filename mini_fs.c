// mini_fs.c
// Simple in-memory file system with directory tree.
// Compile: gcc -std=c11 -Wall -Wextra -o mini_fs mini_fs.c
// Run: ./mini_fs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_NAME 64
#define MAX_CONTENT 2048

typedef struct Node {
    char name[MAX_NAME];
    bool is_file;
    char *content;            // for files
    struct Node *parent;
    struct Node *child;       // first child (for directories)
    struct Node *sibling;     // next sibling
} Node;

// Helper: create node
Node* create_node(const char *name, bool is_file) {
    Node *n = (Node*)malloc(sizeof(Node));
    if (!n) { perror("malloc"); exit(1); }
    strncpy(n->name, name, MAX_NAME-1);
    n->name[MAX_NAME-1] = '\0';
    n->is_file = is_file;
    n->parent = n->child = n->sibling = NULL;
    n->content = NULL;
    if (is_file) {
        n->content = (char*)calloc(MAX_CONTENT, 1);
        if (!n->content) { perror("calloc"); exit(1); }
    }
    return n;
}

// Add child to directory (at front)
void add_child(Node *dir, Node *child) {
    if (!dir || dir->is_file) return;
    child->sibling = dir->child;
    dir->child = child;
    child->parent = dir;
}

// Find direct child by name
Node* find_child(Node *dir, const char *name) {
    if (!dir || dir->is_file) return NULL;
    Node *c = dir->child;
    while (c) {
        if (strcmp(c->name, name) == 0) return c;
        c = c->sibling;
    }
    return NULL;
}

// Tokenize path and return pointer to array of tokens (caller must free tokens)
char** split_path(const char *path, int *count) {
    char *tmp = strdup(path);
    char *p = tmp;
    char *tok;
    int cap = 8;
    int n = 0;
    char **arr = malloc(sizeof(char*) * cap);
    while ((tok = strsep(&p, "/")) != NULL) {
        if (strlen(tok) == 0) continue;
        if (n >= cap) {
            cap *= 2;
            arr = realloc(arr, sizeof(char*) * cap);
        }
        arr[n++] = strdup(tok);
    }
    free(tmp);
    *count = n;
    return arr;
}
void free_tokens(char **toks, int n){
    for (int i=0;i<n;i++) free(toks[i]);
    free(toks);
}

// Traverse path from a given start node; returns node or NULL
Node* traverse(Node *start, const char *path) {
    if (!path || strlen(path)==0) return start;
    Node *cur = NULL;
    if (path[0] == '/') {
        // absolute: go to root
        cur = start;
        while (cur->parent) cur = cur->parent;
    } else {
        cur = start;
    }
    int n;
    char **parts = split_path(path, &n);
    for (int i=0;i<n;i++) {
        if (strcmp(parts[i], ".")==0) { continue; }
        if (strcmp(parts[i], "..")==0) {
            if (cur->parent) cur = cur->parent;
            continue;
        }
        Node *child = find_child(cur, parts[i]);
        if (!child) { cur = NULL; break; }
        cur = child;
    }
    free_tokens(parts, n);
    return cur;
}

// Create directory (mkdir)
bool cmd_mkdir(Node *cwd, const char *path) {
    // separate parent path and new dir name
    char buffer[512];
    strncpy(buffer, path, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1]=0;
    char *slash = strrchr(buffer, '/');
    char parent_path[512] = {0};
    char newname[MAX_NAME] = {0};
    if (slash == NULL) {
        strcpy(newname, buffer);
        strcpy(parent_path, "");
    } else if (slash == buffer) {
        // path like /a
        strcpy(parent_path, "/");
        strncpy(newname, slash+1, MAX_NAME-1);
    } else {
        *slash = '\0';
        strncpy(parent_path, buffer, sizeof(parent_path)-1);
        strncpy(newname, slash+1, MAX_NAME-1);
    }
    Node *parent = traverse(cwd, parent_path);
    if (!parent || parent->is_file) { printf("Invalid parent path\n"); return false; }
    if (find_child(parent, newname)) { printf("Name exists\n"); return false; }
    Node *n = create_node(newname, false);
    add_child(parent, n);
    return true;
}

// Create empty file (touch)
bool cmd_touch(Node *cwd, const char *path) {
    char buffer[512];
    strncpy(buffer, path, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1]=0;
    char *slash = strrchr(buffer, '/');
    char parent_path[512] = {0};
    char fname[MAX_NAME] = {0};
    if (!slash) { strcpy(parent_path, ""); strncpy(fname, buffer, MAX_NAME-1); }
    else if (slash == buffer) { strcpy(parent_path, "/"); strncpy(fname, slash+1, MAX_NAME-1); }
    else { *slash = '\0'; strncpy(parent_path, buffer, sizeof(parent_path)-1); strncpy(fname, slash+1, MAX_NAME-1); }
    Node *parent = traverse(cwd, parent_path);
    if (!parent || parent->is_file) { printf("Invalid parent\n"); return false; }
    if (find_child(parent, fname)) { printf("Name exists\n"); return false; }
    Node *f = create_node(fname, true);
    add_child(parent, f);
    return true;
}

// Write to file (overwrite)
bool cmd_write(Node *cwd, const char *path, const char *content) {
    Node *n = traverse(cwd, path);
    if (!n) { printf("File not found\n"); return false; }
    if (!n->is_file) { printf("Path is a directory\n"); return false; }
    strncpy(n->content, content, MAX_CONTENT-1);
    n->content[MAX_CONTENT-1] = '\0';
    return true;
}

// Append to file
bool cmd_append(Node *cwd, const char *path, const char *content) {
    Node *n = traverse(cwd, path);
    if (!n) { printf("File not found\n"); return false; }
    if (!n->is_file) { printf("Path is a directory\n"); return false; }
    strncat(n->content, content, MAX_CONTENT - strlen(n->content) - 1);
    return true;
}

// Read file
bool cmd_read(Node *cwd, const char *path) {
    Node *n = traverse(cwd, path);
    if (!n) { printf("File not found\n"); return false; }
    if (!n->is_file) { printf("Path is a directory\n"); return false; }
    printf("---- %s ----\n%s\n---- end ----\n", path, n->content);
    return true;
}

// Remove file
bool cmd_rm(Node *cwd, const char *path) {
    Node *n = traverse(cwd, path);
    if (!n) { printf("Not found\n"); return false; }
    if (!n->is_file) { printf("Is a directory\n"); return false; }
    Node *parent = n->parent;
    if (!parent) { printf("Cannot remove root\n"); return false; }
    // remove from sibling list
    Node *cur = parent->child;
    Node *prev = NULL;
    while (cur) {
        if (cur == n) {
            if (prev) prev->sibling = cur->sibling;
            else parent->child = cur->sibling;
            break;
        }
        prev = cur;
        cur = cur->sibling;
    }
    free(n->content);
    free(n);
    return true;
}

// Remove empty directory
bool cmd_rmdir(Node *cwd, const char *path) {
    Node *n = traverse(cwd, path);
    if (!n) { printf("Not found\n"); return false; }
    if (n->is_file) { printf("Is a file\n"); return false; }
    if (n->child) { printf("Directory not empty\n"); return false; }
    Node *parent = n->parent;
    if (!parent) { printf("Cannot remove root\n"); return false; }
    Node *cur = parent->child;
    Node *prev = NULL;
    while (cur) {
        if (cur == n) {
            if (prev) prev->sibling = cur->sibling;
            else parent->child = cur->sibling;
            break;
        }
        prev = cur;
        cur = cur->sibling;
    }
    free(n);
    return true;
}

// List directory
void cmd_ls(Node *cwd, const char *path) {
    Node *n = traverse(cwd, path);
    if (!n) { printf("Not found\n"); return; }
    if (n->is_file) {
        printf("%s\n", n->name);
        return;
    }
    Node *c = n->child;
    while (c) {
        printf("%s%s\t", c->name, c->is_file ? "" : "/");
        c = c->sibling;
    }
    printf("\n");
}

// Print working directory by walking to root
void cmd_pwd(Node *cwd) {
    Node *cur = cwd;
    char path[1024] = {0};
    while (cur) {
        if (cur->parent == NULL) {
            // root
            char tmp[1024];
            if (strlen(path)==0) snprintf(tmp, sizeof(tmp), "/");
            else snprintf(tmp, sizeof(tmp), "/%s", path);
            // reverse pieces (path currently reversed with trailing slashes)
            // overwrite for simplicity
            printf("%s\n", tmp);
            return;
        } else {
            char tmp[1024];
            if (strlen(path)==0) snprintf(tmp, sizeof(tmp), "%s", cur->name);
            else snprintf(tmp, sizeof(tmp), "%s/%s", cur->name, path);
            strncpy(path, tmp, sizeof(path)-1);
            cur = cur->parent;
        }
    }
}

// Change directory
Node* cmd_cd(Node *cwd, const char *path) {
    if (!path || strlen(path)==0) return cwd;
    Node *t = traverse(cwd, path);
    if (!t) { printf("No such path\n"); return cwd; }
    if (t->is_file) { printf("Not a directory\n"); return cwd; }
    return t;
}

// Print tree recursively
void print_tree(Node *n, int depth) {
    for (int i=0;i<depth;i++) printf("  ");
    if (n->parent==NULL) printf("/\n");
    else printf("%s%s\n", n->name, n->is_file ? "" : "/");
    Node *c = n->child;
    while (c) {
        print_tree(c, depth+1);
        c = c->sibling;
    }
}

// Free entire tree (post-order)
void free_tree(Node *n) {
    if (!n) return;
    Node *c = n->child;
    while (c) {
        Node *next = c->sibling;
        free_tree(c);
        c = next;
    }
    if (n->is_file) free(n->content);
    free(n);
}

// Small input helpers
void strip_newline(char *s) {
    char *p = strchr(s, '\n');
    if (p) *p = '\0';
}

int main() {
    // create root
    Node *root = create_node("", false);
    root->parent = NULL;

    Node *cwd = root;

    char line[4096];
    printf("MiniFS ready. Type help for commands.\n");
    while (1) {
        // show prompt with last component
        if (cwd->parent == NULL) printf("/> ");
        else printf("%s/> ", cwd->name);
        if (!fgets(line, sizeof(line), stdin)) break;
        strip_newline(line);
        if (strlen(line)==0) continue;

        // split command and args
        char *cmd = strtok(line, " ");
        if (!cmd) continue;
        if (strcmp(cmd, "help")==0) {
            printf("Commands: mkdir <path>, touch <path>, write <path> <text>, append <path> <text>, read <path>, ls [path], rm <path>, rmdir <path>, cd <path>, pwd, tree, exit\n");
        } else if (strcmp(cmd, "exit")==0) {
            break;
        } else if (strcmp(cmd, "mkdir")==0) {
            char *arg = strtok(NULL, "");
            if (!arg) { printf("Usage: mkdir <path>\n"); continue; }
            cmd_mkdir(cwd, arg);
        } else if (strcmp(cmd, "touch")==0) {
            char *arg = strtok(NULL, "");
            if (!arg) { printf("Usage: touch <path>\n"); continue; }
            cmd_touch(cwd, arg);
        } else if (strcmp(cmd, "write")==0) {
            char *path = strtok(NULL, " ");
            char *rest = strtok(NULL, "");
            if (!path || !rest) { printf("Usage: write <path> <text>\n"); continue; }
            cmd_write(cwd, path, rest);
        } else if (strcmp(cmd, "append")==0) {
            char *path = strtok(NULL, " ");
            char *rest = strtok(NULL, "");
            if (!path || !rest) { printf("Usage: append <path> <text>\n"); continue; }
            cmd_append(cwd, path, rest);
        } else if (strcmp(cmd, "read")==0) {
            char *arg = strtok(NULL, "");
            if (!arg) { printf("Usage: read <path>\n"); continue; }
            cmd_read(cwd, arg);
        } else if (strcmp(cmd, "ls")==0) {
            char *arg = strtok(NULL, "");
            if (!arg) cmd_ls(cwd, "");
            else cmd_ls(cwd, arg);
        } else if (strcmp(cmd, "rm")==0) {
            char *arg = strtok(NULL, "");
            if (!arg) { printf("Usage: rm <path>\n"); continue; }
            cmd_rm(cwd, arg);
        } else if (strcmp(cmd, "rmdir")==0) {
            char *arg = strtok(NULL, "");
            if (!arg) { printf("Usage: rmdir <path>\n"); continue; }
            cmd_rmdir(cwd, arg);
        } else if (strcmp(cmd, "cd")==0) {
            char *arg = strtok(NULL, "");
            if (!arg) { cwd = root; continue; }
            cwd = cmd_cd(cwd, arg);
        } else if (strcmp(cmd, "pwd")==0) {
            cmd_pwd(cwd);
        } else if (strcmp(cmd, "tree")==0) {
            Node *rt = root;
            while (rt->parent) rt = rt->parent;
            print_tree(rt, 0);
        } else {
            printf("Unknown command. Type help.\n");
        }
    }

    free_tree(root);
    printf("Bye.\n");
    return 0;
}

