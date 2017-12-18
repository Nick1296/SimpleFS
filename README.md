# File System (3 people)
   implement a file system interface using binary files
   - The file system reserves the first part of the disk to store:
     - a linked list of free blocks
     - a single global directory

     Each linked list contains the addresses of the blocks creating a tree with two levels, the root level with the addresses and the first level are the block used.
     We could substitute the list with an AVL tree to minimize the time required to reach a block. 
     
   - Blocks are of two types
     - data blocks
     - directory blocks

     A data block are "random" information
     A directory block contains a sequence of
     structs of type "directory_entry",
     containing the blocks where the files in that folder start
     and if they are directory themselves
  
  - Create functions to use the filesystem, in detail:
    - Create a file (touch)
    - Create a directory (mkdir)
    - Print the contents of the directory (ls)
    - Print the contents of the file (cat)
    - Write into file (echo-like)
    - Delete a directory/file (rm)