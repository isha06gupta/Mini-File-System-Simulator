# Mini File System Simulator (C)

A simple **in-memory file system simulator** written in C.  
It supports hierarchical directories, file operations, recursive traversal, and shell-like navigation — similar to a minimal UNIX-style file system.

This project was built to explore **operating system fundamentals** such as directory trees, path resolution, metadata handling, and command parsing.

---

## Features  

### ** Directory Operations**
- **mkdir `<path>`** — Create a directory  
- **rmdir `<path>`** — Remove an empty directory  
- **ls `<path>`** — List contents of a directory  
- **cd `<path>`** — Change current directory  
- **pwd** — Print current working directory  
- **tree** — Display the full directory hierarchy recursively  

---

### ** File Operations**
- **touch `<path>`** — Create an empty file  
- **write `<path>` `<text>`** — Overwrite file content  
- **append `<path>` `<text>`** — Append to existing content  
- **read `<path>`** — Display file content  
- **rm `<path>`** — Remove a file  

---

##  Concepts Demonstrated

- Tree-based directory structure  
- Parent–child–sibling pointers  
- Recursive traversal  
- In-memory metadata storage  
- Path parsing + command interpreter  
- Error handling for invalid paths  

This mirrors simplified UNIX-like file system behavior.

---

##  Compilation

```bash
gcc -std=c11 -Wall -Wextra -o mini_fs mini_fs.c

