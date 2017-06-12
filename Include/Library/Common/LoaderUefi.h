/*
 * dmazar: UEFI wrapper for Mach-O definitions
 * xnu/EXTERNAL_HEADERS/mach-o/loader.h
 *
 */

#ifndef _UEFI_MACHO_LOADER_H_
#define _UEFI_MACHO_LOADER_H_

//
// Base UEFI types
//
#include <Include/Base.h>

typedef UINT8     uint8_t;
typedef UINT16    uint16_t;
typedef UINT32    uint32_t;
typedef UINT64    uint64_t;

typedef INT32     cpu_type_t;
typedef INT32     cpu_subtype_t;

typedef INT32     vm_prot_t;

struct mach_header_64 {
  UINT32          magic;           /* mach magic number identifier */
  cpu_type_t      cputype;          /* cpu specifier */
  cpu_subtype_t   cpusubtype;       /* machine specifier */
  UINT32          filetype;         /* type of file */
  UINT32          ncmds;            /* number of load commands */
  UINT32          sizeofcmds;       /* the size of all the load commands */
  UINT32          flags;            /* flags */
  UINT32          reserved;         /* reserved */
};

/* Constant for the magic field of the mach_header_64 (64-bit architectures) */
#define MH_MAGIC_64     0xfeedfacf  /* the 64-bit mach magic number */
#define MH_CIGAM_64     0xcffaedfe  /* NXSwapInt(MH_MAGIC_64) */
//#define LC_SEGMENT      0x1         /* segment of this file to be mapped */
#define LC_SYMTAB       0x2         /* link-edit stab symbol table info */
#define LC_UNIXTHREAD   0x5         /* unix thread (includes a stack) */
#define LC_SEGMENT_64   0x19        /* 64-bit segment of this file to be mapped */

struct segment_command_64 {         /* for 64-bit architectures */
  UINT32      cmd;                  /* LC_SEGMENT_64 */
  UINT32      cmdsize;              /* includes sizeof section_64 structs */
  CHAR8       segname[16];          /* segment name */
  UINT64      vmaddr;               /* memory address of this segment */
  UINT64      vmsize;               /* memory size of this segment */
  UINT64      fileoff;              /* file offset of this segment */
  UINT64      filesize;             /* amount to map from the file */
  vm_prot_t   maxprot;              /* maximum VM protection */
  vm_prot_t   initprot;             /* initial VM protection */
  UINT32      nsects;               /* number of sections in segment */
  UINT32      flags;                /* flags */
};

struct section_64 {                 /* for 64-bit architectures */
  CHAR8   sectname[16];             /* name of this section */
  CHAR8   segname[16];              /* segment this section goes in */
  UINT64  addr;                     /* memory address of this section */
  UINT64  size;                     /* size in bytes of this section */
  UINT32  offset;                   /* file offset of this section */
  UINT32  align;                    /* section alignment (power of 2) */
  UINT32  reloff;                   /* file offset of relocation entries */
  UINT32  nreloc;                   /* number of relocation entries */
  UINT32  flags;                    /* flags (section type and attributes)*/
  UINT32  reserved1;                /* reserved (for offset or index) */
  UINT32  reserved2;                /* reserved (for count or sizeof) */
  UINT32  reserved3;                /* reserved */
};

struct load_command {
  UINT32 cmd;                       /* type of load command */
  UINT32 cmdsize;                   /* total size of command in bytes */
};

/*
 * This is the symbol table entry structure for 64-bit architectures.
 */
struct nlist_64 {
    union {
      uint32_t  n_strx;   /* index into the string table */
    } n_un;

    uint8_t n_type;       /* type flag, see below */
    uint8_t n_sect;       /* section number or NO_SECT */
    uint16_t n_desc;      /* see <mach-o/stab.h> */
    uint64_t n_value;     /* value of this symbol (or stab offset) */
};

/*
 * The symtab_command contains the offsets and sizes of the link-edit 4.3BSD
 * "stab" style symbol table information as described in the header files
 * <nlist.h> and <stab.h>.
 */
struct symtab_command {
  uint32_t  cmd;      /* LC_SYMTAB */
  uint32_t  cmdsize;  /* sizeof (struct symtab_command) */
  uint32_t  symoff;   /* symbol table offset */
  uint32_t  nsyms;    /* number of symbol table entries */
  uint32_t  stroff;   /* string table offset */
  uint32_t  strsize;  /* string table size in bytes */
};

//
// From xnu/osfmk/mach/i386/_structs.h:
//
#define _STRUCT_X86_THREAD_STATE32  struct __darwin_i386_thread_state
_STRUCT_X86_THREAD_STATE32
{
    // all fields are unsigned int in xnu rources
  UINT32  eax;
  UINT32  ebx;
  UINT32  ecx;
  UINT32  edx;
  UINT32  edi;
  UINT32  esi;
  UINT32  ebp;
  UINT32  esp;
  UINT32  ss;
  UINT32  eflags;
  UINT32  eip;
  UINT32  cs;
  UINT32  ds;
  UINT32  es;
  UINT32  fs;
  UINT32  gs;
};

#define _STRUCT_X86_THREAD_STATE64  struct __darwin_x86_thread_state64
_STRUCT_X86_THREAD_STATE64
{
  UINT64  rax;
  UINT64  rbx;
  UINT64  rcx;
  UINT64  rdx;
  UINT64  rdi;
  UINT64  rsi;
  UINT64  rbp;
  UINT64  rsp;
  UINT64  r8;
  UINT64  r9;
  UINT64  r10;
  UINT64  r11;
  UINT64  r12;
  UINT64  r13;
  UINT64  r14;
  UINT64  r15;
  UINT64  rip;
  UINT64  rflags;
  UINT64  cs;
  UINT64  fs;
  UINT64  gs;
};

//
// From xnu/osfmk/mach/i386/thread_status.h:
//
typedef _STRUCT_X86_THREAD_STATE32    i386_thread_state_t;
typedef _STRUCT_X86_THREAD_STATE64    x86_thread_state64_t;

/*
 * What the booter leaves behind for the kernel.
 */

/*
 * Types of boot driver that may be loaded by the booter.
enum {
  kBootDriverTypeInvalid        = 0,
  kBootDriverTypeKEXT           = 1,
  kBootDriverTypeMKEXT          = 2
};

enum {
  kEfiReservedMemoryType        = 0,
  kEfiLoaderCode                = 1,
  kEfiLoaderData                = 2,
  kEfiBootServicesCode          = 3,
  kEfiBootServicesData          = 4,
  kEfiRuntimeServicesCode       = 5,
  kEfiRuntimeServicesData       = 6,
  kEfiConventionalMemory        = 7,
  kEfiUnusableMemory            = 8,
  kEfiACPIReclaimMemory         = 9,
  kEfiACPIMemoryNVS             = 10,
  kEfiMemoryMappedIO            = 11,
  kEfiMemoryMappedIOPortSpace   = 12,
  kEfiPalCode                   = 13,
  kEfiMaxMemoryType             = 14
};
 */

/*
 * Memory range descriptor.
typedef struct EfiMemoryRange {
  UINT32  Type;
  UINT32  Pad;
  UINT64  PhysicalStart;
  UINT64  VirtualStart;
  UINT64  NumberOfPages;
  UINT64  Attribute;
} EfiMemoryRange;
 */

#define BOOT_LINE_LENGTH        1024
#define BOOT_STRING_LEN         BOOT_LINE_LENGTH

/*
 * Video information..
 */
struct Boot_VideoV1 {
  UINT32  v_baseAddr; /* Base address of video memory */
  UINT32  v_display;  /* Display Code (if Applicable */
  UINT32  v_rowBytes; /* Number of bytes per pixel row */
  UINT32  v_width;  /* Width */
  UINT32  v_height; /* Height */
  UINT32  v_depth;  /* Pixel Depth */
};

typedef struct Boot_VideoV1 Boot_VideoV1;

struct Boot_Video {
  UINT32  v_display;  /* Display Code (if Applicable */
  UINT32  v_rowBytes; /* Number of bytes per pixel row */
  UINT32  v_width;  /* Width */
  UINT32  v_height; /* Height */
  UINT32  v_depth;  /* Pixel Depth */
  UINT32  v_resv[7];  /* Reserved */
  UINT64  v_baseAddr; /* Base address of video memory */
};

typedef struct Boot_Video Boot_Video;

/* Values for v_display */

#define VGA_TEXT_MODE           0
#define GRAPHICS_MODE           1
#define FB_TEXT_MODE            2

/* Boot argument structure - passed into Mach kernel at boot time.
 * "Revision" can be incremented for compatible changes
 */
#define kBootArgsRevision       0
#define kBootArgsVersion        2

//#define kBootArgsVersion2       2

#define kBootArgsEfiMode64      64

/* Bitfields for boot_args->flags */
#define kBootArgsFlagRebootOnPanic      (1 << 0)
#define kBootArgsFlagHiDPI              (1 << 1)
#define kBootArgsFlagBlack              (1 << 2)
#define kBootArgsFlagCSRActiveConfig    (1 << 3)
#define kBootArgsFlagCSRConfigMode      (1 << 4)
#define kBootArgsFlagCSRBoot            (1 << 5)
#define kBootArgsFlagBlackBg            (1 << 6)
#define kBootArgsFlagLoginUI            (1 << 7)
#define kBootArgsFlagInstallUI          (1 << 8)

/* Rootless configuration flags */
#define CSR_ALLOW_UNTRUSTED_KEXTS       (1 << 0)
#define CSR_ALLOW_UNRESTRICTED_FS       (1 << 1)
#define CSR_ALLOW_TASK_FOR_PID          (1 << 2)
#define CSR_ALLOW_KERNEL_DEBUGGER       (1 << 3)
#define CSR_ALLOW_APPLE_INTERNAL        (1 << 4)
//#define CSR_ALLOW_DESTRUCTIVE_DTRACE    (1 << 5) /* name deprecated */
#define CSR_ALLOW_UNRESTRICTED_DTRACE   (1 << 5)
#define CSR_ALLOW_UNRESTRICTED_NVRAM    (1 << 6)
#define CSR_ALLOW_DEVICE_CONFIGURATION  (1 << 7)
#define CSR_ALLOW_ANY_RECOVERY_OS       (1 << 8)

#define CSR_VALID_FLAGS (CSR_ALLOW_UNTRUSTED_KEXTS | \
                         CSR_ALLOW_UNRESTRICTED_FS | \
                         CSR_ALLOW_TASK_FOR_PID | \
                         CSR_ALLOW_KERNEL_DEBUGGER | \
                         CSR_ALLOW_APPLE_INTERNAL | \
                         CSR_ALLOW_UNRESTRICTED_DTRACE | \
                         CSR_ALLOW_UNRESTRICTED_NVRAM | \
                         CSR_ALLOW_DEVICE_CONFIGURATION | \
                         CSR_ALLOW_ANY_RECOVERY_OS)

/* CSR capabilities that a booter can give to the system */
#define CSR_CAPABILITY_UNLIMITED        (1 << 0)
#define CSR_CAPABILITY_CONFIG           (1 << 1)
#define CSR_CAPABILITY_APPLE_INTERNAL   (1 << 2)

#define CSR_VALID_CAPABILITIES (CSR_CAPABILITY_UNLIMITED | CSR_CAPABILITY_CONFIG | CSR_CAPABILITY_APPLE_INTERNAL)

typedef struct {
  UINT16          Revision; /* Revision of boot_args structure */
  UINT16          Version;  /* Version of boot_args structure */

  UINT8           efiMode;    /* 32 = 32-bit, 64 = 64-bit */
  UINT8           debugMode;  /* Bit field with behavior changes */
  UINT16          flags;

  CHAR8           CommandLine[BOOT_LINE_LENGTH];  /* Passed in command line */

  UINT32          MemoryMap;  /* Physical address of memory map */
  UINT32          MemoryMapSize;
  UINT32          MemoryMapDescriptorSize;
  UINT32          MemoryMapDescriptorVersion;

  Boot_VideoV1    VideoV1; /* Video Information */

  UINT32          deviceTreeP;    /* Physical address of flattened device tree */
  UINT32          deviceTreeLength; /* Length of flattened tree */

  UINT32          kaddr;            /* Physical address of beginning of kernel text */
  UINT32          ksize;            /* Size of combined kernel text+data+efi */

  UINT32          efiRuntimeServicesPageStart; /* physical address of defragmented runtime pages */
  UINT32          efiRuntimeServicesPageCount;
  UINT64          efiRuntimeServicesVirtualPageStart; /* virtual address of defragmented runtime pages */

  UINT32          efiSystemTable;   /* physical address of system table in runtime area */
  UINT32          kslide;

  UINT32          performanceDataStart; /* physical address of log */
  UINT32          performanceDataSize;

  UINT32          keyStoreDataStart; /* physical address of key store data */
  UINT32          keyStoreDataSize;
  UINT64          bootMemStart;
  UINT64          bootMemSize;
  UINT64          PhysicalMemorySize;
  UINT64          FSBFrequency;
  UINT64          pciConfigSpaceBaseAddress;
  UINT32          pciConfigSpaceStartBusNumber;
  UINT32          pciConfigSpaceEndBusNumber;
  UINT32          csrActiveConfig;
  UINT32          csrCapabilities;
  UINT32          boot_SMC_plimit;
  UINT16          bootProgressMeterStart;
  UINT16          bootProgressMeterEnd;
  Boot_Video      Video;    /* Video Information */

  UINT32          apfsDataStart; /* Physical address of apfs volume key structure */
  UINT32          apfsDataSize;

  UINT32          __reserved4[710];
} BootArgs2;

typedef struct {
  UINT16  NumSections;
  UINT32  TextVA;
  UINT32  TextOffset;
  UINT32  TextSize;
  UINT32  DataVA;
  UINT32  DataOffset;
  UINT32  DataSize;
} BOOT_EFI_HEADER;


#define DARWIN_KERNEL_VER_MAJOR_MAVERICKS       13
#define DARWIN_KERNEL_VER_MAJOR_YOSEMITE        14
#define DARWIN_KERNEL_VER_MAJOR_ELCAPITAN       15
#define DARWIN_KERNEL_VER_MAJOR_SIERRA          16
#define DARWIN_KERNEL_VER_MAJOR_HIGHSIERRA      17

#define DARWIN_OS_VER_MAJOR_10                  10

#define DARWIN_OS_VER_MINOR_MAVERICKS           9
#define DARWIN_OS_VER_MINOR_YOSEMITE            10
#define DARWIN_OS_VER_MINOR_ELCAPITAN           11
#define DARWIN_OS_VER_MINOR_SIERRA              12
#define DARWIN_OS_VER_MINOR_HIGHSIERRA          13

#define DARWIN_OS_VER_STR_MAVERICKS             "10.9"
#define DARWIN_OS_VER_STR_YOSEMITE              "10.10"
#define DARWIN_OS_VER_STR_ELCAPITAN             "10.11"
#define DARWIN_OS_VER_STR_SIERRA                "10.12"
#define DARWIN_OS_VER_STR_HIGHSIERRA            "10.13"

#define DARWIN_KERNEL_VER_MAJOR_MINIMUM         DARWIN_KERNEL_VER_MAJOR_MAVERICKS
#define DARWIN_OS_VER_MAJOR_MINIMUM             DARWIN_OS_VER_MAJOR_10
#define DARWIN_OS_VER_MINOR_MINIMUM             DARWIN_OS_VER_MINOR_MAVERICKS
#define DARWIN_OS_VER_STR_MINIMUM               DARWIN_OS_VER_STR_MAVERICKS

#define DARWIN_OS_VER_MINIMUM(Major, Minor)     (\
                                                  (Major > DARWIN_OS_VER_MAJOR_MINIMUM) || \
                                                  ((Major == DARWIN_OS_VER_MAJOR_MINIMUM) && (Minor >= DARWIN_OS_VER_MINOR_MINIMUM)) \
                                                )

#endif /* _UEFI_MACHO_LOADER_H_ */
