#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void minifs_init(void);
const char *minifs_execute(const char *input);
const char *minifs_export_tree_json(void);
const char *minifs_get_cwd(void);
void minifs_free(void);

static int expect_contains(const char *label, const char *actual, const char *expected) {
    if (!actual || !strstr(actual, expected)) {
        fprintf(stderr, "FAIL %s: expected [%s] in [%s]\n", label, expected, actual ? actual : "(null)");
        return 1;
    }
    printf("PASS %s\n", label);
    return 0;
}

int main(void) {
    int failures = 0;
    const char *out, *tree;
    minifs_init();
    out = minifs_execute("mkdir docs");
    failures += expect_contains("mkdir command output buffer", out, "");
    failures += expect_contains("mkdir command changes tree", minifs_export_tree_json(), "\"name\":\"docs\"");
    out = minifs_execute("touch docs/notes.txt");
    failures += expect_contains("touch command", out, "");
    out = minifs_execute("write docs/notes.txt WebAssembly bridge");
    failures += expect_contains("write command", out, "");
    out = minifs_execute("read docs/notes.txt");
    failures += expect_contains("read command", out, "WebAssembly bridge");
    out = minifs_execute("cd docs");
    failures += expect_contains("cwd path", minifs_get_cwd(), "/docs");
    out = minifs_execute("cd /");
    (void)out;
    tree = minifs_export_tree_json();
    failures += expect_contains("tree JSON export", tree, "\"name\":\"docs\"");
    failures += expect_contains("tree content export", tree, "WebAssembly bridge");
    out = minifs_execute("mkdir undo-check");
    (void)out;
    out = minifs_execute("undo");
    (void)out;
    failures += expect_contains("undo removes latest directory", minifs_export_tree_json(), "\"name\":\"docs\"");
    if (strstr(minifs_export_tree_json(), "undo-check")) { fprintf(stderr, "FAIL undo removed directory: entry still exists\n"); failures++; }
    else puts("PASS undo removed directory");
    out = minifs_execute("redo");
    (void)out;
    failures += expect_contains("redo restores directory", minifs_export_tree_json(), "undo-check");
    out = minifs_execute("fsck");
    failures += expect_contains("filesystem integrity", out, "filesystem is consistent");
    minifs_free();
    if (failures) { fprintf(stderr, "%d API smoke check(s) failed\n", failures); return 1; }
    puts("All C API smoke checks passed.");
    return 0;
}
