# File System (3 people)
   implement a file system interface using binary files
   - The file system reserves the first part of the disk to store:
     - a Header sections for storing disk info
     - a linked list of free blocks
     - a single global directory

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

## Disk Structure
The disk contains in the first location its Header, then the bitmap.
After the bitmap it can contain files, or directories.

## FS Structure
The user can load an existing disk or a create a new one, specifying the number of blocks contained in the disk.
When the disk is created the FS creates also the root directory.

Upon loading a disk or creating a new one the user can start creating directories or file just like any normal filesystem.

Every file is indexed, the fcb of the file is the beginning of a linked list of Indexes which store the position in disk of the corresponding data block.
A file is formed by two linked lists, one for the indexes and one for the data blocks.

A directory is a file, but with only a linked list of directory blocks, which store the position on disk of the files contained in the directory.

## Shell
To use our filesystem we have included a shell, which performs all the filesystem functions.
all the commands in the shell begin with a / (eg. /ls to perform ls command).
to have a list of the commands in the shell type /help.

For further details see our report.
