 /*
 *  aml_generator.h
 *  Chameleon
 *
 *  Created by Mozodojo on 20/07/10.
 *  Copyright 2010 mozo. All rights reserved.
 *
 */

#ifndef _AML_GENERATOR_H
#define _AML_GENERATOR_H

#include <Library/Platform/Platform.h>

#define XXXX_SIGN     SIGNATURE_32 ('X','X','X','X')
//#define APIC_SIGN     SIGNATURE_32 ('A','P','I','C')
//#define SLIC_SIGN     SIGNATURE_32 ('S','L','I','C')

BOOLEAN     AmlAddToParent      (AML_CHUNK *Parent, AML_CHUNK *Node);
AML_CHUNK   *AmlCreateNode      (AML_CHUNK *Parent);
VOID        AmlDestroyNode      (AML_CHUNK *Node);
AML_CHUNK   *AmlAddBuffer       (AML_CHUNK *Parent, CHAR8 *Buffer, UINT32 Size);
AML_CHUNK   *AmlAddByte         (AML_CHUNK *Parent, UINT8 Value);
AML_CHUNK   *AmlAddWord         (AML_CHUNK *Parent, UINT16 Value);
AML_CHUNK   *AmlAddDword        (AML_CHUNK *Parent, UINT32 Value);
AML_CHUNK   *AmlAddQword        (AML_CHUNK *Parent, UINT64 Value);
AML_CHUNK   *AmlAddScope        (AML_CHUNK *Parent, CHAR8 *Name);
AML_CHUNK   *AmlAddName         (AML_CHUNK *Parent, CHAR8 *Name);
AML_CHUNK   *AmlAddMethod       (AML_CHUNK *Parent, CHAR8 *Name, UINT8 args);
AML_CHUNK   *AmlAddPackage      (AML_CHUNK *Parent);
AML_CHUNK   *AmlAddAlias        (AML_CHUNK *Parent, CHAR8 *Name1, CHAR8 *Name2);
AML_CHUNK   *AmlAddReturnName   (AML_CHUNK *Parent, CHAR8 *Name);
AML_CHUNK   *AmlAddReturnByte   (AML_CHUNK *Parent, UINT8 Value);
UINT32      AmlCalculateSize    (AML_CHUNK *Node);
UINT32      AmlWriteSize        (UINT32 Size, CHAR8 *Buffer, UINT32 Offset);
UINT32      AmlWriteNode        (AML_CHUNK *Node, CHAR8 *Buffer, UINT32 Offset);

// add by pcj
AML_CHUNK   *AmlAddString       (AML_CHUNK *Parent, CHAR8 *StringBuf);
AML_CHUNK   *AmlAddByteBuffer   (AML_CHUNK *Parent, CHAR8 *data, UINT32 Size);
AML_CHUNK   *AmlAddStringBuffer (AML_CHUNK *Parent, CHAR8 *StringBuf);
AML_CHUNK   *AmlAddDevice       (AML_CHUNK *Parent, CHAR8 *Name);
AML_CHUNK   *AmlAddLocal0       (AML_CHUNK *Parent);
AML_CHUNK   *AmlAddStore        (AML_CHUNK *Parent);
AML_CHUNK   *AmlAddReturn       (AML_CHUNK *Parent);

UINT32      AcpiGetSize         (UINT8 *Buffer, UINT32 Adr);

EFI_ACPI_DESCRIPTION_HEADER *
GeneratePssSsdt (
  UINT8   FirstID,
  UINTN   Number
);

EFI_ACPI_DESCRIPTION_HEADER *
GenerateCstSsdt (
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *Fadt,
  UINT8                                       FirstID,
  UINTN                                       Number
);

#endif

/* !_AML_GENERATOR_H */
