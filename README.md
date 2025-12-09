# Mini File System Simulator (C)

A simple in-memory file system simulator written in C.  
It supports hierarchical directories, file operations, recursive traversal, and shell-like navigation — similar to a minimal UNIX-style file system.

This project was built to explore operating system fundamentals such as directory trees, path resolution, metadata handling, and command parsing.

---

## Features  

### Directory Operations
- mkdir `<path>` — Create a directory  
- rmdir `<path>` — Remove an empty directory  
- ls `<path>` — List contents of a directory  
- cd `<path>` — Change current directory  
- pwd — Print current working directory  
- tree — Display the full directory hierarchy recursively  

---

### File Operations
- touch `<path>` — Create an empty file  
- write `<path>` `<text>` — Overwrite file content  
- append `<path>` `<text>` — Append to existing content  
- read `<path>` — Display file content  
- rm `<path>` — Remove a file  

---

## Concepts Demonstrated

- Tree-based directory structure  
- Parent–child–sibling pointer system  
- Recursive traversal  
- In-memory metadata handling  
- Path parsing and command interpretation  
- Error handling for invalid operations  

This simulates simplified UNIX-like file system behavior.

---

## Compilation

```bash
gcc -std=c11 -Wall -Wextra -o mini_fs mini_fs.c

---

## Why This Project Matters

This project provides a practical, hands-on understanding of key operating system concepts, including:

- how file systems organize hierarchical data  
- how directories and files are represented internally  
- how path traversal works across nested directories  
- how recursion enables tree-based operations such as listing and traversal  

It is especially useful for learning:

- systems programming  
- operating system fundamentals  
- storage and file system concepts  

