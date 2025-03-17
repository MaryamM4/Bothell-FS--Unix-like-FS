// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "fs.h"
#include "bfs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0;
}

// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF)
    return EFNF;
  return bfsInumToFd(inum);
}

// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE *fp = fopen(BFSDISK, "w+b");
  if (fp == NULL)
    FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp); // initialize Super block
  if (ret != 0) {
    fclose(fp);
    FATAL(ret);
  }

  ret = bfsInitInodes(fp); // initialize Inodes block
  if (ret != 0) {
    fclose(fp);
    FATAL(ret);
  }

  ret = bfsInitDir(fp); // initialize Dir block
  if (ret != 0) {
    fclose(fp);
    FATAL(ret);
  }

  ret = bfsInitFreeList(); // initialize Freelist
  if (ret != 0) {
    fclose(fp);
    FATAL(ret);
  }

  fclose(fp);
  return 0;
}

// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE *fp = fopen(BFSDISK, "rb");
  if (fp == NULL)
    FATAL(ENODISK); // BFSDISK not found
  fclose(fp);
  return 0;
}

// ============================================================================
// Open the existing file called 'fname'.  On success, return its file
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname); // lookup 'fname' in Directory
  if (inum == EFNF)
    return EFNF;
  return bfsInumToFd(inum);
}

// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort.
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void *buf) {
  i32 bytesRead = 0;
  const i32 inum = bfsFdToInum(fd);

  // Iterative variables:
  i8 *bufItr = (i8 *)buf; // For iterating buf's write-to
  i32 cursorPos = fsTell(fd);
  i32 fbn = fsCursorPosToFdn(cursorPos);

  // We might not be able to read as many bytes as requested
  // if not enough of the file left.
  i32 bytesAvailable =
      (fsSize(fd) - cursorPos > numb) ? numb : fsSize(fd) - cursorPos;

  // Error/Boundary checks:
  if (cursorPos < 0 || cursorPos == EBADCURS) {
    FATAL(EBADCURS); // Invalid cursor.
    return EBADCURS;
  }
  if (cursorPos >= fsSize(fd)) {
    return 0; // At EOF, nothing to read.
  }

  // =============================
  // FIRST PARTIAL BLOCK (if any)
  i32 offset = cursorPos % BYTESPERBLOCK;
  if (offset > 0) {
    i8 temp[BYTESPERBLOCK];
    bfsRead(inum, fbn, temp);

    // Add to bytes read, ignoring part that will be discarded.
    // Bytes read shouldn't be > bytesAvailable.
    bytesRead = (BYTESPERBLOCK - offset > bytesAvailable)
                    ? bytesAvailable
                    : BYTESPERBLOCK - offset;

    // Copy desired (right) portion from temp to buffer.
    memcpy(bufItr, temp + offset, bytesRead);

    // Update iterative buffer pointer and file cursor
    bufItr += bytesRead;
    fsSeek(fd, bytesRead, SEEK_CUR);
  }

  // =============================
  // FULL BLOCK(s) (if any)
  while (bytesAvailable - bytesRead >= BYTESPERBLOCK) {
    fbn = fsFdToFdn(fd);
    bfsRead(inum, fbn, bufItr);

    // Update buf ptr, read count, and cursor position.
    bufItr += BYTESPERBLOCK;
    bytesRead += BYTESPERBLOCK;
    fsSeek(fd, BYTESPERBLOCK, SEEK_CUR);
  }

  // =============================
  // LAST PARTIAL BLOCK (if any)
  i32 remainder = bytesAvailable - bytesRead;
  if (remainder > 0) {
    i8 temp[BYTESPERBLOCK];

    fbn = fsFdToFdn(fd);
    bfsRead(inum, fbn, temp);

    memcpy(bufItr, temp, remainder);
    bytesRead += remainder;
    fsSeek(fd, remainder, SEEK_CUR);
  }

  return bytesRead;
}

// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0)
    FATAL(EBADCURS);

  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);

  switch (whence) {
  case SEEK_SET:
    g_oft[ofte].curs = offset;
    break;
  case SEEK_CUR:
    g_oft[ofte].curs += offset;
    break;
  case SEEK_END: {
    i32 end = fsSize(fd);
    g_oft[ofte].curs = end + offset;
    break;
  }
  default:
    FATAL(EBADWHENCE);
  }
  return 0;
}

// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) { return bfsTell(fd); }

// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}

// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void *buf) {
  i32 bytesWritten = 0;
  const i32 inum = bfsFdToInum(fd);

  i8 *bufItr = (i8 *)buf; // For iterating buf's write-to
  i32 cursorPos = fsTell(fd);
  i32 fbn;
  i32 dbn; // bfsWrite() missing.

  // Error/Boundary checks:
  if (cursorPos < 0 || cursorPos == EBADCURS) {
    FATAL(EBADCURS); // Invalid cursor.
    return EBADCURS;
  }

  // If file doesn't have enough blocks, allocate new DBN blocks.
  if ((fsSize(fd) - cursorPos) < numb) {
    i32 lastFbn = fsCursorPosToFdn(cursorPos + numb);
    bfsExtend(inum, lastFbn);
  }

  // =============================
  // FIRST PARTIAL BLOCK (if any)
  i32 offset = cursorPos % BYTESPERBLOCK;
  if (offset > 0) {
    fbn = fsCursorPosToFdn(cursorPos);
    dbn = bfsFbnToDbn(inum, fbn);

    // Will need to write back left portion
    i8 temp[BYTESPERBLOCK];
    bfsRead(inum, fbn, temp);

    // Add to bytes read, ignoring part that will be discarded.
    // Bytes read shouldn't be > numb.
    bytesWritten =
        (BYTESPERBLOCK - offset > numb) ? numb : BYTESPERBLOCK - offset;

    // Copy right portion from buf to left side of temp.
    memcpy(temp + offset, bufItr, bytesWritten);
    bioWrite(dbn, temp); // Now we write temp to the file

    // Update iteratives
    bufItr += bytesWritten;
    fsSeek(fd, bytesWritten, SEEK_CUR);
  }

  // =============================
  // FULL BLOCK(s) (if any)
  while (numb - bytesWritten >= BYTESPERBLOCK) {
    fbn = fsFdToFdn(fd);
    dbn = bfsFbnToDbn(inum, fbn);

    bioWrite(dbn, bufItr);

    // Update iteratives: buf ptr, read count, and cursor position.
    bufItr += BYTESPERBLOCK;
    bytesWritten += BYTESPERBLOCK;
    fsSeek(fd, BYTESPERBLOCK, SEEK_CUR);
  }

  // =============================
  // LAST PARTIAL BLOCK (if any)
  i32 remainder = numb - bytesWritten;
  if (remainder > 0) {
    fbn = fsFdToFdn(fd);
    dbn = bfsFbnToDbn(inum, fbn);

    // Copy left portion from buf to right side of temp.
    i8 temp[BYTESPERBLOCK];
    bfsRead(inum, fbn, temp);
    memcpy(temp, bufItr, remainder);

    // Now write to disk
    bioWrite(dbn, temp);

    bytesWritten += remainder;
    fsSeek(fd, remainder, SEEK_CUR);
  }

  // Update inode's filesize if needed.
  i32 finalPos = fsTell(fd);
  if (finalPos > bfsGetSize(inum)) {
    bfsSetSize(inum, finalPos);
  }

  return 0;
}

// ============================================================================
// Use fs and it's cursor's position to find the corresponding FDN.
// ============================================================================
i32 fsFdToFdn(i32 fd) {
  i32 cursorPos = fsTell(fd);
  return fsCursorPosToFdn(cursorPos);
}

// ============================================================================
// Use the cursor position to find the corresponding FDN.
// ============================================================================
i32 fsCursorPosToFdn(i32 cursorPos) { return (i32)(cursorPos / BYTESPERBLOCK); }