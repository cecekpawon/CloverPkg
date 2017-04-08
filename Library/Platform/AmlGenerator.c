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
  AML_CHUNK   *Parent,
  AML_CHUNK   *Node
) {
  if (Parent && Node) {
    switch (Parent->Type) {
      case AML_CHUNK_NONE:
      case AML_CHUNK_BYTE:
      case AML_CHUNK_WORD:
      case AML_CHUNK_DWORD:
      case AML_CHUNK_QWORD:
      case AML_CHUNK_ALIAS:
        MsgLog ("AmlAddToParent: Node doesn't support Child Nodes!\n");
        return FALSE;

      case AML_CHUNK_NAME:
        if (Parent->First) {
          MsgLog ("AmlAddToParent: Name Node supports only one Child Node!\n");
          return FALSE;
        }
        break;

      default:
        break;
    }

    if (!Parent->First) {
      Parent->First = Node;
    }

    if (Parent->Last) {
      Parent->Last->Next = Node;
    }

    Parent->Last = Node;

    return TRUE;
  }

  return FALSE;
}

AML_CHUNK *
AmlCreateNode (
  AML_CHUNK   *Parent
) {
  AML_CHUNK   *Node = (AML_CHUNK *)AllocateZeroPool (sizeof (AML_CHUNK));

  AmlAddToParent (Parent, Node);

  return Node;
}

VOID
AmlDestroyNode (
  AML_CHUNK   *Node
) {
  // Delete Child Nodes
  AML_CHUNK   *Child = Node->First;

  while (Child) {
    AML_CHUNK   *Next = Child->Next;

    if (Child->Buffer) {
      FreePool (Child->Buffer);
    }

    FreePool (Child);

    Child = Next;
  }

  // Free Node
  if (Node->Buffer) {
    FreePool (Node->Buffer);
  }

  FreePool (Node);
}

AML_CHUNK *
AmlAddBuffer (
  AML_CHUNK   *Parent,
  CHAR8       *Buffer,
  UINT32      Size
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_NONE;
    Node->Length = (UINT16)Size;
    Node->Buffer = AllocateZeroPool (Node->Length);
    CopyMem (Node->Buffer, Buffer, Node->Length);
  }

  return Node;
}

AML_CHUNK *
AmlAddByte (
  AML_CHUNK   *Parent,
  UINT8       Value
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_BYTE;

    Node->Length = 1;
    Node->Buffer = AllocateZeroPool (Node->Length);
    Node->Buffer[0] = Value;
  }

  return Node;
}

AML_CHUNK *
AmlAddWord (
  AML_CHUNK   *Parent,
  UINT16      Value
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_WORD;
    Node->Length = 2;
    Node->Buffer = AllocateZeroPool (Node->Length);
    Node->Buffer[0] = Value & 0xff;
    Node->Buffer[1] = Value >> 8;
  }

  return Node;
}

AML_CHUNK *
AmlAddDword (
  AML_CHUNK   *Parent,
  UINT32      Value
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_DWORD;
    Node->Length = 4;
    Node->Buffer = AllocateZeroPool (Node->Length);
    Node->Buffer[0] = Value & 0xff;
    Node->Buffer[1] = (Value >> 8) & 0xff;
    Node->Buffer[2] = (Value >> 16) & 0xff;
    Node->Buffer[3] = (Value >> 24) & 0xff;
  }

  return Node;
}

AML_CHUNK *
AmlAddQword (
  AML_CHUNK   *Parent,
  UINT64      Value
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_QWORD;
    Node->Length = 8;
    Node->Buffer = AllocateZeroPool (Node->Length);
    Node->Buffer[0] = Value & 0xff;
    Node->Buffer[1] = RShiftU64 (Value, 8) & 0xff;
    Node->Buffer[2] = RShiftU64 (Value, 16) & 0xff;
    Node->Buffer[3] = RShiftU64 (Value, 24) & 0xff;
    Node->Buffer[4] = RShiftU64 (Value, 32) & 0xff;
    Node->Buffer[5] = RShiftU64 (Value, 40) & 0xff;
    Node->Buffer[6] = RShiftU64 (Value, 48) & 0xff;
    Node->Buffer[7] = RShiftU64 (Value, 56) & 0xff;
  }

  return Node;
}

UINT32
AmlFillSimpleName (
  CHAR8     *Buffer,
  CHAR8     *Name
) {
  if (AsciiStrLen (Name) < 4) {
    //    MsgLog ("AmlFillSimpleName: simple Name %a has incorrect Length! Must
    //    be 4.\n", Name);
    return 0;
  }

  CopyMem (Buffer, Name, 4);
  return 4;
}

UINT32
AmlFillName (
  AML_CHUNK   *Node,
  CHAR8       *Name
) {
  INTN      Len, Offset, Count;
  UINT32    Root = 0;

  if (!Node) {
    return 0;
  }

  Len = AsciiStrLen (Name);
  Offset = 0;
  Count = Len >> 2;

  if (((Len % 4) > 1) || (Count == 0)) {
    //    MsgLog ("AmlFillName: pathName %a has incorrect Length! Must be 4, 8,
    //    12, 16, etc...\n", Name);
    return 0;
  }

  if (((Len % 4) == 1) && (Name[0] == '\\')) {
    Root++;
  }

  if (Count == 1) {
    Node->Length = (UINT16)(4 + Root);
    Node->Buffer = AllocateZeroPool (Node->Length + 4);
    CopyMem (Node->Buffer, Name, 4 + Root);
    Offset += 4 + Root;
    return (UINT32)Offset;
  }

  if (Count == 2) {
    Node->Length = 2 + 8;
    Node->Buffer = AllocateZeroPool (Node->Length + 4);
    Node->Buffer[Offset++] = 0x5c;  // Root Char
    Node->Buffer[Offset++] = 0x2e;  // Double Name
    CopyMem (Node->Buffer + Offset, Name + Root, 8);
    Offset += 8;
    return (UINT32)Offset;
  }

  Node->Length = (UINT16)(3 + (Count << 2));
  Node->Buffer = AllocateZeroPool (Node->Length + 4);
  Node->Buffer[Offset++] = 0x5c;          // Root Char
  Node->Buffer[Offset++] = 0x2f;          // Multi Name
  Node->Buffer[Offset++] = (CHAR8)Count;  // Names Count
  CopyMem (Node->Buffer + Offset, Name + Root, Count * 4);
  Offset += Count * 4;

  return (UINT32)Offset;  // Node->Length;
}

AML_CHUNK *
AmlAddScope (
  AML_CHUNK   *Parent,
  CHAR8       *Name
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_SCOPE;

    AmlFillName (Node, Name);
  }

  return Node;
}

AML_CHUNK *
AmlAddName (
  AML_CHUNK   *Parent,
  CHAR8       *Name
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_NAME;

    AmlFillName (Node, Name);
  }

  return Node;
}

AML_CHUNK *
AmlAddMethod (
  AML_CHUNK   *Parent,
  CHAR8       *Name,
  UINT8       Args
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    UINTN Offset = AmlFillName (Node, Name);
    Node->Type = AML_CHUNK_METHOD;
    Node->Length++;
    Node->Buffer[Offset] = Args;
    //AML_CHUNK *meth = AmlAddByte (Node, Args);
    //return meth;
  }

  return Node;
}

AML_CHUNK *
AmlAddPackage (
  AML_CHUNK   *Parent
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_PACKAGE;

    Node->Length = 1;
    Node->Buffer = AllocateZeroPool (Node->Length);
  }

  return Node;
}

AML_CHUNK *
AmlAddAlias (
  AML_CHUNK   *Parent,
  CHAR8       *Name1,
  CHAR8       *Name2
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_ALIAS;

    Node->Length = 8;
    Node->Buffer = AllocateZeroPool (Node->Length);
    AmlFillSimpleName (Node->Buffer, Name1);
    AmlFillSimpleName (Node->Buffer + 4, Name2);
  }

  return Node;
}

AML_CHUNK *
AmlAddReturnName (
  AML_CHUNK   *Parent,
  CHAR8       *Name
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_RETURN;
    AmlFillName (Node, Name);
  }

  return Node;
}

AML_CHUNK *
AmlAddReturnByte (
  AML_CHUNK   *Parent,
  UINT8       Value
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_RETURN;
    AmlAddByte (Node, Value);
  }

  return Node;
}

AML_CHUNK *
AmlAddDevice (
  AML_CHUNK   *Parent,
  CHAR8       *Name
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_DEVICE;
    AmlFillName (Node, Name);
  }

  return Node;
}

AML_CHUNK *
AmlAddLocal0 (
  AML_CHUNK   *Parent
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_LOCAL0;
    Node->Length = 1;
  }

  return Node;
}

AML_CHUNK *
AmlAddStore (
  AML_CHUNK   *Parent
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_STORE_OP;
    Node->Length = 1;
  }

  return Node;
}

AML_CHUNK *
AmlAddByteBuffer (
  AML_CHUNK   *Parent,
  CHAR8       *data,
  UINT32      Size
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    INTN    Offset = 0;

    Node->Type = AML_CHUNK_BUFFER;
    Node->Length = (UINT8)(Size + 2);
    Node->Buffer = AllocateZeroPool (Node->Length);
    Node->Buffer[Offset++] = AML_CHUNK_BYTE;  // 0x0A
    Node->Buffer[Offset++] = (CHAR8)Size;
    CopyMem (Node->Buffer + Offset, data, Node->Length);
  }

  return Node;
}

AML_CHUNK *
AmlAddStringBuffer (
  AML_CHUNK   *Parent,
  CHAR8       *StringBuf
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    UINTN   Offset = 0, Len = AsciiStrLen (StringBuf);

    Node->Type = AML_CHUNK_BUFFER;
    Node->Length = (UINT8)(Len + 3);
    Node->Buffer = AllocateZeroPool (Node->Length);
    Node->Buffer[Offset++] = AML_CHUNK_BYTE;
    Node->Buffer[Offset++] = (CHAR8)(Len + 1);
    CopyMem (Node->Buffer + Offset, StringBuf, Len);
    //Node->Buffer[Offset + Len] = '\0';  //already zero pool
  }

  return Node;
}

AML_CHUNK *
AmlAddString (
  AML_CHUNK   *Parent,
  CHAR8       *StringBuf
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    INTN    Len = AsciiStrLen (StringBuf);

    Node->Type = AML_CHUNK_STRING;
    Node->Length = (UINT8)(Len + 1);
    Node->Buffer = AllocateZeroPool (Len + 1);
    CopyMem (Node->Buffer, StringBuf, Len);
    //Node->Buffer[Len] = '\0';
  }

  return Node;
}

AML_CHUNK *
AmlAddReturn (
  AML_CHUNK   *Parent
) {
  AML_CHUNK   *Node = AmlCreateNode (Parent);

  if (Node) {
    Node->Type = AML_CHUNK_RETURN;
    // AmlAddByte (Node, Value);
  }

  return Node;
}

UINT8
AmlGetSizeLength (
  UINT32      Size
) {
  if ((Size + 1) <= 0x3f) {
    return 1;
  } else if ((Size + 2) <= 0xfff) {/* Encode in 4 bits and 1 byte */
    return 2;
  } else if ((Size + 3) <= 0xfffff) { /* Encode in 4 bits and 2 bytes */
    return 3;
  }

  return 4; /* Encode 0xfffffff in 4 bits and 2 bytes */
}

UINT32
AmlCalculateSize (
  AML_CHUNK     *Node
) {
  if (Node) {
    // Calculate Child Nodes Size
    AML_CHUNK   *Child = Node->First;
    UINT8       Child_Count = 0;

    Node->Size = 0;
    while (Child) {
      Child_Count++;

      Node->Size += (UINT16)AmlCalculateSize (Child);

      Child = Child->Next;
    }

    switch (Node->Type) {
      case AML_CHUNK_NONE:
      case AML_STORE_OP:
      case AML_CHUNK_LOCAL0:
        Node->Size += Node->Length;
        break;

      case AML_CHUNK_METHOD:
      case AML_CHUNK_SCOPE:
      case AML_CHUNK_BUFFER:
        Node->Size += 1 + Node->Length;
        Node->Size += AmlGetSizeLength (Node->Size);
        break;

      case AML_CHUNK_DEVICE:
        Node->Size += 2 + Node->Length;
        Node->Size += AmlGetSizeLength (Node->Size);
        break;

      case AML_CHUNK_PACKAGE:
        Node->Buffer[0] = Child_Count;
        Node->Size += 1 + Node->Length;
        Node->Size += AmlGetSizeLength (Node->Size);
        break;

      case AML_CHUNK_BYTE:
        if ((Node->Buffer[0] == 0x0) || (Node->Buffer[0] == 0x1)) {
          Node->Size += Node->Length;
        } else {
          Node->Size += 1 + Node->Length;
        }

        break;

      case AML_CHUNK_WORD:
      case AML_CHUNK_DWORD:
      case AML_CHUNK_QWORD:
      case AML_CHUNK_ALIAS:
      case AML_CHUNK_NAME:
      case AML_CHUNK_RETURN:
      case AML_CHUNK_STRING:
        Node->Size += 1 + Node->Length;
        break;
    }

    return Node->Size;
  }

  return 0;
}

UINT32
AmlWriteByte (
  UINT8     Value,
  CHAR8     *Buffer,
  UINT32    Offset
) {
  Buffer[Offset++] = Value;

  return Offset;
}

UINT32
AmlWriteWord (
  UINT16    Value,
  CHAR8     *Buffer,
  UINT32    Offset
) {
  Buffer[Offset++] = Value & 0xff;
  Buffer[Offset++] = Value >> 8;

  return Offset;
}

UINT32
AmlWriteDword (
  UINT32    Value,
  CHAR8     *Buffer,
  UINT32    Offset
) {
  Buffer[Offset++] = Value & 0xff;
  Buffer[Offset++] = (Value >> 8) & 0xff;
  Buffer[Offset++] = (Value >> 16) & 0xff;
  Buffer[Offset++] = (Value >> 24) & 0xff;

  return Offset;
}

UINT32
AmlWriteQword (
  UINT64    Value,
  CHAR8     *Buffer,
  UINT32    Offset
) {
  Buffer[Offset++] = Value & 0xff;
  Buffer[Offset++] = RShiftU64 (Value, 8) & 0xff;
  Buffer[Offset++] = RShiftU64 (Value, 16) & 0xff;
  Buffer[Offset++] = RShiftU64 (Value, 24) & 0xff;
  Buffer[Offset++] = RShiftU64 (Value, 32) & 0xff;
  Buffer[Offset++] = RShiftU64 (Value, 40) & 0xff;
  Buffer[Offset++] = RShiftU64 (Value, 48) & 0xff;
  Buffer[Offset++] = RShiftU64 (Value, 56) & 0xff;

  return Offset;
}

UINT32
AmlWriteBuffer (
  CONST CHAR8     *Value,
        UINT32    Size,
        CHAR8     *Buffer,
        UINT32    Offset
) {
  if (Size > 0) {
    CopyMem (Buffer + Offset, Value, Size);
  }

  return Offset + Size;
}

UINT32
AmlWriteSize (
  UINT32    Size,
  CHAR8     *Buffer,
  UINT32    Offset
) {
  if (Size <= 0x3f) { /* simple 1 byte Length in 6 bits */

    Buffer[Offset++] = (CHAR8)Size;
  } else if (Size <= 0xfff) {
    Buffer[Offset++] = 0x40 | (Size & 0xf); /* 0x40 is type, 0x0X is first nibble of Length */
    Buffer[Offset++] = (Size >> 4) & 0xff;  /* +1 bytes for rest Length */
  } else if (Size <= 0xfffff) {
    Buffer[Offset++] = 0x80 | (Size & 0xf); /* 0x80 is type, 0x0X is first nibble of Length */
    Buffer[Offset++] = (Size >> 4) & 0xff;  /* +2 bytes for rest Length */
    Buffer[Offset++] = (Size >> 12) & 0xff;
  } else {
    Buffer[Offset++] = 0xc0 | (Size & 0xf); /* 0xC0 is type, 0x0X is first nibble of Length */
    Buffer[Offset++] = (Size >> 4) & 0xff;  /* +3 bytes for rest Length */
    Buffer[Offset++] = (Size >> 12) & 0xff;
    Buffer[Offset++] = (Size >> 20) & 0xff;
  }

  return Offset;
}

UINT32
AmlWriteNode (
  AML_CHUNK   *Node,
  CHAR8       *Buffer,
  UINT32      Offset
) {
  if (Node && Buffer) {
    UINT32      Old = Offset;
    AML_CHUNK   *Child = Node->First;

    switch (Node->Type) {
      case AML_CHUNK_NONE:
        Offset = AmlWriteBuffer (Node->Buffer, Node->Length, Buffer, Offset);
        break;

      case AML_CHUNK_LOCAL0:
      case AML_STORE_OP:
        Offset = AmlWriteByte (Node->Type, Buffer, Offset);
        break;

      case AML_CHUNK_DEVICE:
        Offset = AmlWriteByte (AML_CHUNK_OP, Buffer, Offset);
        Offset = AmlWriteByte (Node->Type, Buffer, Offset);
        Offset = AmlWriteSize (Node->Size - 2, Buffer, Offset);
        Offset = AmlWriteBuffer (Node->Buffer, Node->Length, Buffer, Offset);
        break;

      case AML_CHUNK_SCOPE:
      case AML_CHUNK_METHOD:
      case AML_CHUNK_PACKAGE:
      case AML_CHUNK_BUFFER:
        Offset = AmlWriteByte (Node->Type, Buffer, Offset);
        Offset = AmlWriteSize (Node->Size - 1, Buffer, Offset);
        Offset = AmlWriteBuffer (Node->Buffer, Node->Length, Buffer, Offset);
        break;

      case AML_CHUNK_BYTE:
        if ((Node->Buffer[0] == 0x0) || (Node->Buffer[0] == 0x1)) {
          Offset = AmlWriteBuffer (Node->Buffer, Node->Length, Buffer, Offset);
        } else {
          Offset = AmlWriteByte (Node->Type, Buffer, Offset);
          Offset = AmlWriteBuffer (Node->Buffer, Node->Length, Buffer, Offset);
        }
        break;

      case AML_CHUNK_WORD:
      case AML_CHUNK_DWORD:
      case AML_CHUNK_QWORD:
      case AML_CHUNK_ALIAS:
      case AML_CHUNK_NAME:
      case AML_CHUNK_RETURN:
      case AML_CHUNK_STRING:
        Offset = AmlWriteByte (Node->Type, Buffer, Offset);
        Offset = AmlWriteBuffer (Node->Buffer, Node->Length, Buffer, Offset);
        break;

      default:
        break;
    }

    while (Child) {
      Offset = AmlWriteNode (Child, Buffer, Offset);
      Child = Child->Next;
    }

    if ((Offset - Old) != Node->Size) {
      MsgLog ("Node Size incorrect: type=0x%x Size=%x Offset=%x\n", Node->Type,
             Node->Size, (Offset - Old));
    }
  }

  return Offset;
}
