/*
 * Callable API adapter for the existing MiniFS C engine.
 * Compile with Emscripten using scripts/build-wasm.sh.
 * The original mini_fs.c remains the sole implementation of filesystem rules.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif

#define MINIFS_OUTPUT_CAPACITY (256u * 1024u)
static char minifs_output[MINIFS_OUTPUT_CAPACITY];
static size_t minifs_output_length = 0;
int minifs_printf(const char *format, ...);

/* Redirect the included engine's console output into a browser-readable buffer. */
#define printf minifs_printf
#define main minifs_native_cli_main
#include "../mini_fs.c"
#undef main
#undef printf

static Node *wasm_root = NULL;
static Node *wasm_cwd = NULL;
static bool wasm_initialized = false;

int minifs_printf(const char *format, ...) {
    va_list args;
    int needed;
    size_t available;
    if (minifs_output_length >= MINIFS_OUTPUT_CAPACITY - 1) return 0;
    available = MINIFS_OUTPUT_CAPACITY - minifs_output_length;
    va_start(args, format);
    needed = vsnprintf(minifs_output + minifs_output_length, available, format, args);
    va_end(args);
    if (needed < 0) return needed;
    if ((size_t)needed >= available) minifs_output_length = MINIFS_OUTPUT_CAPACITY - 1;
    else minifs_output_length += (size_t)needed;
    return needed;
}

static void clear_output(void) {
    minifs_output_length = 0;
    minifs_output[0] = '\0';
}

static void ensure_initialized(void) {
    if (!wasm_initialized) {
        wasm_root = create_node("", false);
        wasm_cwd = wasm_root;
        rebuild_search_index(wasm_root);
        wasm_initialized = true;
    }
}

EMSCRIPTEN_KEEPALIVE
void minifs_init(void) {
    if (wasm_root) free_tree(wasm_root);
    clear_snapshot_stack(&undo_stack);
    clear_snapshot_stack(&redo_stack);
    clear_search_index();
    clear_cache();
    memset(file_cache, 0, sizeof(file_cache));
    cache_count = 0;
    cache_clock = cache_hits = cache_misses = 0;
    quota_limit = SIZE_MAX;
    wasm_root = create_node("", false);
    wasm_cwd = wasm_root;
    wasm_initialized = true;
    rebuild_search_index(wasm_root);
    clear_output();
    minifs_printf("MiniFS C engine ready.\n");
}

static void json_string(const char *value) {
    const unsigned char *p = (const unsigned char *)(value ? value : "");
    minifs_printf("\"");
    while (*p) {
        switch (*p) {
            case '"': minifs_printf("\\\""); break;
            case '\\': minifs_printf("\\\\"); break;
            case '\n': minifs_printf("\\n"); break;
            case '\r': minifs_printf("\\r"); break;
            case '\t': minifs_printf("\\t"); break;
            default:
                if (*p < 32) minifs_printf("\\u%04x", (unsigned int)*p);
                else minifs_printf("%c", *p);
        }
        p++;
    }
    minifs_printf("\"");
}

static void export_node_json(const Node *node, bool *first) {
    const Node *child;
    char path[MAX_PATH];
    bool child_first = true;
    if (!node) return;
    node_path(node, path, sizeof(path));
    if (!*first) minifs_printf(",");
    *first = false;
    minifs_printf("{\"name\":"); json_string(node->parent ? node->name : "/");
    minifs_printf(",\"path\":"); json_string(path);
    minifs_printf(",\"type\":\"%s\",\"size\":%zu,\"permissions\":%u,\"symlink\":%s,\"target\":",
        node->is_file ? "file" : "dir", node->size, node->permissions, node->is_symlink ? "true" : "false");
    json_string(node->link_target);
    minifs_printf(",\"content\":"); json_string(node->is_file && node->content ? node->content : "");
    minifs_printf(",\"children\":[");
    for (child = node->child; child; child = child->sibling) export_node_json(child, &child_first);
    minifs_printf("]}");
}

EMSCRIPTEN_KEEPALIVE
const char *minifs_get_output(void) {
    return minifs_output;
}

EMSCRIPTEN_KEEPALIVE
const char *minifs_export_tree_json(void) {
    bool first = true;
    ensure_initialized();
    clear_output();
    export_node_json(wasm_root, &first);
    return minifs_output;
}

EMSCRIPTEN_KEEPALIVE
const char *minifs_get_cwd(void) {
    static char path[MAX_PATH];
    ensure_initialized();
    node_path(wasm_cwd, path, sizeof(path));
    return path;
}

EMSCRIPTEN_KEEPALIVE
const char *minifs_execute(const char *input) {
    char line[4096];
    char *cmd, *arg;
    const char *operation = NULL, *target = NULL;
    Node *before = NULL;
    bool changed = false;
    ensure_initialized();
    clear_output();
    if (!input) { minifs_printf("Error: command is empty.\n"); return minifs_output; }
    if (strlen(input) >= sizeof(line)) { minifs_printf("Error: command is too long.\n"); return minifs_output; }
    strcpy(line, input);
    strip_newline(line);
    cmd = strtok(line, " ");
    if (!cmd) return minifs_output;
    if (strcmp(cmd, "mkdir") == 0 || strcmp(cmd, "touch") == 0 || strcmp(cmd, "write") == 0 ||
        strcmp(cmd, "append") == 0 || strcmp(cmd, "rm") == 0 || strcmp(cmd, "rmdir") == 0 ||
        strcmp(cmd, "mv") == 0 || strcmp(cmd, "rename") == 0 || strcmp(cmd, "cp") == 0 ||
        strcmp(cmd, "chmod") == 0 || strcmp(cmd, "load") == 0 || strcmp(cmd, "restore") == 0 ||
        strcmp(cmd, "emptytrash") == 0 || strcmp(cmd, "restoreversion") == 0 || strcmp(cmd, "ln") == 0)
        before = clone_tree(wasm_root);

    if (strcmp(cmd, "help") == 0) print_help();
    else if (strcmp(cmd, "exit") == 0) minifs_printf("Use the page controls to reset the browser session.\n");
    else if (strcmp(cmd, "clear") == 0) minifs_printf("Terminal cleared.\n");
    else if (strcmp(cmd, "mkdir") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: mkdir [-p] <path>\n"); else { target = strncmp(arg, "-p ", 3) == 0 ? arg + 3 : arg; changed = cmd_mkdir(wasm_cwd, target, strncmp(arg, "-p ", 3) == 0); operation = "MKDIR"; } }
    else if (strcmp(cmd, "touch") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: touch <path>\n"); else { changed = cmd_touch(wasm_cwd, arg); operation = "TOUCH"; target = arg; } }
    else if (strcmp(cmd, "write") == 0 || strcmp(cmd, "append") == 0) { char *path = strtok(NULL, " "), *text = strtok(NULL, ""); if (!path || !text) minifs_printf("Usage: %s <path> <text>\n", cmd); else { changed = cmd_write(wasm_cwd, path, text, strcmp(cmd, "append") == 0); operation = strcmp(cmd, "append") == 0 ? "APPEND" : "WRITE"; target = path; } }
    else if (strcmp(cmd, "read") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: read <path>\n"); else cmd_read(wasm_cwd, arg); }
    else if (strcmp(cmd, "history") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: history <file>\n"); else cmd_history(wasm_cwd, arg); }
    else if (strcmp(cmd, "restoreversion") == 0) { char *path = strtok(NULL, " "), *number = strtok(NULL, " "), *end; unsigned long value; if (!path || !number || strtok(NULL, " ")) minifs_printf("Usage: restoreversion <file> <version>\n"); else { value = strtoul(number, &end, 10); if (*end) minifs_printf("Invalid version\n"); else { changed = cmd_restoreversion(wasm_cwd, path, (size_t)value); operation = "RESTOREVERSION"; target = path; } } }
    else if (strcmp(cmd, "ls") == 0) { bool all = false, long_format = false; char *token; arg = strtok(NULL, " "); while (arg && arg[0] == '-') { for (token = arg + 1; *token; token++) { if (*token == 'a') all = true; else if (*token == 'l') long_format = true; else minifs_printf("Unknown ls option: -%c\n", *token); } arg = strtok(NULL, " "); } cmd_ls(wasm_cwd, arg ? arg : "", all, long_format); }
    else if (strcmp(cmd, "rm") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: rm [-r] <path>\n"); else { target = strncmp(arg, "-r ", 3) == 0 ? arg + 3 : arg; changed = cmd_rm(wasm_cwd, target, strncmp(arg, "-r ", 3) == 0); operation = "RM"; } }
    else if (strcmp(cmd, "rmdir") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: rmdir <path>\n"); else { changed = cmd_rmdir(wasm_cwd, arg); operation = "RMDIR"; target = arg; } }
    else if (strcmp(cmd, "mv") == 0 || strcmp(cmd, "rename") == 0 || strcmp(cmd, "cp") == 0) { char *source = strtok(NULL, " "), *destination = strtok(NULL, " "); if (!source || !destination || strtok(NULL, " ")) minifs_printf("Usage: %s <source> <destination>\n", cmd); else { changed = strcmp(cmd, "cp") == 0 ? cmd_cp(wasm_cwd, source, destination) : cmd_mv(wasm_cwd, source, destination); operation = strcmp(cmd, "cp") == 0 ? "CP" : (strcmp(cmd, "rename") == 0 ? "RENAME" : "MV"); target = destination; } }
    else if (strcmp(cmd, "find") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: find <pattern>\n"); else cmd_find(wasm_cwd, arg); }
    else if (strcmp(cmd, "chmod") == 0) { char *mode = strtok(NULL, " "), *path = strtok(NULL, " "); if (!mode || !path || strtok(NULL, " ")) minifs_printf("Usage: chmod <mode> <path>\n"); else { changed = cmd_chmod(wasm_cwd, mode, path); operation = "CHMOD"; target = path; } }
    else if (strcmp(cmd, "du") == 0) { arg = strtok(NULL, ""); cmd_du(wasm_cwd, arg ? arg : ""); }
    else if (strcmp(cmd, "count") == 0) cmd_count(wasm_cwd);
    else if (strcmp(cmd, "quota") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: quota <bytes>\n"); else cmd_quota(arg); }
    else if (strcmp(cmd, "quotainfo") == 0) cmd_quotainfo(wasm_root);
    else if (strcmp(cmd, "cache") == 0) cmd_cache();
    else if (strcmp(cmd, "clearcache") == 0) clear_cache();
    else if (strcmp(cmd, "locate") == 0) { arg = strtok(NULL, ""); cmd_locate(arg); }
    else if (strcmp(cmd, "fsck") == 0) cmd_fsck(wasm_root);
    else if (strcmp(cmd, "trash") == 0) cmd_trash(wasm_root);
    else if (strcmp(cmd, "restore") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: restore <name>\n"); else { changed = cmd_restore(wasm_root, arg); operation = "RESTORE"; target = arg; } }
    else if (strcmp(cmd, "emptytrash") == 0) { changed = cmd_emptytrash(wasm_root); operation = "EMPTYTRASH"; target = ".Trash"; }
    else if (strcmp(cmd, "ln") == 0) { char *option = strtok(NULL, " "), *link_target = strtok(NULL, " "), *link_name = strtok(NULL, " "); if (!option || !link_target || !link_name || strcmp(option, "-s") != 0 || strtok(NULL, " ")) minifs_printf("Usage: ln -s <target> <linkname>\n"); else { changed = cmd_symlink(wasm_cwd, link_target, link_name); operation = "SYMLINK"; target = link_name; } }
    else if (strcmp(cmd, "undo") == 0) { free_tree(before); before = NULL; cmd_undo(&wasm_root, &wasm_cwd); }
    else if (strcmp(cmd, "redo") == 0) { free_tree(before); before = NULL; cmd_redo(&wasm_root, &wasm_cwd); }
    else if (strcmp(cmd, "fsinfo") == 0) cmd_fsinfo(wasm_cwd);
    else if (strcmp(cmd, "save") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: save <filename>\n"); else if (save_filesystem(wasm_root, arg)) minifs_printf("Filesystem saved.\n"); else minifs_printf("Save failed.\n"); }
    else if (strcmp(cmd, "load") == 0) { arg = strtok(NULL, ""); if (!arg) minifs_printf("Usage: load <filename>\n"); else if (load_filesystem(arg, &wasm_root)) { wasm_cwd = wasm_root; changed = true; operation = "LOAD"; target = arg; minifs_printf("Filesystem loaded.\n"); } else minifs_printf("Load failed: invalid or unreadable file.\n"); }
    else if (strcmp(cmd, "cd") == 0) { arg = strtok(NULL, ""); wasm_cwd = arg ? cmd_cd(wasm_cwd, arg) : wasm_root; }
    else if (strcmp(cmd, "pwd") == 0) cmd_pwd(wasm_cwd);
    else if (strcmp(cmd, "tree") == 0) print_tree(wasm_root, 0);
    else minifs_printf("Unknown command. Type help.\n");
    if (changed) {
        if (before) { push_snapshot(&undo_stack, before); clear_snapshot_stack(&redo_stack); }
        record_mutation(wasm_root, operation, target);
        rebuild_search_index(wasm_root);
    } else free_tree(before);
    return minifs_output;
}

EMSCRIPTEN_KEEPALIVE
void minifs_free(void) {
    if (wasm_root) free_tree(wasm_root);
    wasm_root = NULL; wasm_cwd = NULL; wasm_initialized = false;
    clear_snapshot_stack(&undo_stack); clear_snapshot_stack(&redo_stack);
    clear_search_index(); clear_cache(); clear_output();
}
