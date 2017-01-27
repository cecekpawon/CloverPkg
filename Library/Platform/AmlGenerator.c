/*
 *  AmlGenerator.c
 *  Chameleon
 *
 *  Created by Mozodojo on 20/07/10.
 *  Copyright 2010 mozo. All rights reserved.
 *
 * additions and corrections by Slice and pcj, 2012.
 */

#include <Library/Platform/AmlGenerator.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_AML_GEN
#define DEBUG_AML_GEN -1
#endif
#else
#ifdef DEBUG_AML_GEN
#undef DEBUG_AML_GEN
#endif
#define DEBUG_AML_GEN DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_AML_GEN, __VA_ARGS__)

BOOLEAN
AmlAddToParent (
  AML_CHUNK   *parent,
  AML_CHUNK   *node
) {
  if (parent && node) {
    switch (parent->Type) {
      case AML_CHUNK_NONE:
      case AML_CHUNK_BYTE:
      case AML_CHUNK_WORD:
      case AML_CHUNK_DWORD:
      case AML_CHUNK_QWORD:
      case AML_CHUNK_ALIAS:
        MsgLog ("AmlAddToParent: node doesn't support child nodes!\n");
        return FALSE;

      case AML_CHUNK_NAME:
        if (parent->First) {
          MsgLog ("AmlAddToParent: name node supports only one child node!\n");
          return FALSE;
        }
        break;

      default:
        break;
    }

    if (!parent->First) {
      parent->First = node;
    }

    if (parent->Last) {
      parent->Last->Next = node;
    }

    parent->Last = node;

    return TRUE;
  }

  return FALSE;
}

AML_CHUNK *
AmlCreateNode (
  AML_CHUNK   *parent
) {
  AML_CHUNK   *node = (AML_CHUNK *)AllocateZeroPool (sizeof (AML_CHUNK));

  AmlAddToParent (parent, node);

  return node;
}

VOID
AmlDestroyNode (
  AML_CHUNK   *node
) {
  // Delete child nodes
  AML_CHUNK   *child = node->First;

  while (child) {
    AML_CHUNK   *next = child->Next;

    if (child->Buffer) {
      FreePool (child->Buffer);
    }

    FreePool (child);

    child = next;
  }

  // Free node
  if (node->Buffer) {
    FreePool (node->Buffer);
  }

  FreePool (node);
}

AML_CHUNK *
AmlAddBuffer (
  AML_CHUNK   *parent,
  CHAR8       *buffer,
  UINT32      size
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_NONE;
    node->Length = (UINT16)size;
    node->Buffer = AllocateZeroPool (node->Length);
    CopyMem (node->Buffer, buffer, node->Length);
  }

  return node;
}

AML_CHUNK *
AmlAddByte (
  AML_CHUNK   *parent,
  UINT8       value
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_BYTE;

    node->Length = 1;
    node->Buffer = AllocateZeroPool (node->Length);
    node->Buffer[0] = value;
  }

  return node;
}

AML_CHUNK *
AmlAddWord (
  AML_CHUNK   *parent,
  UINT16      value
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_WORD;
    node->Length = 2;
    node->Buffer = AllocateZeroPool (node->Length);
    node->Buffer[0] = value & 0xff;
    node->Buffer[1] = value >> 8;
  }

  return node;
}

AML_CHUNK *
AmlAddDword (
  AML_CHUNK   *parent,
  UINT32      value
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_DWORD;
    node->Length = 4;
    node->Buffer = AllocateZeroPool (node->Length);
    node->Buffer[0] = value & 0xff;
    node->Buffer[1] = (value >> 8) & 0xff;
    node->Buffer[2] = (value >> 16) & 0xff;
    node->Buffer[3] = (value >> 24) & 0xff;
  }

  return node;
}

AML_CHUNK *
AmlAddQword (
  AML_CHUNK   *parent,
  UINT64      value
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_QWORD;
    node->Length = 8;
    node->Buffer = AllocateZeroPool (node->Length);
    node->Buffer[0] = value & 0xff;
    node->Buffer[1] = RShiftU64 (value, 8) & 0xff;
    node->Buffer[2] = RShiftU64 (value, 16) & 0xff;
    node->Buffer[3] = RShiftU64 (value, 24) & 0xff;
    node->Buffer[4] = RShiftU64 (value, 32) & 0xff;
    node->Buffer[5] = RShiftU64 (value, 40) & 0xff;
    node->Buffer[6] = RShiftU64 (value, 48) & 0xff;
    node->Buffer[7] = RShiftU64 (value, 56) & 0xff;
  }

  return node;
}

UINT32
AmlFillSimpleName (
  CHAR8     *buffer,
  CHAR8     *name
) {
  if (AsciiStrLen (name) < 4) {
    //    MsgLog ("AmlFillSimpleName: simple name %a has incorrect lengh! Must
    //    be 4.\n", name);
    return 0;
  }

  CopyMem (buffer, name, 4);
  return 4;
}

UINT32
AmlFillName (
  AML_CHUNK   *node,
  CHAR8       *name
) {
  INTN      len, offset, count;
  UINT32    root = 0;

  if (!node) {
    return 0;
  }

  len = AsciiStrLen (name);
  offset = 0;
  count = len >> 2;

  if (((len % 4) > 1) || (count == 0)) {
    //    MsgLog ("AmlFillName: pathname %a has incorrect length! Must be 4, 8,
    //    12, 16, etc...\n", name);
    return 0;
  }

  if (((len % 4) == 1) && (name[0] == '\\')) {
    root++;
  }

  if (count == 1) {
    node->Length = (UINT16)(4 + root);
    node->Buffer = AllocateZeroPool (node->Length + 4);
    CopyMem (node->Buffer, name, 4 + root);
    offset += 4 + root;
    return (UINT32)offset;
  }

  if (count == 2) {
    node->Length = 2 + 8;
    node->Buffer = AllocateZeroPool (node->Length + 4);
    node->Buffer[offset++] = 0x5c;  // Root Char
    node->Buffer[offset++] = 0x2e;  // Double name
    CopyMem (node->Buffer + offset, name + root, 8);
    offset += 8;
    return (UINT32)offset;
  }

  node->Length = (UINT16)(3 + (count << 2));
  node->Buffer = AllocateZeroPool (node->Length + 4);
  node->Buffer[offset++] = 0x5c;          // Root Char
  node->Buffer[offset++] = 0x2f;          // Multi name
  node->Buffer[offset++] = (CHAR8)count;  // Names count
  CopyMem (node->Buffer + offset, name + root, count * 4);
  offset += count * 4;

  return (UINT32)offset;  // node->Length;
}

AML_CHUNK *
AmlAddScope (
  AML_CHUNK   *parent,
  CHAR8       *name
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_SCOPE;

    AmlFillName (node, name);
  }

  return node;
}

AML_CHUNK *
AmlAddName (
  AML_CHUNK   *parent,
  CHAR8       *name
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_NAME;

    AmlFillName (node, name);
  }

  return node;
}

AML_CHUNK *
AmlAddMethod (
  AML_CHUNK   *parent,
  CHAR8       *name,
  UINT8       args
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    UINTN offset = AmlFillName (node, name);
    node->Type = AML_CHUNK_METHOD;
    node->Length++;
    node->Buffer[offset] = args;
    //AML_CHUNK *meth = AmlAddByte (node, args);
    //return meth;
  }

  return node;
}

AML_CHUNK *
AmlAddPackage (
  AML_CHUNK   *parent
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_PACKAGE;

    node->Length = 1;
    node->Buffer = AllocateZeroPool (node->Length);
  }

  return node;
}

AML_CHUNK *
AmlAddAlias (
  AML_CHUNK   *parent,
  CHAR8       *name1,
  CHAR8       *name2
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_ALIAS;

    node->Length = 8;
    node->Buffer = AllocateZeroPool (node->Length);
    AmlFillSimpleName (node->Buffer, name1);
    AmlFillSimpleName (node->Buffer + 4, name2);
  }

  return node;
}

AML_CHUNK *
AmlAddReturnName (
  AML_CHUNK   *parent,
  CHAR8       *name
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_RETURN;
    AmlFillName (node, name);
  }

  return node;
}

AML_CHUNK *
AmlAddReturnByte (
  AML_CHUNK   *parent,
  UINT8       value
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_RETURN;
    AmlAddByte (node, value);
  }

  return node;
}

AML_CHUNK *
AmlAddDevice (
  AML_CHUNK   *parent,
  CHAR8       *name
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_DEVICE;
    AmlFillName (node, name);
  }

  return node;
}

AML_CHUNK *
AmlAddLocal0 (
  AML_CHUNK   *parent
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_LOCAL0;
    node->Length = 1;
  }

  return node;
}

AML_CHUNK *
AmlAddStore (
  AML_CHUNK   *parent
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_STORE_OP;
    node->Length = 1;
  }

  return node;
}

AML_CHUNK *
AmlAddByteBuffer (
  AML_CHUNK   *parent,
  CHAR8       *data,
  UINT32      size
) {
  AML_CHUNK *node = AmlCreateNode (parent);

  if (node) {
    INTN    offset = 0;

    node->Type = AML_CHUNK_BUFFER;
    node->Length = (UINT8)(size + 2);
    node->Buffer = AllocateZeroPool (node->Length);
    node->Buffer[offset++] = AML_CHUNK_BYTE;  // 0x0A
    node->Buffer[offset++] = (CHAR8)size;
    CopyMem (node->Buffer + offset, data, node->Length);
  }

  return node;
}

AML_CHUNK *
AmlAddStringBuffer (
  AML_CHUNK   *parent,
  CHAR8       *StringBuf
) {
  AML_CHUNK *node = AmlCreateNode (parent);

  if (node) {
    UINTN   offset = 0, len = AsciiStrLen (StringBuf);

    node->Type = AML_CHUNK_BUFFER;
    node->Length = (UINT8)(len + 3);
    node->Buffer = AllocateZeroPool (node->Length);
    node->Buffer[offset++] = AML_CHUNK_BYTE;
    node->Buffer[offset++] = (CHAR8)(len + 1);
    CopyMem (node->Buffer + offset, StringBuf, len);
    //node->Buffer[offset+len] = '\0';  //already zero pool
  }

  return node;
}

AML_CHUNK *
AmlAddString (
  AML_CHUNK   *parent,
  CHAR8       *StringBuf
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    INTN    len = AsciiStrLen (StringBuf);

    node->Type = AML_CHUNK_STRING;
    node->Length = (UINT8)(len + 1);
    node->Buffer = AllocateZeroPool (len + 1);
    CopyMem (node->Buffer, StringBuf, len);
    //node->Buffer[len] = '\0';
  }

  return node;
}

AML_CHUNK *
AmlAddReturn (
  AML_CHUNK   *parent
) {
  AML_CHUNK   *node = AmlCreateNode (parent);

  if (node) {
    node->Type = AML_CHUNK_RETURN;
    // AmlAddByte (node, value);
  }

  return node;
}

UINT8
AmlGetSizeLength (
  UINT32      size
) {
  if ((size + 1) <= 0x3f) {
    return 1;
  } else if ((size + 2) <= 0xfff) {/* Encode in 4 bits and 1 byte */
    return 2;
  } else if ((size + 3) <= 0xfffff) { /* Encode in 4 bits and 2 bytes */
    return 3;
  }

  return 4; /* Encode 0xfffffff in 4 bits and 2 bytes */
}

UINT32
AmlCalculateSize (
  AML_CHUNK     *node
) {
  if (node) {
    // Calculate child nodes size
    AML_CHUNK   *child = node->First;
    UINT8       child_count = 0;

    node->Size = 0;
    while (child) {
      child_count++;

      node->Size += (UINT16)AmlCalculateSize (child);

      child = child->Next;
    }

    switch (node->Type) {
      case AML_CHUNK_NONE:
      case AML_STORE_OP:
      case AML_CHUNK_LOCAL0:
        node->Size += node->Length;
        break;

      case AML_CHUNK_METHOD:
      case AML_CHUNK_SCOPE:
      case AML_CHUNK_BUFFER:
        node->Size += 1 + node->Length;
        node->Size += AmlGetSizeLength (node->Size);
        break;

      case AML_CHUNK_DEVICE:
        node->Size += 2 + node->Length;
        node->Size += AmlGetSizeLength (node->Size);
        break;

      case AML_CHUNK_PACKAGE:
        node->Buffer[0] = child_count;
        node->Size += 1 + node->Length;
        node->Size += AmlGetSizeLength (node->Size);
        break;

      case AML_CHUNK_BYTE:
        if ((node->Buffer[0] == 0x0) || (node->Buffer[0] == 0x1)) {
          node->Size += node->Length;
        } else {
          node->Size += 1 + node->Length;
        }

        break;

      case AML_CHUNK_WORD:
      case AML_CHUNK_DWORD:
      case AML_CHUNK_QWORD:
      case AML_CHUNK_ALIAS:
      case AML_CHUNK_NAME:
      case AML_CHUNK_RETURN:
      case AML_CHUNK_STRING:
        node->Size += 1 + node->Length;
        break;
    }

    return node->Size;
  }

  return 0;
}

UINT32
AmlWriteByte (
  UINT8     value,
  CHAR8     *buffer,
  UINT32    offset
) {
  buffer[offset++] = value;

  return offset;
}

UINT32
AmlWriteWord (
  UINT16    value,
  CHAR8     *buffer,
  UINT32    offset
) {
  buffer[offset++] = value & 0xff;
  buffer[offset++] = value >> 8;

  return offset;
}

UINT32
AmlWriteDword (
  UINT32    value,
  CHAR8     *buffer,
  UINT32    offset
) {
  buffer[offset++] = value & 0xff;
  buffer[offset++] = (value >> 8) & 0xff;
  buffer[offset++] = (value >> 16) & 0xff;
  buffer[offset++] = (value >> 24) & 0xff;

  return offset;
}

UINT32
AmlWriteQword (
  UINT64    value,
  CHAR8     *buffer,
  UINT32    offset
) {
  buffer[offset++] = value & 0xff;
  buffer[offset++] = RShiftU64 (value, 8) & 0xff;
  buffer[offset++] = RShiftU64 (value, 16) & 0xff;
  buffer[offset++] = RShiftU64 (value, 24) & 0xff;
  buffer[offset++] = RShiftU64 (value, 32) & 0xff;
  buffer[offset++] = RShiftU64 (value, 40) & 0xff;
  buffer[offset++] = RShiftU64 (value, 48) & 0xff;
  buffer[offset++] = RShiftU64 (value, 56) & 0xff;

  return offset;
}

UINT32
AmlWriteBuffer (
  CONST CHAR8     *value,
        UINT32    size,
        CHAR8     *buffer,
        UINT32    offset
) {
  if (size > 0) {
    CopyMem (buffer + offset, value, size);
  }

  return offset + size;
}

UINT32
AmlWriteSize (
  UINT32    size,
  CHAR8     *buffer,
  UINT32    offset
) {
  if (size <= 0x3f) { /* simple 1 byte length in 6 bits */

    buffer[offset++] = (CHAR8)size;
  } else if (size <= 0xfff) {
    buffer[offset++] = 0x40 | (size & 0xf); /* 0x40 is type, 0x0X is first nibble of length */
    buffer[offset++] = (size >> 4) & 0xff;  /* +1 bytes for rest length */
  } else if (size <= 0xfffff) {
    buffer[offset++] = 0x80 | (size & 0xf); /* 0x80 is type, 0x0X is first nibble of length */
    buffer[offset++] = (size >> 4) & 0xff;  /* +2 bytes for rest length */
    buffer[offset++] = (size >> 12) & 0xff;
  } else {
    buffer[offset++] = 0xc0 | (size & 0xf); /* 0xC0 is type, 0x0X is first nibble of length */
    buffer[offset++] = (size >> 4) & 0xff;  /* +3 bytes for rest length */
    buffer[offset++] = (size >> 12) & 0xff;
    buffer[offset++] = (size >> 20) & 0xff;
  }

  return offset;
}

UINT32
AmlWriteNode (
  AML_CHUNK   *node,
  CHAR8       *buffer,
  UINT32      offset
) {
  if (node && buffer) {
    UINT32      old = offset;
    AML_CHUNK   *child = node->First;

    switch (node->Type) {
      case AML_CHUNK_NONE:
        offset = AmlWriteBuffer (node->Buffer, node->Length, buffer, offset);
        break;

      case AML_CHUNK_LOCAL0:
      case AML_STORE_OP:
        offset = AmlWriteByte (node->Type, buffer, offset);
        break;

      case AML_CHUNK_DEVICE:
        offset = AmlWriteByte (AML_CHUNK_OP, buffer, offset);
        offset = AmlWriteByte (node->Type, buffer, offset);
        offset = AmlWriteSize (node->Size - 2, buffer, offset);
        offset = AmlWriteBuffer (node->Buffer, node->Length, buffer, offset);
        break;

      case AML_CHUNK_SCOPE:
      case AML_CHUNK_METHOD:
      case AML_CHUNK_PACKAGE:
      case AML_CHUNK_BUFFER:
        offset = AmlWriteByte (node->Type, buffer, offset);
        offset = AmlWriteSize (node->Size - 1, buffer, offset);
        offset = AmlWriteBuffer (node->Buffer, node->Length, buffer, offset);
        break;

      case AML_CHUNK_BYTE:
        if ((node->Buffer[0] == 0x0) || (node->Buffer[0] == 0x1)) {
          offset = AmlWriteBuffer (node->Buffer, node->Length, buffer, offset);
        } else {
          offset = AmlWriteByte (node->Type, buffer, offset);
          offset = AmlWriteBuffer (node->Buffer, node->Length, buffer, offset);
        }
        break;

      case AML_CHUNK_WORD:
      case AML_CHUNK_DWORD:
      case AML_CHUNK_QWORD:
      case AML_CHUNK_ALIAS:
      case AML_CHUNK_NAME:
      case AML_CHUNK_RETURN:
      case AML_CHUNK_STRING:
        offset = AmlWriteByte (node->Type, buffer, offset);
        offset = AmlWriteBuffer (node->Buffer, node->Length, buffer, offset);
        break;

      default:
        break;
    }

    while (child) {
      offset = AmlWriteNode (child, buffer, offset);
      child = child->Next;
    }

    if ((offset - old) != node->Size) {
      MsgLog ("Node size incorrect: type=0x%x size=%x offset=%x\n", node->Type,
             node->Size, (offset - old));
    }
  }

  return offset;
}
