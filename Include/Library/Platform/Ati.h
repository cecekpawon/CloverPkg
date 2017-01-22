/*
 *  ati.h
 *
 *  Created by Slice on 19.02.12.
 *
 *  the code ported from Chameleon project as well as from RadeonFB by Joblo and RadeonHD by dong
 *  big thank to Islam M. Ahmed Zaid for the updating the collection
 */

#include <Library/Platform/Platform.h> //this include needed for Uefi types

#define OFFSET_TO_GET_ATOMBIOS_STRINGS_START 0x6e
#define DATVAL(x)     {kPtr, sizeof(x), (UINT8 *)x}
#define STRVAL(x)     {kStr, sizeof(x)-1, (UINT8 *)x}
#define BYTVAL(x)     {kCst, 1, (UINT8 *)(UINTN)x}
#define WRDVAL(x)     {kCst, 2, (UINT8 *)(UINTN)x}
#define DWRVAL(x)     {kCst, 4, (UINT8 *)(UINTN)x}
//QWRVAL would work only in 64 bit
//#define QWRVAL(x)     {kCst, 8, (UINT8 *)(UINTN)x}
#define NULVAL        {kNul, 0, (UINT8 *)NULL}

#define RADEON_CONFIG_MEMSIZE                     0x00f8
#define RADEON_CONFIG_APER_SIZE                   0x0108
#define RADEON_VIPH_CONTROL                       0x0c40
#define RADEON_BUS_CNTL                           0x0030
#define AVIVO_D1VGA_CONTROL                       0x0330
#define AVIVO_D2VGA_CONTROL                       0x0338
#define AVIVO_VGA_RENDER_CONTROL                  0x0300
#define R600_ROM_CNTL                             0x1600
#define RADEON_VIPH_EN                            (1 << 21)
#define RADEON_BUS_BIOS_DIS_ROM                   (1 << 12)
#define AVIVO_DVGA_CONTROL_MODE_ENABLE            (1<<0)
#define AVIVO_DVGA_CONTROL_TIMING_SELECT          (1<<8)
#define AVIVO_VGA_VSTATUS_CNTL_MASK               (3 << 16)
#define R600_CG_SPLL_FUNC_CNTL                    0x600
#define R600_SPLL_BYPASS_EN                       (1 << 3)
#define R600_CG_SPLL_STATUS                       0x60c
#define R600_SPLL_CHG_STATUS                      (1 << 1)
#define R600_SCK_OVERWRITE                        (1 << 1)
#define RADEON_CRTC_GEN_CNTL                      0x0050
#define RADEON_CRTC_EN                            (1 << 25)
#define RADEON_CRTC2_GEN_CNTL                     0x03f8
#define R600_GENERAL_PWRMGT                       0x618
#define R600_OPEN_DRAIN_PADS                      (1 << 11)
#define R600_CTXSW_VID_LOWER_GPIO_CNTL            0x718
#define R600_LOWER_GPIO_ENABLE                    0x710
#define R600_LOW_VID_LOWER_GPIO_CNTL              0x724
#define R600_MEDIUM_VID_LOWER_GPIO_CNTL           0x720
#define R600_HIGH_VID_LOWER_GPIO_CNTL             0x71c
#define R600_CONFIG_MEMSIZE                       0x5428
#define R600_SCK_PRESCALE_CRYSTAL_CLK_SHIFT       28
#define R600_SCK_PRESCALE_CRYSTAL_CLK_MASK        (0xf << 28)

/* Typedefs ENUMS */
typedef enum {
  kNul,
  kStr,
  kPtr,
  kCst
} type_t;

typedef enum {
  CHIP_FAMILY_UNKNOW,
  /* Old */
  CHIP_FAMILY_R420,
  CHIP_FAMILY_R423,
  CHIP_FAMILY_RV410,
  CHIP_FAMILY_RV515,
  CHIP_FAMILY_R520,
  CHIP_FAMILY_RV530,
  CHIP_FAMILY_RV560,
  CHIP_FAMILY_RV570,
  CHIP_FAMILY_R580,
  /* IGP */
  CHIP_FAMILY_RS600,
  CHIP_FAMILY_RS690,
  CHIP_FAMILY_RS740,
  CHIP_FAMILY_RS780,
  CHIP_FAMILY_RS880,
  /* R600 */
  CHIP_FAMILY_R600,
  CHIP_FAMILY_RV610,
  CHIP_FAMILY_RV620,
  CHIP_FAMILY_RV630,
  CHIP_FAMILY_RV635,
  CHIP_FAMILY_RV670,
  /* R700 */
  CHIP_FAMILY_RV710,
  CHIP_FAMILY_RV730,
  CHIP_FAMILY_RV740,
  CHIP_FAMILY_RV770,
  CHIP_FAMILY_RV772,
  CHIP_FAMILY_RV790,
  /* Evergreen */
  CHIP_FAMILY_CEDAR,
  CHIP_FAMILY_CYPRESS,
  CHIP_FAMILY_HEMLOCK,
  CHIP_FAMILY_JUNIPER,
  CHIP_FAMILY_REDWOOD,
  /* Northern Islands */
  CHIP_FAMILY_BARTS,
  CHIP_FAMILY_CAICOS,
  CHIP_FAMILY_CAYMAN,
  CHIP_FAMILY_TURKS,
  /* Southern Islands */
  CHIP_FAMILY_PALM,
  CHIP_FAMILY_SUMO,
  CHIP_FAMILY_SUMO2,
  CHIP_FAMILY_ARUBA,
  CHIP_FAMILY_TAHITI,
  CHIP_FAMILY_PITCAIRN,
  CHIP_FAMILY_VERDE,
  CHIP_FAMILY_OLAND,
  CHIP_FAMILY_HAINAN,
  CHIP_FAMILY_BONAIRE,
  CHIP_FAMILY_KAVERI,
  CHIP_FAMILY_KABINI,
  CHIP_FAMILY_HAWAII,
  /* ... */
  CHIP_FAMILY_MULLINS,
  CHIP_FAMILY_TOPAS,
  CHIP_FAMILY_AMETHYST,
  CHIP_FAMILY_TONGA,
  CHIP_FAMILY_LAST
} ati_chip_family_t;

typedef struct {
  CONST CHAR8   *name;
  UINT8 ports;
} card_config_t;

typedef enum {
  kNull,
  /* OLDController */
  kWormy,
  kAlopias,
  kCaretta,
  kKakapo,
  kKipunji,
  kPeregrine,
  kRaven,
  kSphyrna,
  /* AMD2400Controller */
  kIago,
  /* AMD2600Controller */
  kHypoprion,
  kLamna,
  /* AMD3800Controller */
  kMegalodon,
  kTriakis,
  /* AMD4600Controller */
  kFlicker,
  kGliff,
  kShrike,
  /* AMD4800Controller */
  kCardinal,
  kMotmot,
  kQuail,
  /* AMD5000Controller */
  kDouc,
  kLangur,
  kUakari,
  kZonalis,
  kAlouatta,
  kHoolock,
  kVervet,
  kBaboon,
  kEulemur,
  kGalago,
  kColobus,
  kMangabey,
  kNomascus,
  kOrangutan,
  /* AMD6000Controller */
  kPithecia,
  kBulrushes,
  kCattail,
  kHydrilla,
  kDuckweed,
  kFanwort,
  kElodea,
  kKudzu,
  kGibba,
  kLotus,
  kIpomoea,
  kMuskgrass,
  kJuncus,
  kOsmunda,
  kPondweed,
  kSpikerush,
  kTypha,
  /* AMD7000Controller */
  kNamako,
  kAji,
  kBuri,
  kChutoro,
  kDashimaki,
  kEbi,
  kGari,
  kFutomaki,
  kHamachi,
  kOPM,
  kIkura,
  kIkuraS,
  kJunsai,
  kKani,
  kKaniS,
  kDashimakiS,
  kMaguro,
  kMaguroS,
  /* AMD8000Controller */
  kBaladi,
  /* AMD9000Controller */
  kExmoor,
  kBasset,
  kGreyhound,
  kLabrador,
  kCfgEnd
} config_name_t;

typedef struct {
  UINT16                device_id;
  //UINT32              subsys_id;
  ati_chip_family_t     chip_family;
  //CONST CHAR8         *model_name;
  config_name_t         cfg_name;
} radeon_card_info_t;

typedef struct {
  DevPropDevice         *device;
  radeon_card_info_t    *info;
  pci_dt_t              *pci_dev;
  UINT8                 *fb;
  UINT8                 *mmio;
  UINT8                 *io;
  UINT8                 *rom;
  UINT32                rom_size;
  UINT64                vram_size;
  CONST CHAR8           *cfg_name;
  UINT8                 ports;
  UINT32                flags;
  BOOLEAN               posted;
} card_t;

#define MKFLAG(n)   (1 << n)
#define FLAGTRUE    MKFLAG(0)
#define EVERGREEN   MKFLAG(1)
#define FLAGMOBILE  MKFLAG(2)
#define FLAGOLD     MKFLAG(3)
#define FLAGNOTFAKE MKFLAG(4)

typedef struct {
  type_t    type;
  UINT32    size;
  UINT8   *data;
} value_t;

typedef struct {
  UINT32    flags;
  BOOLEAN   all_ports;
  CHAR8   *name;
  BOOLEAN   (*get_value) (value_t *val, INTN index);
  value_t   default_val;
} AtiDevProp;

BOOLEAN GetBootDisplayVal     (value_t *val, INTN index);
BOOLEAN GetVramVal            (value_t *val, INTN index);
BOOLEAN GetEdidVal            (value_t *val, INTN index);
BOOLEAN GetDisplayType        (value_t *val, INTN index);
BOOLEAN GetNameVal            (value_t *val, INTN index);
BOOLEAN GetNameParentVal      (value_t *val, INTN index);
BOOLEAN GetModelVal           (value_t *val, INTN index);
BOOLEAN GetConnTypeVal        (value_t *val, INTN index);
BOOLEAN GetVramSizeVal        (value_t *val, INTN index);
BOOLEAN GetBinImageVal        (value_t *val, INTN index);
BOOLEAN GetBinImageOwr        (value_t *val, INTN index);
BOOLEAN GetRomRevisionVal     (value_t *val, INTN index);
BOOLEAN GetDeviceIdVal        (value_t *val, INTN index);
BOOLEAN GetMclkVal            (value_t *val, INTN index);
BOOLEAN GetSclkVal            (value_t *val, INTN index);
BOOLEAN GetRefclkVal          (value_t *val, INTN index);
BOOLEAN GetPlatformInfoVal    (value_t *val, INTN index);
BOOLEAN GetVramTotalSizeVal   (value_t *val, INTN index);
BOOLEAN GetDualLinkVal        (value_t *val, INTN index);
BOOLEAN GetNamePciVal         (value_t *val, INTN index);

extern card_config_t card_configs[];
extern radeon_card_info_t radeon_cards[];
extern AtiDevProp ati_devprop_list[];
extern CONST CHAR8 *chip_family_name[];
