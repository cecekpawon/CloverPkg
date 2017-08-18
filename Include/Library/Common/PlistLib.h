/*
Headers collection for procedures
*/

#ifndef __REFIT_PLIST_H__
#define __REFIT_PLIST_H__

/* XML Tags */
typedef enum {
  kTagTypeNone,
  kTagTypeDict,
  kTagTypeKey,
  kTagTypeString,
  kTagTypeInteger,
  kTagTypeData,
  kTagTypeDate,
  kTagTypeFalse,
  kTagTypeTrue,
  kTagTypeArray
} TAG_TYPE;

#define kXMLTagPList        "plist"
#define kXMLTagDict         "dict"
#define kXMLTagKey          "key"
#define kXMLTagString       "string"
#define kXMLTagInteger      "integer"
#define kXMLTagData         "data"
#define kXMLTagDate         "date"
#define kXMLTagFalse        "false"
#define kXMLTagTrue         "true"
#define kXMLTagArray        "array"
#define kXMLTagReference    "reference"
#define kXMLTagID           "ID="
#define kXMLTagIDREF        "IDREF="
#define kXMLTagSIZE         "size="

typedef struct Symbol {
          UINTN   refCount;
          CHAR8   *string;
  struct  Symbol  *next;
} Symbol , *SymbolPtr;

typedef struct sREF {
          CHAR8   *string;
          INT32   id;
          INTN    integer;
          INTN    size;
  struct  sREF    *next;
} sREF;

typedef struct {
  UINTN   type;
  CHAR8   *string;
  INTN    integer;
  UINT8   *data;
  UINTN   size;
  VOID    *tag;
  VOID    *tagNext;
  UINTN   offset;
  UINTN   taglen;
  INT32   ref;
  INT32   id;
  sREF    *ref_strings;
  sREF    *ref_integer;
} TagStruct, *TagPtr;

CHAR8 *
EFIAPI
XMLDecode (
  CHAR8   *src
);

EFI_STATUS
EFIAPI
ParseXML (
  CHAR8   *Buffer,
  UINT32  BufSize,
  TagPtr  *Dict
);

TagPtr
EFIAPI
GetProperty (
  TagPtr  Dict,
  CHAR8   *Key
);

EFI_STATUS
EFIAPI
GetRefString (
  IN  TagPtr  Tag,
  IN  INT32   Id,
  OUT CHAR8   **Val,
  OUT INTN    *Size
);

EFI_STATUS
EFIAPI
GetRefInteger (
   IN TagPtr  Tag,
   IN INT32   Id,
  OUT CHAR8   **Val,
  OUT INTN    *DecVal,
  OUT INTN    *Size
);

VOID
EFIAPI
FreeTag (
  TagPtr  Tag
);

INTN
EFIAPI
GetTagCount (
  TagPtr  Dict
);

EFI_STATUS
EFIAPI
GetElement (
  TagPtr    Dict,
  INTN      Id,
  INTN      Count,
  TagPtr    *Dict1
);

BOOLEAN
EFIAPI
GetPropertyBool (
  TagPtr    Prop,
  BOOLEAN   Default
);

INTN
EFIAPI
GetPropertyInteger (
  TagPtr  Prop,
  INTN    Default
);

CHAR8 *
EFIAPI
GetPropertyString (
  TagPtr  Prop,
  CHAR8   *Default
);

VOID *
EFIAPI
GetDataSetting (
  IN   TagPtr   Dict,
  IN   CHAR8    *PropName,
  OUT  UINTN    *DataLen
);

VOID
EFIAPI
DumpBody (
  CHAR16  **Str,
  INT32   Depth
);

CHAR16 *
EFIAPI
DumpTag (
  TagPtr  Tag,
  INT32   Depth
);

#endif
