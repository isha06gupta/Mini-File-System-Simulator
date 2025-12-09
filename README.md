Mini File System Simulator (C)

A simple in-memory file system simulator implemented in C.
It supports hierarchical directories, file operations, recursive traversal, and basic navigation commands — similar to a tiny UNIX-like file system.

This project was built to deepen understanding of operating system fundamentals, such as file system structures, metadata management, and path resolution.

🚀 Features
Directory Operations

mkdir <path> — Create a directory

rmdir <path> — Remove an empty directory

ls <path> — List directory contents

cd <path> — Change current directory

pwd — Print current working directory

tree — Display the entire file system hierarchy

File Operations

touch <path> — Create an empty file

write <path> <text> — Overwrite file content

append <path> <text> — Append to an existing file

read <path> — Display file content

rm <path> — Remove a file

🧠 Concepts Demonstrated

This project covers key system-level ideas:

Directory tree representation (parent/child/sibling pointers)

Path parsing and traversal

Recursive tree walking

File metadata and in-memory storage

Command parsing and state management

Basic error handling (invalid paths, duplicates, non-empty folders)

It functions entirely in memory, making it a lightweight simulation of how real file systems organize data.

🛠️ How to Compile
gcc -std=c11 -Wall -Wextra -o mini_fs mini_fs.c

▶️ How to Run
./mini_fs

📘 Sample Commands
mkdir dir1
mkdir dir1/sub
touch dir1/sub/hello.txt
write dir1/sub/hello.txt This_is_a_test
read dir1/sub/hello.txt
tree
pwd


Expected output preview:

/
  dir1/
    sub/
      hello.txt

📂 Project Structure
mini_fs.c         -> Main source code
README.md         -> Project overview + usage instructions


The program uses a simple in-memory tree data structure where each node is either:

a directory (with children), or

a file (storing content)

🎯 Purpose of This Project

This project was built as a hands-on way to learn:

how file systems store data

how hierarchical directories work internally

how commands like ls, cd, and pwd actually resolve paths

how OS concepts can be implemented using plain C

It demonstrates practical system-level thinking — useful in roles involving storage systems, infrastructure, OS internals, or backend engineering.

👩‍💻 Author

Isha Gupta
