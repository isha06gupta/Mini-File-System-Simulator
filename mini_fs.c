// mini_fs.c
// Simple in-memory file system with directory tree.
// Compile: gcc -std=c11 -Wall -Wextra -o mini_fs mini_fs.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>

#define MAX_NAME 64
#define MAX_CONTENT 2048
#define MAX_PATH 1024
#define AUTOSAVE_FILE "minifs.autosave"
#define JOURNAL_FILE "journal.log"
#define SERIAL_MAGIC 0x4D465331u
#define SERIAL_VERSION 2u
#define MAX_SERIAL_DEPTH 128u
#define MAX_SERIAL_CHILDREN 100000u
#define CACHE_CAPACITY 10u
#define INDEX_BUCKETS 101u

/* ==================== Structures and shared state ==================== */

/* Immutable snapshot of a file before write/append/version restoration. */
typedef struct FileVersion {
    char *content;
    size_t size;
    time_t timestamp;
    struct FileVersion *next;
} FileVersion;

/* A tree node represents a file, directory, or symbolic link. */
typedef struct Node {
    char name[MAX_NAME], target[MAX_PATH], original[MAX_PATH];
    bool is_file;
    bool hidden;
    bool is_symlink;
    char *content;
    char *link_target;
    char original_path[MAX_PATH];
    FileVersion *versions;
    size_t size;
    unsigned int permissions;
    time_t created_at;
    time_t modified_at;
    struct Node *parent;
    struct Node *child;
    struct Node *sibling;
} Node;

typedef struct {
    size_t files;
    size_t directories;
    size_t size;
} Counts;

typedef struct {
    uint32_t magic;
    uint32_t version;
} FileHeader;

/* Undo/redo stacks own complete cloned filesystem roots. */
typedef struct Snapshot {
    Node *root;
    struct Snapshot *next;
} Snapshot;

/* Fixed-size LRU cache for recently accessed files. */
typedef struct CacheEntry {
    Node *node;
    char path[MAX_PATH];
    unsigned long stamp;
} CacheEntry;

/* Chained hash-table entry used by locate. */
typedef struct IndexEntry {
    Node *node;
    struct IndexEntry *next;
} IndexEntry;

static Snapshot *undo_stack = NULL;
static Snapshot *redo_stack = NULL;
static size_t quota_limit = SIZE_MAX;
static CacheEntry file_cache[CACHE_CAPACITY];
static size_t cache_count = 0;
static unsigned long cache_clock = 0;
static size_t cache_hits = 0;
static size_t cache_misses = 0;
static IndexEntry *search_index[INDEX_BUCKETS];

static size_t recursive_size(const Node *node);
static void node_path(const Node *node, char *path, size_t path_size);
static void rebuild_search_index(Node *root);
static void clear_cache(void);

/* ==================== Utility and tree helpers ==================== */

static void touch_metadata(Node *node) {
    if (node) node->modified_at = time(NULL);
}

static bool is_valid_node_name(const char *name) {
    size_t length;
    if (!name || !*name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;
    length = strlen(name);
    if (length >= MAX_NAME) return false;
    for (size_t i = 0; i < length; i++)
        if (name[i] == '/' || name[i] == '\\' || (unsigned char)name[i] < 32) return false;
    return true;
}

Node *create_node(const char *name, bool is_file) {
    Node *node = calloc(1, sizeof(*node));
    if (!node) { perror("calloc"); exit(EXIT_FAILURE); }
    if (!name || (name[0] && !is_valid_node_name(name))) { free(node); return NULL; }
    strncpy(node->name, name, MAX_NAME - 1);
    node->is_file = is_file;
    node->hidden = name[0] == '.';
    node->permissions = is_file ? 0644 : 0755;
    node->created_at = node->modified_at = time(NULL);
    if (is_file) {
        node->content = calloc(MAX_CONTENT, 1);
        if (!node->content) { perror("calloc"); free(node); exit(EXIT_FAILURE); }
    }
    return node;
}

void add_child(Node *dir, Node *child) {
    if (!dir || dir->is_file || dir->is_symlink || !child || child->parent) return;
    child->sibling = dir->child;
    dir->child = child;
    child->parent = dir;
    touch_metadata(dir);
}

Node *find_child(Node *dir, const char *name) {
    Node *child;
    if (!dir || dir->is_file) return NULL;
    for (child = dir->child; child; child = child->sibling)
        if (strcmp(child->name, name) == 0) return child;
    return NULL;
}

Node *root_of(Node *node) {
    while (node && node->parent) node = node->parent;
    return node;
}

static Node *traverse_internal(Node *start, const char *path, unsigned int link_depth) {
    const char *cursor;
    Node *current;
    if (!start || !path || path[0] == '\0') return start;
    if (link_depth > MAX_SERIAL_DEPTH) return NULL;
    current = path[0] == '/' ? root_of(start) : start;
    cursor = path;
    while (*cursor) {
        char part[MAX_NAME];
        size_t length = 0;
        Node *child;
        while (*cursor == '/') cursor++;
        if (!*cursor) break;
        while (cursor[length] && cursor[length] != '/') {
            if (length >= MAX_NAME - 1) return NULL;
            part[length] = cursor[length];
            length++;
        }
        part[length] = '\0';
        cursor += length;
        if (strcmp(part, ".") == 0) continue;
        if (strcmp(part, "..") == 0) { if (current->parent) current = current->parent; continue; }
        child = find_child(current, part);
        if (!child) return NULL;
        if (child->is_symlink) {
            current = traverse_internal(child->parent, child->link_target, link_depth + 1);
            if (!current) return NULL;
        } else current = child;
    }
    return current;
}

Node *traverse(Node *start, const char *path) { return traverse_internal(start, path, 0); }

bool split_parent_path(const char *path, char *parent_path, char *name) {
    char buffer[MAX_PATH], *slash;
    size_t length;
    if (!path || !*path || strlen(path) >= sizeof(buffer)) return false;
    strcpy(buffer, path);
    length = strlen(buffer);
    while (length > 1 && buffer[length - 1] == '/') buffer[--length] = '\0';
    slash = strrchr(buffer, '/');
    if (!slash) { parent_path[0] = '\0'; strncpy(name, buffer, MAX_NAME - 1); }
    else if (slash == buffer) { strcpy(parent_path, "/"); strncpy(name, slash + 1, MAX_NAME - 1); }
    else { *slash = '\0'; strcpy(parent_path, buffer); strncpy(name, slash + 1, MAX_NAME - 1); }
    name[MAX_NAME - 1] = '\0';
    return is_valid_node_name(name);
}

bool has_write_permission(const Node *node) { return node && (node->permissions & 0222) != 0; }

void detach_node(Node *node) {
    Node *current, *previous = NULL, *parent;
    if (!node || !(parent = node->parent)) return;
    for (current = parent->child; current; previous = current, current = current->sibling) {
        if (current == node) {
            if (previous) previous->sibling = current->sibling;
            else parent->child = current->sibling;
            node->parent = node->sibling = NULL;
            touch_metadata(parent);
            return;
        }
    }
}

void free_tree(Node *node) {
    Node *child, *next;
    FileVersion *version, *next_version;
    if (!node) return;
    for (child = node->child; child; child = next) { next = child->sibling; free_tree(child); }
    for (version = node->versions; version; version = next_version) {
        next_version = version->next;
        free(version->content);
        free(version);
    }
    free(node->content);
    free(node->link_target);
    free(node);
}

static void save_file_version(Node *node) {
    FileVersion *version;
    if (!node || !node->is_file) return;
    version = malloc(sizeof(*version));
    if (!version) { perror("malloc"); exit(EXIT_FAILURE); }
    version->content = malloc(node->size + 1);
    if (!version->content) { perror("malloc"); free(version); exit(EXIT_FAILURE); }
    memcpy(version->content, node->content, node->size + 1);
    version->size = node->size;
    version->timestamp = time(NULL);
    version->next = node->versions;
    node->versions = version;
}

static FileVersion *clone_versions(const FileVersion *source) {
    FileVersion *copy = NULL, **tail = &copy;
    while (source) {
        FileVersion *item = malloc(sizeof(*item));
        if (!item) { perror("malloc"); free_tree(NULL); exit(EXIT_FAILURE); }
        item->content = malloc(source->size + 1);
        if (!item->content) { perror("malloc"); free(item); exit(EXIT_FAILURE); }
        memcpy(item->content, source->content, source->size + 1);
        item->size = source->size;
        item->timestamp = source->timestamp;
        item->next = NULL;
        *tail = item;
        tail = &item->next;
        source = source->next;
    }
    return copy;
}

static Node *clone_tree(const Node *source) {
    const Node *child;
    Node *copy = create_node(source->name, source->is_file);
    Node *last_child = NULL;
    copy->hidden = source->hidden;
    copy->is_symlink = source->is_symlink;
    copy->permissions = source->permissions;
    copy->size = source->size;
    copy->created_at = source->created_at;
    copy->modified_at = source->modified_at;
    memcpy(copy->original_path, source->original_path, sizeof(copy->original_path));
    if (source->is_file) memcpy(copy->content, source->content, source->size + 1);
    if (source->link_target) {
        copy->link_target = malloc(strlen(source->link_target) + 1);
        if (!copy->link_target) { perror("malloc"); free_tree(copy); exit(EXIT_FAILURE); }
        strcpy(copy->link_target, source->link_target);
    }
    copy->versions = clone_versions(source->versions);
    for (child = source->child; child; child = child->sibling) {
        Node *child_copy = clone_tree(child);
        child_copy->parent = copy;
        if (last_child) last_child->sibling = child_copy;
        else copy->child = child_copy;
        last_child = child_copy;
    }
    return copy;
}

static void clear_snapshot_stack(Snapshot **stack) {
    Snapshot *snapshot;
    while ((snapshot = *stack) != NULL) {
        *stack = snapshot->next;
        free_tree(snapshot->root);
        free(snapshot);
    }
}

static void push_snapshot(Snapshot **stack, Node *root) {
    Snapshot *snapshot = malloc(sizeof(*snapshot));
    if (!snapshot) { perror("malloc"); free_tree(root); exit(EXIT_FAILURE); }
    snapshot->root = root;
    snapshot->next = *stack;
    *stack = snapshot;
}

static Node *pop_snapshot(Snapshot **stack) {
    Snapshot *snapshot = *stack;
    Node *root;
    if (!snapshot) return NULL;
    *stack = snapshot->next;
    root = snapshot->root;
    free(snapshot);
    return root;
}

static bool write_bytes(FILE *file, const void *data, size_t size) {
    if (size == 0) return true;
    return fwrite(data, 1, size, file) == size;
}

static bool read_bytes(FILE *file, void *data, size_t size) {
    if (size == 0) return true;
    return fread(data, 1, size, file) == size;
}

static bool serialize_node(FILE *file, const Node *node) {
    const Node *child;
    const FileVersion *version;
    uint32_t name_length = (uint32_t)strlen(node->name);
    uint8_t type = node->is_file ? 1u : 0u;
    uint8_t hidden = node->hidden ? 1u : 0u;
    uint8_t symlink = node->is_symlink ? 1u : 0u;
    uint16_t reserved = 0;
    uint32_t permissions = node->permissions;
    uint32_t child_count = 0;
    uint32_t target_length = node->link_target ? (uint32_t)strlen(node->link_target) : 0;
    uint32_t original_length = (uint32_t)strlen(node->original_path);
    uint32_t version_count = 0;
    uint64_t size = (uint64_t)node->size;
    int64_t created = (int64_t)node->created_at;
    int64_t modified = (int64_t)node->modified_at;

    for (child = node->child; child; child = child->sibling) child_count++;
    for (version = node->versions; version; version = version->next) version_count++;
    if (!write_bytes(file, &name_length, sizeof(name_length)) ||
        !write_bytes(file, node->name, name_length) ||
        !write_bytes(file, &type, sizeof(type)) || !write_bytes(file, &hidden, sizeof(hidden)) || !write_bytes(file, &symlink, sizeof(symlink)) ||
        !write_bytes(file, &reserved, sizeof(reserved)) ||
        !write_bytes(file, &permissions, sizeof(permissions)) ||
        !write_bytes(file, &size, sizeof(size)) || !write_bytes(file, &created, sizeof(created)) ||
        !write_bytes(file, &modified, sizeof(modified)) || !write_bytes(file, &target_length, sizeof(target_length)) ||
        !write_bytes(file, node->link_target, target_length) || !write_bytes(file, &original_length, sizeof(original_length)) ||
        !write_bytes(file, node->original_path, original_length) || !write_bytes(file, &version_count, sizeof(version_count)) ||
        !write_bytes(file, &child_count, sizeof(child_count))) return false;
    if (node->is_file && node->size && !write_bytes(file, node->content, node->size)) return false;
    for (version = node->versions; version; version = version->next) {
        uint64_t version_size = version->size;
        int64_t version_time = (int64_t)version->timestamp;
        if (!write_bytes(file, &version_size, sizeof(version_size)) || !write_bytes(file, &version_time, sizeof(version_time)) ||
            !write_bytes(file, version->content, version->size)) return false;
    }
    for (child = node->child; child; child = child->sibling)
        if (!serialize_node(file, child)) return false;
    return true;
}

static bool validate_tree(const Node *node, const Node *expected_parent, unsigned int depth) {
    const Node *child, *other;
    if (!node || node->parent != expected_parent || depth > MAX_SERIAL_DEPTH) return false;
    if (node->is_file && node->child) return false;
    if (node->is_file && (!node->content || node->size >= MAX_CONTENT)) return false;
    if (node->is_symlink && (node->is_file || node->child || !node->link_target)) return false;
    for (child = node->child; child; child = child->sibling) {
        if (child == node || child->parent != node || child->name[0] == '\0') return false;
        for (other = child->sibling; other; other = other->sibling)
            if (strcmp(child->name, other->name) == 0) return false;
        if (!validate_tree(child, node, depth + 1)) return false;
    }
    return true;
}

static bool deserialize_node(FILE *file, Node **result, Node *parent, unsigned int depth, bool root_node) {
    uint32_t name_length, permissions, child_count, target_length, original_length, version_count;
    uint8_t type, hidden, symlink;
    uint16_t reserved;
    uint64_t size;
    int64_t created, modified;
    char name[MAX_NAME], target[MAX_PATH], original[MAX_PATH];
    Node *node, *last_child = NULL;

    if (depth > MAX_SERIAL_DEPTH || !read_bytes(file, &name_length, sizeof(name_length)) ||
        name_length >= MAX_NAME || !read_bytes(file, name, name_length) ||
        !read_bytes(file, &type, sizeof(type)) || !read_bytes(file, &hidden, sizeof(hidden)) || !read_bytes(file, &symlink, sizeof(symlink)) ||
        !read_bytes(file, &reserved, sizeof(reserved)) || !read_bytes(file, &permissions, sizeof(permissions)) ||
        !read_bytes(file, &size, sizeof(size)) || !read_bytes(file, &created, sizeof(created)) ||
        !read_bytes(file, &modified, sizeof(modified)) || !read_bytes(file, &target_length, sizeof(target_length)) ||
        target_length >= MAX_PATH || !read_bytes(file, target, target_length) || !read_bytes(file, &original_length, sizeof(original_length)) ||
        original_length >= MAX_PATH || !read_bytes(file, original, original_length) || !read_bytes(file, &version_count, sizeof(version_count)) ||
        !read_bytes(file, &child_count, sizeof(child_count))) return false;
    name[name_length] = '\0';
    target[target_length] = '\0';
    original[original_length] = '\0';
    if ((root_node && (name_length != 0 || type != 0 || symlink != 0)) || (!root_node && !is_valid_node_name(name)) || type > 1 || hidden > 1 || symlink > 1 ||
        permissions > 0777 || child_count > MAX_SERIAL_CHILDREN || version_count > MAX_SERIAL_CHILDREN || size >= MAX_CONTENT ||
        (type == 0 && size != 0) || (symlink && (type || child_count || version_count || target_length == 0))) return false;
    node = create_node(name, type == 1);
    node->parent = parent;
    node->hidden = hidden == 1;
    node->permissions = permissions;
    node->size = (size_t)size;
    node->created_at = (time_t)created;
    node->modified_at = (time_t)modified;
    node->is_symlink = symlink == 1;
    if (target_length) {
        node->link_target = malloc(target_length + 1);
        if (!node->link_target) { perror("malloc"); free_tree(node); exit(EXIT_FAILURE); }
        strcpy(node->link_target, target);
    }
    strcpy(node->original_path, original);
    if (node->is_file && node->size && !read_bytes(file, node->content, node->size)) { free_tree(node); return false; }
    if (node->is_file) node->content[node->size] = '\0';
    for (uint32_t i = 0; i < version_count; i++) {
        FileVersion *version;
        uint64_t version_size;
        int64_t version_time;
        if (!node->is_file || !read_bytes(file, &version_size, sizeof(version_size)) || version_size >= MAX_CONTENT ||
            !read_bytes(file, &version_time, sizeof(version_time))) { free_tree(node); return false; }
        version = malloc(sizeof(*version));
        if (!version) { perror("malloc"); free_tree(node); exit(EXIT_FAILURE); }
        version->content = malloc((size_t)version_size + 1);
        if (!version->content) { perror("malloc"); free(version); free_tree(node); exit(EXIT_FAILURE); }
        if (!read_bytes(file, version->content, (size_t)version_size)) { free(version->content); free(version); free_tree(node); return false; }
        version->content[version_size] = '\0';
        version->size = (size_t)version_size;
        version->timestamp = (time_t)version_time;
        version->next = node->versions;
        node->versions = version;
    }
    for (uint32_t i = 0; i < child_count; i++) {
        Node *child;
        if (!deserialize_node(file, &child, node, depth + 1, false)) { free_tree(node); return false; }
        if (last_child) last_child->sibling = child;
        else node->child = child;
        last_child = child;
    }
    *result = node;
    return true;
}

static bool save_filesystem(const Node *root, const char *filename) {
    FileHeader header = { SERIAL_MAGIC, SERIAL_VERSION };
    FILE *file;
    if (!filename || !*filename) return false;
    file = fopen(filename, "wb");
    if (!file) return false;
    if (!write_bytes(file, &header, sizeof(header)) || !serialize_node(file, root)) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

static bool load_filesystem(const char *filename, Node **root) {
    FileHeader header;
    Node *loaded = NULL;
    FILE *file;
    int trailing;
    if (!filename || !*filename || !root) return false;
    file = fopen(filename, "rb");
    if (!file) return false;
    if (!read_bytes(file, &header, sizeof(header)) || header.magic != SERIAL_MAGIC ||
        header.version != SERIAL_VERSION || !deserialize_node(file, &loaded, NULL, 0, true) ||
        !validate_tree(loaded, NULL, 0) || (trailing = fgetc(file)) != EOF) {
        free_tree(loaded);
        fclose(file);
        return false;
    }
    fclose(file);
    free_tree(*root);
    *root = loaded;
    return true;
}

static bool autosave_filesystem(const Node *root) {
    return save_filesystem(root, AUTOSAVE_FILE);
}

static bool file_exists(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static void append_journal(const char *operation, const char *target) {
    char timestamp[32];
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    FILE *file = fopen(JOURNAL_FILE, "a");
    if (!file || !time_info) { if (file) fclose(file); return; }
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);
    fprintf(file, "%s\n%s\n%s\n", timestamp, operation, target ? target : "/");
    fclose(file);
}

static void record_mutation(const Node *root, const char *operation, const char *target) {
    if (!autosave_filesystem(root)) printf("Warning: autosave failed\n");
    append_journal(operation, target);
    rebuild_search_index((Node *)root);
    if (strcmp(operation, "WRITE") != 0 && strcmp(operation, "APPEND") != 0 && strcmp(operation, "CHMOD") != 0)
        clear_cache();
}

static size_t journal_entry_count(void) {
    char line[MAX_PATH];
    size_t lines = 0;
    FILE *file = fopen(JOURNAL_FILE, "r");
    if (!file) return 0;
    while (fgets(line, sizeof(line), file)) lines++;
    fclose(file);
    return lines / 3;
}

bool is_descendant(const Node *node, const Node *possible_parent) {
    const Node *current;
    for (current = possible_parent; current; current = current->parent)
        if (current == node) return true;
    return false;
}

void node_path(const Node *node, char *path, size_t path_size) {
    const Node *parts[128];
    size_t count = 0, used = 0;
    if (!node || !path_size) return;
    while (node && node->parent && count < sizeof(parts) / sizeof(parts[0])) { parts[count++] = node; node = node->parent; }
    path[0] = '\0';
    if (count == 0) { snprintf(path, path_size, "/"); return; }
    while (count) {
        const char *name = parts[--count]->name;
        int written = snprintf(path + used, path_size - used, "/%s", name);
        if (written < 0 || (size_t)written >= path_size - used) { path[path_size - 1] = '\0'; return; }
        used += (size_t)written;
    }
}

size_t recursive_size(const Node *node) {
    const Node *child;
    size_t total = node && node->is_file ? node->size : 0;
    if (!node) return 0;
    for (child = node->child; child; child = child->sibling) total += recursive_size(child);
    return total;
}

static bool quota_allows(const Node *root, size_t current_size, size_t replacement_size) {
    size_t used = recursive_size(root);
    if (replacement_size <= current_size) return true;
    return used <= quota_limit && replacement_size - current_size <= quota_limit - used;
}

static void clear_cache(void) {
    memset(file_cache, 0, sizeof(file_cache));
    cache_count = 0;
}

static void cache_access(Node *node) {
    size_t i, slot = 0;
    char path[MAX_PATH];
    for (i = 0; i < cache_count; i++) {
        if (file_cache[i].node == node) {
            cache_hits++;
            file_cache[i].stamp = ++cache_clock;
            return;
        }
    }
    cache_misses++;
    if (cache_count < CACHE_CAPACITY) slot = cache_count++;
    else {
        for (i = 1; i < CACHE_CAPACITY; i++)
            if (file_cache[i].stamp < file_cache[slot].stamp) slot = i;
    }
    node_path(node, path, sizeof(path));
    file_cache[slot].node = node;
    strncpy(file_cache[slot].path, path, MAX_PATH - 1);
    file_cache[slot].path[MAX_PATH - 1] = '\0';
    file_cache[slot].stamp = ++cache_clock;
}

static void cmd_cache(void) {
    size_t i;
    printf("Hits: %zu\nMisses: %zu\nCached files:\n", cache_hits, cache_misses);
    for (i = 0; i < cache_count; i++) printf("  %s\n", file_cache[i].path);
}

static unsigned int hash_name(const char *name) {
    unsigned int hash = 5381u;
    while (*name) hash = ((hash << 5) + hash) ^ (unsigned char)*name++;
    return hash % INDEX_BUCKETS;
}

static void clear_search_index(void) {
    size_t i;
    for (i = 0; i < INDEX_BUCKETS; i++) {
        IndexEntry *entry = search_index[i];
        while (entry) { IndexEntry *next = entry->next; free(entry); entry = next; }
        search_index[i] = NULL;
    }
}

static void index_subtree(Node *node, bool inside_trash) {
    Node *child;
    IndexEntry *entry;
    if (!inside_trash && node->parent) {
        entry = malloc(sizeof(*entry));
        if (!entry) { perror("malloc"); exit(EXIT_FAILURE); }
        entry->node = node;
        entry->next = search_index[hash_name(node->name)];
        search_index[hash_name(node->name)] = entry;
    }
    for (child = node->child; child; child = child->sibling)
        index_subtree(child, inside_trash || (node->parent == NULL && strcmp(child->name, ".Trash") == 0));
}

static void rebuild_search_index(Node *root) {
    clear_search_index();
    if (root) index_subtree(root, false);
}

static void cmd_locate(const char *name) {
    IndexEntry *entry;
    bool found = false;
    if (!name || !*name) { printf("Usage: locate <filename>\n"); return; }
    for (entry = search_index[hash_name(name)]; entry; entry = entry->next) {
        char path[MAX_PATH];
        if (strcmp(entry->node->name, name) != 0) continue;
        node_path(entry->node, path, sizeof(path));
        printf("%s\n", path);
        found = true;
    }
    if (!found) printf("Not found\n");
}

bool cmd_mkdir(Node *cwd, const char *path, bool parents) {
    char parent_path[MAX_PATH], name[MAX_NAME], buffer[MAX_PATH], *part;
    Node *parent;
    if (!parents) {
        if (!split_parent_path(path, parent_path, name)) { printf("Invalid path\n"); return false; }
        parent = traverse(cwd, parent_path);
        if (!parent || parent->is_file) { printf("Invalid parent path\n"); return false; }
        if (find_child(parent, name)) { printf("Name exists\n"); return false; }
        add_child(parent, create_node(name, false));
        return true;
    }
    if (!path || !*path || strlen(path) >= sizeof(buffer)) { printf("Invalid path\n"); return false; }
    strcpy(buffer, path);
    parent = path[0] == '/' ? root_of(cwd) : cwd;
    for (part = strtok(buffer, "/"); part; part = strtok(NULL, "/")) {
        Node *child;
        if (strcmp(part, ".") == 0) continue;
        if (strcmp(part, "..") == 0) { if (parent->parent) parent = parent->parent; continue; }
        if (!is_valid_node_name(part)) { printf("Invalid directory name: %s\n", part); return false; }
        child = find_child(parent, part);
        if (!child) {
            child = create_node(part, false);
            if (!child) { printf("Invalid directory name\n"); return false; }
            add_child(parent, child);
        }
        if (child->is_file) { printf("Path component is a file: %s\n", part); return false; }
        parent = child;
    }
    return true;
}

bool cmd_touch(Node *cwd, const char *path) {
    char parent_path[MAX_PATH], name[MAX_NAME];
    Node *parent;
    if (!split_parent_path(path, parent_path, name)) { printf("Invalid path\n"); return false; }
    parent = traverse(cwd, parent_path);
    if (!parent || parent->is_file) { printf("Invalid parent\n"); return false; }
    if (find_child(parent, name)) { printf("Name exists\n"); return false; }
    add_child(parent, create_node(name, true));
    return true;
}

static bool cmd_symlink(Node *cwd, const char *target, const char *link_path) {
    char parent_path[MAX_PATH], name[MAX_NAME];
    Node *parent, *link;
    if (!split_parent_path(link_path, parent_path, name)) { printf("Invalid link path\n"); return false; }
    parent = traverse(cwd, parent_path);
    if (!parent || parent->is_file || parent->is_symlink || find_child(parent, name)) { printf("Invalid link destination\n"); return false; }
    link = create_node(name, false);
    link->is_symlink = true;
    link->link_target = malloc(strlen(target) + 1);
    if (!link->link_target) { perror("malloc"); free_tree(link); exit(EXIT_FAILURE); }
    strcpy(link->link_target, target);
    add_child(parent, link);
    return true;
}

bool cmd_write(Node *cwd, const char *path, const char *content, bool append) {
    Node *node = traverse(cwd, path);
    size_t available, new_size;
    if (!node) { printf("File not found\n"); return false; }
    if (!node->is_file) { printf("Path is a directory\n"); return false; }
    if (!has_write_permission(node)) { printf("Permission denied\n"); return false; }
    if (!append) new_size = strlen(content) < MAX_CONTENT ? strlen(content) : MAX_CONTENT - 1;
    else { available = MAX_CONTENT - node->size - 1; new_size = node->size + (strlen(content) < available ? strlen(content) : available); }
    if (!quota_allows(root_of(cwd), node->size, new_size)) { printf("Quota exceeded\n"); return false; }
    save_file_version(node);
    if (!append) { strncpy(node->content, content, MAX_CONTENT - 1); node->content[MAX_CONTENT - 1] = '\0'; }
    else { available = MAX_CONTENT - strlen(node->content) - 1; strncat(node->content, content, available); }
    node->size = strlen(node->content);
    touch_metadata(node);
    cache_access(node);
    return true;
}

bool cmd_read(Node *cwd, const char *path) {
    Node *node = traverse(cwd, path);
    if (!node) { printf("File not found\n"); return false; }
    if (!node->is_file) { printf("Path is a directory\n"); return false; }
    cache_access(node);
    printf("---- %s ----\n%s\n---- end ----\n", path, node->content);
    return true;
}

static void cmd_history(Node *cwd, const char *path) {
    Node *node = traverse(cwd, path);
    FileVersion *version;
    size_t count = 0, index = 0, i;
    FileVersion **items;
    if (!node || !node->is_file) { printf("File not found\n"); return; }
    for (version = node->versions; version; version = version->next) count++;
    if (count == 0) { printf("No previous versions\n"); return; }
    items = malloc(count * sizeof(*items));
    if (!items) { perror("malloc"); return; }
    for (version = node->versions; version; version = version->next) items[index++] = version;
    for (i = count; i > 0; i--) {
        char timestamp[32];
        struct tm *time_info = localtime(&items[i - 1]->timestamp);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);
        printf("Version %zu  %s  %zu bytes\n", count - i + 1, timestamp, items[i - 1]->size);
    }
    free(items);
}

static bool cmd_restoreversion(Node *cwd, const char *path, size_t requested_version) {
    Node *node = traverse(cwd, path);
    FileVersion *version;
    size_t count = 0, index = 0;
    FileVersion **items;
    if (!node || !node->is_file || requested_version == 0) { printf("Invalid file or version\n"); return false; }
    for (version = node->versions; version; version = version->next) count++;
    if (requested_version > count) { printf("Version not found\n"); return false; }
    items = malloc(count * sizeof(*items));
    if (!items) { perror("malloc"); return false; }
    for (version = node->versions; version; version = version->next) items[index++] = version;
    version = items[count - requested_version];
    if (!quota_allows(root_of(cwd), node->size, version->size)) { free(items); printf("Quota exceeded\n"); return false; }
    save_file_version(node);
    memcpy(node->content, version->content, version->size + 1);
    node->size = version->size;
    touch_metadata(node);
    cache_access(node);
    free(items);
    return true;
}

static Node *trash_directory(Node *root, bool create) {
    Node *trash = find_child(root, ".Trash");
    if (!trash && create) {
        trash = create_node(".Trash", false);
        trash->hidden = true;
        add_child(root, trash);
    }
    return trash && !trash->is_file && !trash->is_symlink ? trash : NULL;
}

bool cmd_rm(Node *cwd, const char *path, bool recursive) {
    Node *node = traverse(cwd, path);
    Node *trash;
    char parent_path[MAX_PATH];
    if (!node) { printf("Not found\n"); return false; }
    if (!node->parent) { printf("Cannot remove root\n"); return false; }
    if (!node->is_file && !recursive) { printf("Is a directory\n"); return false; }
    trash = trash_directory(root_of(cwd), true);
    if (!trash || node == trash) { printf("Cannot remove trash directory\n"); return false; }
    if (find_child(trash, node->name)) { printf("Trash already contains an entry named %s\n", node->name); return false; }
    node_path(node->parent, parent_path, sizeof(parent_path));
    detach_node(node);
    strncpy(node->original_path, parent_path, sizeof(node->original_path) - 1);
    node->original_path[sizeof(node->original_path) - 1] = '\0';
    add_child(trash, node);
    return true;
}

bool cmd_rmdir(Node *cwd, const char *path) {
    Node *node = traverse(cwd, path);
    if (!node) { printf("Not found\n"); return false; }
    if (node->is_file) { printf("Is a file\n"); return false; }
    if (node->child) { printf("Directory not empty\n"); return false; }
    return cmd_rm(cwd, path, true);
}

static void cmd_trash(Node *root) {
    Node *trash = trash_directory(root, false);
    Node *child;
    if (!trash || !trash->child) { printf("Trash is empty\n"); return; }
    for (child = trash->child; child; child = child->sibling)
        printf("%s\t(from %s)\n", child->name, child->original_path[0] ? child->original_path : "/");
}

static bool cmd_restore(Node *root, const char *name) {
    Node *trash = trash_directory(root, false);
    Node *node, *parent;
    if (!trash || !(node = find_child(trash, name))) { printf("Trash entry not found\n"); return false; }
    parent = traverse(root, node->original_path[0] ? node->original_path : "/");
    if (!parent || parent->is_file || parent->is_symlink) parent = root;
    if (find_child(parent, node->name)) { printf("Cannot restore: name exists at destination\n"); return false; }
    detach_node(node);
    node->original_path[0] = '\0';
    add_child(parent, node);
    return true;
}

static bool cmd_emptytrash(Node *root) {
    Node *trash = trash_directory(root, false);
    Node *child, *next;
    if (!trash) return true;
    for (child = trash->child; child; child = next) {
        next = child->sibling;
        child->parent = child->sibling = NULL;
        free_tree(child);
    }
    trash->child = NULL;
    touch_metadata(trash);
    return true;
}

Node *copy_node(const Node *source) {
    const Node *child;
    Node *copy = create_node(source->name, source->is_file);
    copy->permissions = source->permissions;
    copy->hidden = source->hidden;
    copy->is_symlink = source->is_symlink;
    if (source->link_target) {
        copy->link_target = malloc(strlen(source->link_target) + 1);
        if (!copy->link_target) { perror("malloc"); free_tree(copy); exit(EXIT_FAILURE); }
        strcpy(copy->link_target, source->link_target);
    }
    copy->versions = clone_versions(source->versions);
    if (source->is_file) {
        memcpy(copy->content, source->content, MAX_CONTENT);
        copy->size = source->size;
        copy->modified_at = source->modified_at;
    }
    for (child = source->child; child; child = child->sibling) add_child(copy, copy_node(child));
    return copy;
}

bool cmd_cp(Node *cwd, const char *source_path, const char *destination_path) {
    Node *source = traverse(cwd, source_path), *destination = traverse(cwd, destination_path), *parent;
    char parent_path[MAX_PATH], name[MAX_NAME];
    if (!source) { printf("Source not found\n"); return false; }
    if (!quota_allows(root_of(cwd), 0, recursive_size(source))) { printf("Quota exceeded\n"); return false; }
    if (destination) { if (destination->is_file) { printf("Destination exists\n"); return false; } parent = destination; strncpy(name, source->name, MAX_NAME); }
    else { if (!split_parent_path(destination_path, parent_path, name)) { printf("Invalid destination\n"); return false; } parent = traverse(cwd, parent_path); }
    if (!parent || parent->is_file) { printf("Invalid destination\n"); return false; }
    if (find_child(parent, name)) { printf("Name exists\n"); return false; }
    { Node *copy = copy_node(source); strncpy(copy->name, name, MAX_NAME - 1); copy->name[MAX_NAME - 1] = '\0'; copy->hidden = copy->name[0] == '.'; add_child(parent, copy); }
    return true;
}

bool cmd_mv(Node *cwd, const char *source_path, const char *destination_path) {
    Node *source = traverse(cwd, source_path), *destination = traverse(cwd, destination_path), *parent;
    char parent_path[MAX_PATH], name[MAX_NAME];
    if (!source || !source->parent) { printf("Source not found or is root\n"); return false; }
    if (destination) { if (destination->is_file) { printf("Destination exists\n"); return false; } parent = destination; strncpy(name, source->name, MAX_NAME); }
    else { if (!split_parent_path(destination_path, parent_path, name)) { printf("Invalid destination\n"); return false; } parent = traverse(cwd, parent_path); }
    if (!parent || parent->is_file || is_descendant(source, parent)) { printf("Invalid destination\n"); return false; }
    if (find_child(parent, name) && !(parent == source->parent && strcmp(name, source->name) == 0)) { printf("Name exists\n"); return false; }
    detach_node(source);
    strncpy(source->name, name, MAX_NAME - 1); source->name[MAX_NAME - 1] = '\0';
    source->hidden = source->name[0] == '.';
    add_child(parent, source);
    touch_metadata(source);
    return true;
}

int compare_nodes(const void *left, const void *right) {
    const Node *const *a = left, *const *b = right;
    return strcmp((*a)->name, (*b)->name);
}

void print_permissions(unsigned int permissions) {
    const char chars[] = "rwx";
    for (int i = 8; i >= 0; --i) putchar((permissions & (1u << i)) ? chars[(8 - i) % 3] : '-');
}

void print_long_entry(const Node *node) {
    char created[32], modified[32];
    struct tm *tm_info;
    print_permissions(node->permissions);
    tm_info = localtime(&node->created_at); strftime(created, sizeof(created), "%Y-%m-%d %H:%M", tm_info);
    tm_info = localtime(&node->modified_at); strftime(modified, sizeof(modified), "%Y-%m-%d %H:%M", tm_info);
    printf(" %8zu  %s  %s  %s", node->is_file ? node->size : recursive_size(node), created, modified, node->name);
    if (node->is_symlink) printf(" -> %s", node->link_target);
    else if (!node->is_file) printf("/");
    printf("\n");
}

void cmd_ls(Node *cwd, const char *path, bool show_all, bool long_format) {
    Node *node = traverse(cwd, path), *child;
    Node **entries;
    size_t count = 0, index = 0;
    if (!node) { printf("Not found\n"); return; }
    if (node->is_file) { if (long_format) print_long_entry(node); else printf("%s\n", node->name); return; }
    for (child = node->child; child; child = child->sibling) if (show_all || !child->hidden) count++;
    entries = count ? malloc(count * sizeof(*entries)) : NULL;
    if (count && !entries) { perror("malloc"); return; }
    for (child = node->child; child; child = child->sibling) if (show_all || !child->hidden) entries[index++] = child;
    qsort(entries, count, sizeof(*entries), compare_nodes);
    for (index = 0; index < count; index++) {
        if (long_format) print_long_entry(entries[index]);
        else printf("%s%s%s", entries[index]->name, entries[index]->is_file ? "" : "/", index + 1 == count ? "\n" : "\t");
    }
    if (count == 0 && !long_format) printf("\n");
    free(entries);
}

bool wildcard_match(const char *pattern, const char *text) {
    if (*pattern == '\0') return *text == '\0';
    if (*pattern == '*') return wildcard_match(pattern + 1, text) || (*text && wildcard_match(pattern, text + 1));
    if (*pattern == '?' && *text) return wildcard_match(pattern + 1, text + 1);
    return *pattern == *text && wildcard_match(pattern + 1, text + 1);
}

void find_recursive(const Node *node, const char *pattern) {
    const Node *child;
    char path[MAX_PATH];
    for (child = node->child; child; child = child->sibling) {
        if (wildcard_match(pattern, child->name)) { node_path(child, path, sizeof(path)); printf("%s\n", path); }
        if (!child->is_file) find_recursive(child, pattern);
    }
}

void cmd_find(Node *cwd, const char *pattern) { find_recursive(cwd, pattern); }

void cmd_du(Node *cwd, const char *path) {
    Node *node = traverse(cwd, path);
    char full_path[MAX_PATH];
    if (!node) { printf("Not found\n"); return; }
    node_path(node, full_path, sizeof(full_path));
    printf("%zu\t%s\n", recursive_size(node), full_path);
}

void count_recursive(const Node *node, Counts *counts) {
    const Node *child;
    for (child = node->child; child; child = child->sibling) {
        if (child->is_file) { counts->files++; counts->size += child->size; }
        else { counts->directories++; count_recursive(child, counts); }
    }
}

void cmd_count(Node *cwd) {
    Counts counts = {0, 0, 0};
    count_recursive(cwd, &counts);
    printf("Total files: %zu\nTotal directories: %zu\nTotal size: %zu\n", counts.files, counts.directories, counts.size);
}

static size_t maximum_depth(const Node *node, size_t depth) {
    const Node *child;
    size_t maximum = depth;
    for (child = node->child; child; child = child->sibling) {
        size_t child_depth = maximum_depth(child, depth + 1);
        if (child_depth > maximum) maximum = child_depth;
    }
    return maximum;
}

void cmd_fsinfo(Node *cwd) {
    Counts counts = {0, 0, 0};
    Node *root = root_of(cwd);
    char path[MAX_PATH];
    count_recursive(root, &counts);
    node_path(cwd, path, sizeof(path));
    printf("Total files: %zu\nTotal directories: %zu\nTotal size: %zu\nMaximum depth: %zu\n"
           "Current path: %s\nAutosave status: %s\nJournal entries count: %zu\n",
           counts.files, counts.directories, counts.size, maximum_depth(root, 0), path,
           file_exists(AUTOSAVE_FILE) ? "available" : "not available", journal_entry_count());
}

bool cmd_chmod(Node *cwd, const char *mode_text, const char *path) {
    char *end;
    long mode = strtol(mode_text, &end, 8);
    Node *node;
    if (*mode_text == '\0' || *end != '\0' || mode < 0 || mode > 0777) { printf("Invalid mode\n"); return false; }
    node = traverse(cwd, path);
    if (!node) { printf("Not found\n"); return false; }
    node->permissions = (unsigned int)mode;
    touch_metadata(node);
    return true;
}

static void cmd_quota(const char *text) {
    char *end;
    unsigned long long value;
    if (!text) { printf("Invalid quota\n"); return; }
    value = strtoull(text, &end, 10);
    if (!*text || *end != '\0' || value > SIZE_MAX) { printf("Invalid quota\n"); return; }
    quota_limit = (size_t)value;
    printf("Quota set to %zu bytes\n", quota_limit);
}

static void cmd_quotainfo(Node *root) {
    size_t used = recursive_size(root);
    if (quota_limit == SIZE_MAX) printf("Quota: unlimited\nUsed: %zu bytes\n", used);
    else printf("Quota: %zu bytes\nUsed: %zu bytes\nAvailable: %zu bytes\n", quota_limit, used, used <= quota_limit ? quota_limit - used : 0);
}

static size_t fsck_node(Node *node, Node *expected_parent) {
    Node *child, *other;
    size_t problems = 0;
    if (!node || node->parent != expected_parent) return 1;
    if (node->is_file && (node->child || !node->content || node->size != strlen(node->content))) problems++;
    if (node->is_symlink && (node->is_file || node->child || !node->link_target || !traverse(node->parent, node->link_target))) problems++;
    if (node->hidden != (node->name[0] == '.')) problems++;
    if (node->created_at > node->modified_at) problems++;
    for (child = node->child; child; child = child->sibling) {
        if (child->parent != node) problems++;
        for (other = child->sibling; other; other = other->sibling)
            if (strcmp(child->name, other->name) == 0) problems++;
        problems += fsck_node(child, node);
    }
    return problems;
}

static void cmd_fsck(Node *root) {
    size_t problems = fsck_node(root, NULL);
    if (quota_limit != SIZE_MAX && recursive_size(root) > quota_limit) problems++;
    if (problems) printf("fsck: %zu problem(s) found\n", problems);
    else printf("fsck: filesystem is consistent\n");
}

static bool cmd_undo(Node **root, Node **cwd) {
    Node *previous = pop_snapshot(&undo_stack);
    if (!previous) { printf("Nothing to undo\n"); return false; }
    push_snapshot(&redo_stack, *root);
    *root = previous;
    *cwd = previous;
    record_mutation(*root, "UNDO", "/");
    return true;
}

static bool cmd_redo(Node **root, Node **cwd) {
    Node *next = pop_snapshot(&redo_stack);
    if (!next) { printf("Nothing to redo\n"); return false; }
    push_snapshot(&undo_stack, *root);
    *root = next;
    *cwd = next;
    record_mutation(*root, "REDO", "/");
    return true;
}

void cmd_pwd(Node *cwd) { char path[MAX_PATH]; node_path(cwd, path, sizeof(path)); printf("%s\n", path); }

Node *cmd_cd(Node *cwd, const char *path) {
    Node *node = traverse(cwd, path);
    if (!node) { printf("No such path\n"); return cwd; }
    if (node->is_file) { printf("Not a directory\n"); return cwd; }
    return node;
}

void print_tree(const Node *node, int depth) {
    const Node *child;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s", node->parent ? node->name : "/");
    if (node->is_symlink) printf(" -> %s", node->link_target);
    else if (node->parent && !node->is_file) printf("/");
    printf("\n");
    for (child = node->child; child; child = child->sibling) print_tree(child, depth + 1);
}

void print_help(void) {
    printf("Commands:\n"
           "  mkdir [-p] <path>          Create a directory (and parents with -p)\n"
           "  touch <path>               Create an empty file\n"
           "  write <path> <text>        Replace file content\n"
           "  append <path> <text>       Append file content\n"
           "  history <file>             Show previous file versions\n"
           "  restoreversion <file> <n>  Restore a previous file version\n"
           "  read <path>                Display file content\n"
           "  ls [-a] [-l] [path]        List entries (hidden/long formats)\n"
           "  mv <source> <destination>  Move or rename a file/directory\n"
           "  rename <old> <new>         Rename a file or directory\n"
           "  cp <source> <destination>  Deep-copy a file or directory\n"
           "  ln -s <target> <linkname>  Create a symbolic link\n"
           "  rm [-r] <path>             Remove a file or directory recursively\n"
           "  rmdir <path>               Remove an empty directory\n"
           "  find <pattern>             Recursively find names (supports * and ?)\n"
           "  chmod <mode> <path>        Set permissions, e.g. chmod 644 file\n"
           "  du [path]                  Display recursive size\n"
           "  count                      Display file, directory, and size totals\n"
           "  quota <bytes>              Set the storage quota\n"
           "  quotainfo                  Display quota usage\n"
           "  cache | clearcache          Show or clear the file cache\n"
           "  locate <filename>          Find indexed matching names\n"
           "  trash | restore <name>     View or restore deleted entries\n"
           "  emptytrash                 Permanently clear the trash\n"
           "  undo | redo                Reverse or reapply a modification\n"
           "  fsck                       Check filesystem integrity\n"
           "  fsinfo                     Display filesystem status and statistics\n"
           "  save <filename>            Save the filesystem to a binary file\n"
           "  load <filename>            Load and validate a saved filesystem\n"
           "  cd [path]                  Change directory (no path means root)\n"
           "  pwd                        Print current directory\n"
           "  tree                       Print directory tree\n"
           "  clear                      Clear the console\n"
           "  help                       Show this help\n"
           "  exit                       Exit MiniFS\n");
}

void strip_newline(char *text) { char *newline = strchr(text, '\n'); if (newline) *newline = '\0'; }

int main(void) {
    Node *root = create_node("", false), *cwd = root;
    char line[4096];
    if (file_exists(AUTOSAVE_FILE)) {
        if (load_filesystem(AUTOSAVE_FILE, &root)) {
            cwd = root;
            printf("Autosave restored.\n");
        } else {
            printf("Warning: autosave is invalid and was ignored.\n");
        }
    }
    rebuild_search_index(root);
    printf("MiniFS ready. Type help for commands.\n");
    while (1) {
        char *cmd, *arg;
        const char *operation = NULL;
        const char *target = NULL;
        Node *before = NULL;
        bool changed = false;
        if (cwd->parent) printf("%s/> ", cwd->name);
        else printf("/> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        strip_newline(line);
        cmd = strtok(line, " ");
        if (!cmd) continue;
        if (strcmp(cmd, "mkdir") == 0 || strcmp(cmd, "touch") == 0 || strcmp(cmd, "write") == 0 ||
            strcmp(cmd, "append") == 0 || strcmp(cmd, "rm") == 0 || strcmp(cmd, "rmdir") == 0 ||
            strcmp(cmd, "mv") == 0 || strcmp(cmd, "rename") == 0 || strcmp(cmd, "cp") == 0 ||
            strcmp(cmd, "chmod") == 0 || strcmp(cmd, "load") == 0 || strcmp(cmd, "restore") == 0 ||
            strcmp(cmd, "emptytrash") == 0 || strcmp(cmd, "restoreversion") == 0 || strcmp(cmd, "ln") == 0)
            before = clone_tree(root);
        if (strcmp(cmd, "help") == 0) print_help();
        else if (strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "clear") == 0) { printf("\033[2J\033[H"); fflush(stdout); }
        else if (strcmp(cmd, "mkdir") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: mkdir [-p] <path>\n"); else { target = strncmp(arg, "-p ", 3) == 0 ? arg + 3 : arg; changed = cmd_mkdir(cwd, target, strncmp(arg, "-p ", 3) == 0); operation = "MKDIR"; } }
        else if (strcmp(cmd, "touch") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: touch <path>\n"); else { changed = cmd_touch(cwd, arg); operation = "TOUCH"; target = arg; } }
        else if (strcmp(cmd, "write") == 0 || strcmp(cmd, "append") == 0) { char *path = strtok(NULL, " "), *text = strtok(NULL, ""); if (!path || !text) printf("Usage: %s <path> <text>\n", cmd); else { changed = cmd_write(cwd, path, text, strcmp(cmd, "append") == 0); operation = strcmp(cmd, "append") == 0 ? "APPEND" : "WRITE"; target = path; } }
        else if (strcmp(cmd, "read") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: read <path>\n"); else cmd_read(cwd, arg); }
        else if (strcmp(cmd, "history") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: history <file>\n"); else cmd_history(cwd, arg); }
        else if (strcmp(cmd, "restoreversion") == 0) { char *path = strtok(NULL, " "), *number = strtok(NULL, " "), *end; unsigned long value; if (!path || !number || strtok(NULL, " ")) printf("Usage: restoreversion <file> <version>\n"); else { value = strtoul(number, &end, 10); if (*end) printf("Invalid version\n"); else { changed = cmd_restoreversion(cwd, path, (size_t)value); operation = "RESTOREVERSION"; target = path; } } }
        else if (strcmp(cmd, "ls") == 0) { bool all = false, long_format = false; char *token; arg = strtok(NULL, " "); while (arg && arg[0] == '-') { for (token = arg + 1; *token; token++) { if (*token == 'a') all = true; else if (*token == 'l') long_format = true; else printf("Unknown ls option: -%c\n", *token); } arg = strtok(NULL, " "); } cmd_ls(cwd, arg ? arg : "", all, long_format); }
        else if (strcmp(cmd, "rm") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: rm [-r] <path>\n"); else { target = strncmp(arg, "-r ", 3) == 0 ? arg + 3 : arg; changed = cmd_rm(cwd, target, strncmp(arg, "-r ", 3) == 0); operation = "RM"; } }
        else if (strcmp(cmd, "rmdir") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: rmdir <path>\n"); else { changed = cmd_rmdir(cwd, arg); operation = "RMDIR"; target = arg; } }
        else if (strcmp(cmd, "mv") == 0 || strcmp(cmd, "rename") == 0 || strcmp(cmd, "cp") == 0) { char *source = strtok(NULL, " "), *destination = strtok(NULL, " "); if (!source || !destination || strtok(NULL, " ")) printf("Usage: %s <source> <destination>\n", cmd); else { changed = strcmp(cmd, "cp") == 0 ? cmd_cp(cwd, source, destination) : cmd_mv(cwd, source, destination); operation = strcmp(cmd, "cp") == 0 ? "CP" : (strcmp(cmd, "rename") == 0 ? "RENAME" : "MV"); target = destination; } }
        else if (strcmp(cmd, "find") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: find <pattern>\n"); else cmd_find(cwd, arg); }
        else if (strcmp(cmd, "chmod") == 0) { char *mode = strtok(NULL, " "), *path = strtok(NULL, " "); if (!mode || !path || strtok(NULL, " ")) printf("Usage: chmod <mode> <path>\n"); else { changed = cmd_chmod(cwd, mode, path); operation = "CHMOD"; target = path; } }
        else if (strcmp(cmd, "du") == 0) { arg = strtok(NULL, ""); cmd_du(cwd, arg ? arg : ""); }
        else if (strcmp(cmd, "count") == 0) cmd_count(cwd);
        else if (strcmp(cmd, "quota") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: quota <bytes>\n"); else cmd_quota(arg); }
        else if (strcmp(cmd, "quotainfo") == 0) cmd_quotainfo(root);
        else if (strcmp(cmd, "cache") == 0) cmd_cache();
        else if (strcmp(cmd, "clearcache") == 0) clear_cache();
        else if (strcmp(cmd, "locate") == 0) { arg = strtok(NULL, ""); cmd_locate(arg); }
        else if (strcmp(cmd, "fsck") == 0) cmd_fsck(root);
        else if (strcmp(cmd, "trash") == 0) cmd_trash(root);
        else if (strcmp(cmd, "restore") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: restore <name>\n"); else { changed = cmd_restore(root, arg); operation = "RESTORE"; target = arg; } }
        else if (strcmp(cmd, "emptytrash") == 0) { changed = cmd_emptytrash(root); operation = "EMPTYTRASH"; target = ".Trash"; }
        else if (strcmp(cmd, "ln") == 0) { char *option = strtok(NULL, " "), *link_target = strtok(NULL, " "), *link_name = strtok(NULL, " "); if (!option || !link_target || !link_name || strcmp(option, "-s") != 0 || strtok(NULL, " ")) printf("Usage: ln -s <target> <linkname>\n"); else { changed = cmd_symlink(cwd, link_target, link_name); operation = "SYMLINK"; target = link_name; } }
        else if (strcmp(cmd, "undo") == 0) { free_tree(before); before = NULL; cmd_undo(&root, &cwd); }
        else if (strcmp(cmd, "redo") == 0) { free_tree(before); before = NULL; cmd_redo(&root, &cwd); }
        else if (strcmp(cmd, "fsinfo") == 0) cmd_fsinfo(cwd);
        else if (strcmp(cmd, "save") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: save <filename>\n"); else if (save_filesystem(root, arg)) printf("Filesystem saved.\n"); else printf("Save failed.\n"); }
        else if (strcmp(cmd, "load") == 0) { arg = strtok(NULL, ""); if (!arg) printf("Usage: load <filename>\n"); else if (load_filesystem(arg, &root)) { cwd = root; changed = true; operation = "LOAD"; target = arg; printf("Filesystem loaded.\n"); } else printf("Load failed: invalid or unreadable file.\n"); }
        else if (strcmp(cmd, "cd") == 0) { arg = strtok(NULL, ""); cwd = arg ? cmd_cd(cwd, arg) : root; }
        else if (strcmp(cmd, "pwd") == 0) cmd_pwd(cwd);
        else if (strcmp(cmd, "tree") == 0) print_tree(root, 0);
        else printf("Unknown command. Type help.\n");
        if (changed) {
            if (before) { push_snapshot(&undo_stack, before); clear_snapshot_stack(&redo_stack); }
            record_mutation(root, operation, target);
        } else free_tree(before);
    }
    clear_snapshot_stack(&undo_stack);
    clear_snapshot_stack(&redo_stack);
    clear_search_index();
    clear_cache();
    free_tree(root);
    printf("Bye.\n");
    return 0;
}
