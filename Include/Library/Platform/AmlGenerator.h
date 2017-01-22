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

BOOLEAN     AmlAddToParent      (AML_CHUNK *parent, AML_CHUNK *node);
AML_CHUNK   *AmlCreateNode      (AML_CHUNK *parent);
VOID        AmlDestroyNode      (AML_CHUNK *node);
AML_CHUNK   *AmlAddBuffer       (AML_CHUNK *parent, CHAR8 *buffer, UINT32 size);
AML_CHUNK   *AmlAddByte         (AML_CHUNK *parent, UINT8 value);
AML_CHUNK   *AmlAddWord         (AML_CHUNK *parent, UINT16 value);
AML_CHUNK   *AmlAddDword        (AML_CHUNK *parent, UINT32 value);
AML_CHUNK   *AmlAddQword        (AML_CHUNK *parent, UINT64 value);
AML_CHUNK   *AmlAddScope        (AML_CHUNK *parent, CHAR8 *name);
AML_CHUNK   *AmlAddName         (AML_CHUNK *parent, CHAR8 *name);
AML_CHUNK   *AmlAddMethod       (AML_CHUNK *parent, CHAR8 *name, UINT8 args);
AML_CHUNK   *AmlAddPackage      (AML_CHUNK *parent);
AML_CHUNK   *AmlAddAlias        (AML_CHUNK *parent, CHAR8 *name1, CHAR8 *name2);
AML_CHUNK   *AmlAddReturnName   (AML_CHUNK *parent, CHAR8 *name);
AML_CHUNK   *AmlAddReturnByte   (AML_CHUNK *parent, UINT8 value);
UINT32      AmlCalculateSize    (AML_CHUNK *node);
UINT32      AmlWriteSize        (UINT32 size, CHAR8 *buffer, UINT32 offset);
UINT32      AmlWriteNode        (AML_CHUNK *node, CHAR8 *buffer, UINT32 offset);

// add by pcj
AML_CHUNK   *AmlAddString       (AML_CHUNK *parent, CHAR8 *string);
AML_CHUNK   *AmlAddByteBuffer   (AML_CHUNK *parent, CHAR8 *data, UINT32 size);
AML_CHUNK   *AmlAddStringBuffer (AML_CHUNK *parent, CHAR8 *string);
AML_CHUNK   *AmlAddDevice       (AML_CHUNK *parent, CHAR8 *name);
AML_CHUNK   *AmlAddLocal0       (AML_CHUNK *parent);
AML_CHUNK   *AmlAddStore        (AML_CHUNK *parent);
AML_CHUNK   *AmlAddReturn       (AML_CHUNK *parent);

UINT32      AcpiGetSize         (UINT8 *Buffer, UINT32 adr);

typedef     EFI_ACPI_DESCRIPTION_HEADER SSDT_TABLE;

SSDT_TABLE *
GeneratePssSsdt (
  UINT8   FirstID,
  UINTN   Number
);

SSDT_TABLE *
GenerateCstSsdt (
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE   *fadt,
  UINT8                                       FirstID,
  UINTN                                       Number
);

#endif

/* !_AML_GENERATOR_H */
