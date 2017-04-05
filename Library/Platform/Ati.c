/*
 * ATI Graphics Card Enabler, part of the Chameleon Boot Loader Project
 *
 * Copyright 2010 by Islam M. Ahmed Zaid. All rights reserved.
 *
 */

#include <Library/Platform/Platform.h>
#include <Library/Platform/Ati.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_ATI
#define DEBUG_ATI -1
#endif
#else
#ifdef DEBUG_ATI
#undef DEBUG_ATI
#endif
#define DEBUG_ATI DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_ATI, __VA_ARGS__)

#define S_ATIMODEL "ATI/AMD Graphics"

//STATIC ATI_VAL  ATYModel;
STATIC  ATI_VAL   ATYName;
STATIC  ATI_VAL   ATYNameParent;
        ATI_CARD  *Card;

ATI_CARD_CONFIG   ATICardConfigs[] = {
  { NULL, 0 },
  /* OLDController */
  { "Wormy", 2 },
  { "Alopias", 2 },
  { "Caretta", 1 },
  { "Kakapo", 3 },
  { "Kipunji", 4 },
  { "Peregrine", 2 },
  { "Raven", 3 },
  { "Sphyrna", 1 },
  /* AMD2400Controller */
  { "Iago", 2 },
  /* AMD2600Controller */
  { "Hypoprion", 2 },
  { "Lamna", 2 },
  /* AMD3800Controller */
  { "Megalodon", 3 },
  { "Triakis", 2 },
  /* AMD4600Controller */
  { "Flicker", 3 },
  { "Gliff", 3 },
  { "Shrike", 3 },
  /* AMD4800Controller */
  { "Cardinal", 2 },
  { "Motmot", 2 },
  { "Quail", 3 },
  /* AMD5000Controller */
  { "Douc", 2 },
  { "Langur", 3 },
  { "Uakari", 4 },
  { "Zonalis", 6 },
  { "Alouatta", 4 },
  { "Hoolock", 1 },
  { "Vervet", 4 },
  { "Baboon", 3 },
  { "Eulemur", 3 },
  { "Galago", 2 },
  { "Colobus", 2 },
  { "Mangabey", 2 },
  { "Nomascus", 5 },
  { "Orangutan", 2 },
  /* AMD6000Controller */
  { "Pithecia", 3 },
  { "Bulrushes", 6 },
  { "Cattail", 4 },
  { "Hydrilla", 5 },
  { "Duckweed", 4 },
  { "Fanwort", 4 },
  { "Elodea", 5 },
  { "Kudzu", 2 },
  { "Gibba", 5 },
  { "Lotus", 3 },
  { "Ipomoea", 3 },
  { "Muskgrass", 4 },
  { "Juncus", 4 },
  { "Osmunda", 4 },
  { "Pondweed", 3 },
  { "Spikerush", 4 },
  { "Typha", 5 },
  /* AMD7000Controller */
  { "Namako", 4 },
  { "Aji", 4 },
  { "Buri", 4 },
  { "Chutoro", 5 },
  { "Dashimaki", 4 },
  { "Ebi", 5 },
  { "Gari", 5 },
  { "Futomaki", 5 },
  { "Hamachi", 4 },
  { "OPM", 6 },
  { "Ikura", 1 },
  { "IkuraS", 6 },
  { "Junsai", 6 },
  { "Kani", 1 },
  { "KaniS", 6 },
  { "DashimakiS", 4 },
  { "Maguro", 1 },
  { "MaguroS", 6 },
  /* AMD8000Controller */
  { "Baladi", 6 },
  /* AMD9000Controller */
  { "Exmoor", 4 },
  { "Basset", 4 },
  { "Greyhound", 6 },
  { "Labrador", 6 },
};

ATI_CARD_INFO   ATICards[] = {

  // Earlier cards are not supported
  //
  // Layout is device_id, fake_id, ATIFamilyName, display name, frame buffer
  // Cards are grouped by device id  to make it easier to add new cards
  //

  // OLAND

  // Oland: R7-240, 250  - Southand Island
  // Earlier cards are not supported
  //
  // Layout is device_id, fake_id, ATIFamilyName, display name, frame buffer
  // Cards are grouped by device id  to make it easier to add new cards
  //

  { 0x6600, CHIP_FAMILY_OLAND, kNull },
  { 0x6601, CHIP_FAMILY_OLAND, kNull },
  //{ 0x6602, CHIP_FAMILY_OLAND, kNull },
  //{ 0x6603, CHIP_FAMILY_OLAND, kNull },
  { 0x6604, CHIP_FAMILY_OLAND, kNull },
  { 0x6605, CHIP_FAMILY_OLAND, kNull },
  { 0x6606, CHIP_FAMILY_OLAND, kNull },
  { 0x6607, CHIP_FAMILY_OLAND, kNull },
  { 0x6608, CHIP_FAMILY_OLAND, kNull },
  { 0x6610, CHIP_FAMILY_OLAND, kFutomaki },
  { 0x6611, CHIP_FAMILY_OLAND, kNull },
  { 0x6613, CHIP_FAMILY_OLAND, kFutomaki },
  //{ 0x6620, CHIP_FAMILY_OLAND, kNull },
  //{ 0x6621, CHIP_FAMILY_OLAND, kNull },
  //{ 0x6623, CHIP_FAMILY_OLAND, kNull },
  //{ 0x6631, CHIP_FAMILY_OLAND, kNull },

  // BONAIRE - Sea Island
  { 0x6640, CHIP_FAMILY_BONAIRE, kNull },
  { 0x6641, CHIP_FAMILY_BONAIRE, kNull },
  { 0x6646, CHIP_FAMILY_BONAIRE, kNull },
  { 0x6647, CHIP_FAMILY_BONAIRE, kNull },
  { 0x6649, CHIP_FAMILY_BONAIRE, kNull },
  //{ 0x6650, CHIP_FAMILY_BONAIRE, kNull },
  //{ 0x6651, CHIP_FAMILY_BONAIRE, kNull },
  { 0x6658, CHIP_FAMILY_BONAIRE, kNull },
  { 0x665C, CHIP_FAMILY_BONAIRE, kFutomaki },
  { 0x665D, CHIP_FAMILY_BONAIRE, kFutomaki },
  { 0x665F, CHIP_FAMILY_BONAIRE, kFutomaki },

  // HAINAN - Southand Island
  { 0x6660, CHIP_FAMILY_HAINAN, kNull },
  { 0x6663, CHIP_FAMILY_HAINAN, kNull },
  { 0x6664, CHIP_FAMILY_HAINAN, kNull },
  { 0x6665, CHIP_FAMILY_HAINAN, kNull },
  { 0x6667, CHIP_FAMILY_HAINAN, kNull },
  { 0x666F, CHIP_FAMILY_HAINAN, kNull },

  // CAYMAN
  { 0x6701, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6702, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6703, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6704, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6705, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6706, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6707, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6708, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6709, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6718, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x6719, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x671C, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x671D, CHIP_FAMILY_CAYMAN, kLotus },
  { 0x671F, CHIP_FAMILY_CAYMAN, kLotus },

  // BARTS
  { 0x6720, CHIP_FAMILY_BARTS, kFanwort },
  { 0x6722, CHIP_FAMILY_BARTS, kFanwort },
  { 0x6729, CHIP_FAMILY_BARTS, kFanwort },
  { 0x6738, CHIP_FAMILY_BARTS, kDuckweed },
  { 0x6739, CHIP_FAMILY_BARTS, kDuckweed },
  { 0x673E, CHIP_FAMILY_BARTS, kDuckweed },

  // TURKS
  { 0x6740, CHIP_FAMILY_TURKS, kCattail },
  { 0x6741, CHIP_FAMILY_TURKS, kCattail },
  { 0x6742, CHIP_FAMILY_TURKS, kCattail },
  { 0x6745, CHIP_FAMILY_TURKS, kCattail },
  { 0x6749, CHIP_FAMILY_TURKS, kPithecia },
  { 0x674A, CHIP_FAMILY_TURKS, kPithecia },
  { 0x6750, CHIP_FAMILY_TURKS, kPithecia },
  { 0x6758, CHIP_FAMILY_TURKS, kPithecia },
  { 0x6759, CHIP_FAMILY_TURKS, kPithecia },
  { 0x675B, CHIP_FAMILY_TURKS, kPithecia },
  { 0x675D, CHIP_FAMILY_TURKS, kCattail },
  { 0x675F, CHIP_FAMILY_TURKS, kPithecia },

  // CAICOS
  { 0x6760, CHIP_FAMILY_CAICOS, kHydrilla },
  { 0x6761, CHIP_FAMILY_CAICOS, kHydrilla },
  { 0x6763, CHIP_FAMILY_CAICOS, kHydrilla },
  { 0x6768, CHIP_FAMILY_CAICOS, kHydrilla },
  { 0x6770, CHIP_FAMILY_CAICOS, kBulrushes },
  { 0x6771, CHIP_FAMILY_CAICOS, kBulrushes },
  { 0x6772, CHIP_FAMILY_CAICOS, kBulrushes },
  { 0x6778, CHIP_FAMILY_CAICOS, kBulrushes },
  { 0x6779, CHIP_FAMILY_CAICOS, kBulrushes },
  { 0x677B, CHIP_FAMILY_CAICOS, kBulrushes },

  // TAHITI
  //Framebuffers: Aji - 4 Desktop, Buri - 4 Mobile, Chutoro - 5 Mobile,  Dashimaki - 4, IkuraS - HMDI
  // Ebi - 5 Mobile, Gari - 5 M, Futomaki - 4 D, Hamachi - 4 D, OPM - 6 Server, Ikura - 6
  { 0x6780, CHIP_FAMILY_TAHITI, kIkuraS },
  { 0x6784, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x6788, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x678A, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x6790, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x6791, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x6792, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x6798, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x6799, CHIP_FAMILY_TAHITI, kAji },
  { 0x679A, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x679B, CHIP_FAMILY_TAHITI, kChutoro },
  { 0x679E, CHIP_FAMILY_TAHITI, kFutomaki },
  { 0x679F, CHIP_FAMILY_TAHITI, kFutomaki },

  // HAWAII - Sea Island
  //{ 0x67A0, CHIP_FAMILY_HAWAII, kFutomaki },
  //{ 0x67A1, CHIP_FAMILY_HAWAII, kFutomaki },
  //{ 0x67A2, CHIP_FAMILY_HAWAII, kFutomaki },
  //{ 0x67A8, CHIP_FAMILY_HAWAII, kFutomaki },
  //{ 0x67A9, CHIP_FAMILY_HAWAII, kFutomaki },
  //{ 0x67AA, CHIP_FAMILY_HAWAII, kFutomaki },
  { 0x67B0, CHIP_FAMILY_HAWAII, kBaladi },
  { 0x67B1, CHIP_FAMILY_HAWAII, kBaladi },
  //{ 0x67B8, CHIP_FAMILY_HAWAII, kFutomaki },
  { 0x67B9, CHIP_FAMILY_HAWAII, kFutomaki },
  //{ 0x67BA, CHIP_FAMILY_HAWAII, kFutomaki },
  //{ 0x67BE, CHIP_FAMILY_HAWAII, kFutomaki },

  //Polaris
  { 0x67C0, CHIP_FAMILY_HAWAII, kNull },
  { 0x67DF, CHIP_FAMILY_HAWAII, kNull },
  { 0x67E0, CHIP_FAMILY_HAWAII, kNull },
  { 0x67EF, CHIP_FAMILY_HAWAII, kNull },
  { 0x67FF, CHIP_FAMILY_HAWAII, kNull },

  // PITCAIRN
  { 0x6800, CHIP_FAMILY_PITCAIRN, kBuri },
  { 0x6801, CHIP_FAMILY_PITCAIRN, kFutomaki },
  //{ 0x6802, CHIP_FAMILY_PITCAIRN, kFutomaki },
  { 0x6806, CHIP_FAMILY_PITCAIRN, kFutomaki },
  { 0x6808, CHIP_FAMILY_PITCAIRN, kFutomaki },
  { 0x6809, CHIP_FAMILY_PITCAIRN, kNull },
  // Curacao
  { 0x6810, CHIP_FAMILY_PITCAIRN, kNamako },
  { 0x6811, CHIP_FAMILY_PITCAIRN, kFutomaki },
  //{ 0x6816, CHIP_FAMILY_PITCAIRN, kFutomaki },
  //{ 0x6817, CHIP_FAMILY_PITCAIRN, kFutomaki },
  { 0x6818, CHIP_FAMILY_PITCAIRN, kFutomaki },
  { 0x6819, CHIP_FAMILY_PITCAIRN, kFutomaki },

  // VERDE
  { 0x6820, CHIP_FAMILY_VERDE, kBuri },
  { 0x6821, CHIP_FAMILY_VERDE, kBuri },
  { 0x6822, CHIP_FAMILY_VERDE, kBuri },
  { 0x6823, CHIP_FAMILY_VERDE, kBuri },
  //{ 0x6824, CHIP_FAMILY_VERDE, kBuri },
  { 0x6825, CHIP_FAMILY_VERDE, kChutoro },
  { 0x6826, CHIP_FAMILY_VERDE, kBuri },
  { 0x6827, CHIP_FAMILY_VERDE, kChutoro },
  { 0x6828, CHIP_FAMILY_VERDE, kBuri },
  //{ 0x6829, CHIP_FAMILY_VERDE, kBuri },
  //{ 0x682A, CHIP_FAMILY_VERDE, kBuri },
  { 0x682B, CHIP_FAMILY_VERDE, kBuri },
  { 0x682D, CHIP_FAMILY_VERDE, kBuri },
  { 0x682F, CHIP_FAMILY_VERDE, kBuri },
  { 0x6830, CHIP_FAMILY_VERDE, kBuri },
  { 0x6831, CHIP_FAMILY_VERDE, kBuri },
  { 0x6835, CHIP_FAMILY_VERDE, kBuri },
  { 0x6837, CHIP_FAMILY_VERDE, kFutomaki },
  { 0x6838, CHIP_FAMILY_VERDE, kFutomaki },
  { 0x6839, CHIP_FAMILY_VERDE, kFutomaki },
  { 0x683B, CHIP_FAMILY_VERDE, kFutomaki },
  { 0x683D, CHIP_FAMILY_VERDE, kFutomaki },
  { 0x683F, CHIP_FAMILY_VERDE, kFutomaki },

  //actually they are controlled by 6000Controller
  // TURKS
  { 0x6840, CHIP_FAMILY_TURKS, kPondweed },
  { 0x6841, CHIP_FAMILY_TURKS, kPondweed },
  { 0x6842, CHIP_FAMILY_TURKS, kPondweed },
  { 0x6843, CHIP_FAMILY_TURKS, kPondweed },
  { 0x6849, CHIP_FAMILY_TURKS, kPondweed },

  // PITCAIRN
  //  { 0x684C, CHIP_FAMILY_PITCAIRN, kNull },

  // TURKS
  { 0x6850, CHIP_FAMILY_TURKS, kPondweed },
  { 0x6858, CHIP_FAMILY_TURKS, kPondweed },
  { 0x6859, CHIP_FAMILY_TURKS, kPondweed },

  // CYPRESS
  //  { 0x6880, CHIP_FAMILY_CYPRESS, kNull },
  { 0x6888, CHIP_FAMILY_CYPRESS, kNull },
  { 0x6889, CHIP_FAMILY_CYPRESS, kNull },
  { 0x688A, CHIP_FAMILY_CYPRESS, kNull },
  { 0x688C, CHIP_FAMILY_CYPRESS, kZonalis },
  { 0x688D, CHIP_FAMILY_CYPRESS, kZonalis },
  { 0x6898, CHIP_FAMILY_CYPRESS, kUakari },
  { 0x6899, CHIP_FAMILY_CYPRESS, kUakari },
  { 0x689B, CHIP_FAMILY_CYPRESS, kNull },

  // HEMLOCK
  { 0x689C, CHIP_FAMILY_HEMLOCK, kUakari },
  { 0x689D, CHIP_FAMILY_HEMLOCK, kUakari },

  // CYPRESS
  { 0x689E, CHIP_FAMILY_CYPRESS, kUakari },

  // JUNIPER
  { 0x68A0, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68A1, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68A8, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68A9, CHIP_FAMILY_JUNIPER, kHoolock },
  // was Vervet but Hoolock is better.
  // doesn't matter if you made connectors patch
  { 0x68B0, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68B1, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68B8, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68B9, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68BA, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68BC, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68BD, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68BE, CHIP_FAMILY_JUNIPER, kHoolock },
  { 0x68BF, CHIP_FAMILY_JUNIPER, kHoolock },

  // REDWOOD
  { 0x68C0, CHIP_FAMILY_REDWOOD, kGalago },
  { 0x68C1, CHIP_FAMILY_REDWOOD, kGalago },
  { 0x68C7, CHIP_FAMILY_REDWOOD, kGalago },
  { 0x68C8, CHIP_FAMILY_REDWOOD, kBaboon },
  { 0x68C9, CHIP_FAMILY_REDWOOD, kBaboon },
  { 0x68D8, CHIP_FAMILY_REDWOOD, kBaboon },
  { 0x68D9, CHIP_FAMILY_REDWOOD, kBaboon },
  { 0x68DA, CHIP_FAMILY_REDWOOD, kBaboon },
  { 0x68DE, CHIP_FAMILY_REDWOOD, kNull },

  // CEDAR
  { 0x68E0, CHIP_FAMILY_CEDAR, kGalago },
  { 0x68E1, CHIP_FAMILY_CEDAR, kGalago },
  { 0x68E4, CHIP_FAMILY_CEDAR, kGalago },
  { 0x68E5, CHIP_FAMILY_CEDAR, kGalago },
  //{ 0x68E8, CHIP_FAMILY_CEDAR, kNull },
  //{ 0x68E9, CHIP_FAMILY_CEDAR, kNull },
  { 0x68F1, CHIP_FAMILY_CEDAR, kEulemur },
  { 0x68F2, CHIP_FAMILY_CEDAR, kEulemur },
  //{ 0x68F8, CHIP_FAMILY_CEDAR, kNull },
  { 0x68F9, CHIP_FAMILY_CEDAR, kEulemur },
  { 0x68FA, CHIP_FAMILY_CEDAR, kEulemur },
  //{ 0x68FE, CHIP_FAMILY_CEDAR, kNull },

  // Volcanic Island
  { 0x6900, CHIP_FAMILY_TOPAS, kExmoor },
  { 0x6901, CHIP_FAMILY_TOPAS, kExmoor },
  { 0x6920, CHIP_FAMILY_AMETHYST, kLabrador },
  { 0x6921, CHIP_FAMILY_AMETHYST, kExmoor },
  { 0x692b, CHIP_FAMILY_TONGA, kBaladi },
  { 0x6938, CHIP_FAMILY_AMETHYST, kExmoor },
  { 0x6939, CHIP_FAMILY_TONGA, kBaladi },

  // RV515
  { 0x7140, CHIP_FAMILY_RV515, kCaretta },
  { 0x7141, CHIP_FAMILY_RV515, kCaretta },
  { 0x7142, CHIP_FAMILY_RV515, kCaretta },
  { 0x7143, CHIP_FAMILY_RV515, kCaretta },
  { 0x7144, CHIP_FAMILY_RV515, kCaretta },
  { 0x7145, CHIP_FAMILY_RV515, kCaretta },
  //7146, 7187 - Caretta
  { 0x7146, CHIP_FAMILY_RV515, kCaretta },
  { 0x7147, CHIP_FAMILY_RV515, kCaretta },
  { 0x7149, CHIP_FAMILY_RV515, kCaretta },
  { 0x714A, CHIP_FAMILY_RV515, kCaretta },
  { 0x714B, CHIP_FAMILY_RV515, kCaretta },
  { 0x714C, CHIP_FAMILY_RV515, kCaretta },
  { 0x714D, CHIP_FAMILY_RV515, kCaretta },
  { 0x714E, CHIP_FAMILY_RV515, kCaretta },
  { 0x714F, CHIP_FAMILY_RV515, kCaretta },
  { 0x7151, CHIP_FAMILY_RV515, kCaretta },
  { 0x7152, CHIP_FAMILY_RV515, kCaretta },
  { 0x7153, CHIP_FAMILY_RV515, kCaretta },
  { 0x715E, CHIP_FAMILY_RV515, kCaretta },
  { 0x715F, CHIP_FAMILY_RV515, kCaretta },
  { 0x7180, CHIP_FAMILY_RV515, kCaretta },
  { 0x7181, CHIP_FAMILY_RV515, kCaretta },
  { 0x7183, CHIP_FAMILY_RV515, kCaretta },
  { 0x7186, CHIP_FAMILY_RV515, kCaretta },
  { 0x7187, CHIP_FAMILY_RV515, kCaretta },
  { 0x7188, CHIP_FAMILY_RV515, kCaretta },
  { 0x718A, CHIP_FAMILY_RV515, kCaretta },
  { 0x718B, CHIP_FAMILY_RV515, kCaretta },
  { 0x718C, CHIP_FAMILY_RV515, kCaretta },
  { 0x718D, CHIP_FAMILY_RV515, kCaretta },
  { 0x718F, CHIP_FAMILY_RV515, kCaretta },
  { 0x7193, CHIP_FAMILY_RV515, kCaretta },
  { 0x7196, CHIP_FAMILY_RV515, kCaretta },
  { 0x719B, CHIP_FAMILY_RV515, kCaretta },
  { 0x719F, CHIP_FAMILY_RV515, kCaretta },

  // RV530
  { 0x71C0, CHIP_FAMILY_RV530, kWormy },
  { 0x71C1, CHIP_FAMILY_RV530, kWormy },
  { 0x71C2, CHIP_FAMILY_RV530, kWormy },
  { 0x71C3, CHIP_FAMILY_RV530, kWormy },
  { 0x71C4, CHIP_FAMILY_RV530, kWormy },
  //71c5 -Wormy
  { 0x71C5, CHIP_FAMILY_RV530, kWormy },
  { 0x71C6, CHIP_FAMILY_RV530, kWormy },
  { 0x71C7, CHIP_FAMILY_RV530, kWormy },
  { 0x71CD, CHIP_FAMILY_RV530, kWormy },
  { 0x71CE, CHIP_FAMILY_RV530, kWormy },
  { 0x71D2, CHIP_FAMILY_RV530, kWormy },
  { 0x71D4, CHIP_FAMILY_RV530, kWormy },
  { 0x71D5, CHIP_FAMILY_RV530, kWormy },
  { 0x71D6, CHIP_FAMILY_RV530, kWormy },
  { 0x71DA, CHIP_FAMILY_RV530, kWormy },
  { 0x71DE, CHIP_FAMILY_RV530, kWormy },

  // RV515
  { 0x7200, CHIP_FAMILY_RV515, kWormy },
  { 0x7210, CHIP_FAMILY_RV515, kWormy },
  { 0x7211, CHIP_FAMILY_RV515, kWormy },

  //
  { 0x9400, CHIP_FAMILY_R600, kNull },
  { 0x9401, CHIP_FAMILY_R600, kNull },
  { 0x9402, CHIP_FAMILY_R600, kNull },
  { 0x9403, CHIP_FAMILY_R600, kNull },
  { 0x9405, CHIP_FAMILY_R600, kNull },
  { 0x940A, CHIP_FAMILY_R600, kNull },
  { 0x940B, CHIP_FAMILY_R600, kNull },
  { 0x940F, CHIP_FAMILY_R600, kNull },

  // RV740
  { 0x94A0, CHIP_FAMILY_RV740, kFlicker },
  { 0x94A1, CHIP_FAMILY_RV740, kFlicker },
  { 0x94A3, CHIP_FAMILY_RV740, kFlicker },
  { 0x94B1, CHIP_FAMILY_RV740, kFlicker },
  { 0x94B3, CHIP_FAMILY_RV740, kFlicker },
  { 0x94B4, CHIP_FAMILY_RV740, kFlicker },
  { 0x94B5, CHIP_FAMILY_RV740, kFlicker },
  { 0x94B9, CHIP_FAMILY_RV740, kFlicker },

  //9440, 944A - Cardinal
  // RV770
  { 0x9440, CHIP_FAMILY_RV770, kMotmot },
  { 0x9441, CHIP_FAMILY_RV770, kMotmot },
  { 0x9442, CHIP_FAMILY_RV770, kMotmot },
  { 0x9443, CHIP_FAMILY_RV770, kMotmot },
  { 0x9444, CHIP_FAMILY_RV770, kMotmot },
  { 0x9446, CHIP_FAMILY_RV770, kMotmot },
  { 0x9447, CHIP_FAMILY_RV770, kMotmot },
  { 0x944A, CHIP_FAMILY_RV770, kMotmot },
  { 0x944B, CHIP_FAMILY_RV770, kMotmot },
  { 0x944C, CHIP_FAMILY_RV770, kMotmot },
  { 0x944E, CHIP_FAMILY_RV770, kMotmot },
  { 0x9450, CHIP_FAMILY_RV770, kMotmot },
  { 0x9452, CHIP_FAMILY_RV770, kMotmot },
  { 0x9456, CHIP_FAMILY_RV770, kMotmot },
  { 0x945A, CHIP_FAMILY_RV770, kMotmot },
  { 0x9460, CHIP_FAMILY_RV770, kMotmot },
  { 0x9462, CHIP_FAMILY_RV770, kMotmot },

  //9488, 9490 - Gliff
  // RV730
  { 0x9480, CHIP_FAMILY_RV730, kGliff },
  { 0x9487, CHIP_FAMILY_RV730, kGliff },
  { 0x9488, CHIP_FAMILY_RV730, kGliff },
  { 0x9489, CHIP_FAMILY_RV730, kGliff },
  { 0x948A, CHIP_FAMILY_RV730, kGliff },
  { 0x948F, CHIP_FAMILY_RV730, kGliff },
  { 0x9490, CHIP_FAMILY_RV730, kGliff },
  { 0x9491, CHIP_FAMILY_RV730, kGliff },
  { 0x9495, CHIP_FAMILY_RV730, kGliff },
  { 0x9498, CHIP_FAMILY_RV730, kGliff },
  { 0x949C, CHIP_FAMILY_RV730, kGliff },
  { 0x949E, CHIP_FAMILY_RV730, kGliff },
  { 0x949F, CHIP_FAMILY_RV730, kGliff },

  // RV710
  { 0x9540, CHIP_FAMILY_RV710, kFlicker },
  { 0x9541, CHIP_FAMILY_RV710, kFlicker },
  { 0x9542, CHIP_FAMILY_RV710, kFlicker },
  { 0x954E, CHIP_FAMILY_RV710, kFlicker },
  { 0x954F, CHIP_FAMILY_RV710, kFlicker },
  { 0x9552, CHIP_FAMILY_RV710, kShrike },
  { 0x9553, CHIP_FAMILY_RV710, kShrike },
  { 0x9555, CHIP_FAMILY_RV710, kShrike },
  { 0x9557, CHIP_FAMILY_RV710, kFlicker },
  { 0x955F, CHIP_FAMILY_RV710, kFlicker },

  { 0x0000, CHIP_FAMILY_UNKNOW, kNull }

};

CONST CHAR8   *ATIFamilyName[] = {
  "UNKNOW",
  "R420",
  "R423",
  "RV410",
  "RV515",
  "R520",
  "RV530",
  "RV560",
  "RV570",
  "R580",
  /* IGP */
  "RS600",
  "RS690",
  "RS740",
  "RS780",
  "RS880",
  /* R600 */
  "R600",
  "RV610",
  "RV620",
  "RV630",
  "RV635",
  "RV670",
  /* R700 */
  "RV710",
  "RV730",
  "RV740",
  "RV770",
  "RV772",
  "RV790",
  /* Evergreen */
  "Cedar",
  "Cypress",
  "Hemlock",
  "Juniper",
  "Redwood",
  //   "Broadway",
  /* Northern Islands */
  "Barts",
  "Caicos",
  "Cayman",
  "Turks",
  /* Southern Islands */
  "Palm",
  "Sumo",
  "Sumo2",
  "Aruba",
  "Tahiti",
  "Pitcairn",
  "Verde",
  "Oland",
  "Hainan",
  "Bonaire",
  "Kaveri",
  "Kabini",
  "Hawaii",
  /* ... */
  "Mullins",
  "Topas",
  "Amethyst",
  "Tonga",
  ""
};

ATI_DEV_PROP  ATIDevPropList[] = {
  {FLAGTRUE,    FALSE,  "@0,AAPL,boot-display",             GetBootDisplayVal,    NULVAL },
  //{FLAGTRUE,  FALSE,  "@0,ATY,EFIDisplay",                NULL,                 STRVAL ("TMDSA") },

  //{FLAGTRUE,  TRUE,   "@0,AAPL,vram-memory",              GetVramVal,           NULVAL },
  {FLAGTRUE,    TRUE,   "AAPL00,override-no-connect",       GetEdidVal,           NULVAL },
  {FLAGNOTFAKE, TRUE,   "@0,compatible",                    GetNameVal,           NULVAL },
  {FLAGTRUE,    TRUE,   "@0,connector-type",                GetConnTypeVal,       NULVAL },
  {FLAGTRUE,    TRUE,   "@0,device_type",                   NULL,                 STRVAL ("display") },
  //{FLAGTRUE,  FALSE,  "@0,display-connect-flags",         NULL,                 DWRVAL (0) },

  //some set of properties for mobile radeons
  {FLAGMOBILE,  FALSE,  "@0,display-link-component-bits",   NULL,                 DWRVAL (6) },
  {FLAGMOBILE,  FALSE,  "@0,display-pixel-component-bits",  NULL,                 DWRVAL (6) },
  {FLAGMOBILE,  FALSE,  "@0,display-dither-support",        NULL,                 DWRVAL (0) },
  {FLAGMOBILE,  FALSE,  "@0,backlight-control",             NULL,                 DWRVAL (1) },
  {FLAGTRUE,    FALSE,  "AAPL00,Dither",                    NULL,                 DWRVAL (0) },

  //{FLAGTRUE,  TRUE,   "@0,display-type",                  NULL,                 STRVAL ("NONE") },
  {FLAGTRUE,    TRUE,   "@0,name",                          GetNameVal,           NULVAL },
  {FLAGTRUE,    TRUE,   "@0,VRAM,memsize",                  GetVramSizeVal,       NULVAL },
  //{FLAGTRUE,  TRUE,   "@0,ATY,memsize",                   GetVramSizeVal,       NULVAL },

  {FLAGTRUE,    FALSE,  "AAPL,aux-power-connected",         NULL,                 DWRVAL (1) },
  {FLAGTRUE,    FALSE,  "AAPL00,DualLink",                  GetDualLinkVal,       NULVAL  },
  {FLAGMOBILE,  FALSE,  "AAPL,HasPanel",                    NULL,                 DWRVAL (1) },
  {FLAGMOBILE,  FALSE,  "AAPL,HasLid",                      NULL,                 DWRVAL (1) },
  {FLAGMOBILE,  FALSE,  "AAPL,backlight-control",           NULL,                 DWRVAL (1) },
  {FLAGTRUE,    FALSE,  "AAPL,overwrite_binimage",          GetBinImageOwr,       NULVAL },
  {FLAGTRUE,    FALSE,  "ATY,bin_image",                    GetBinImageVal,       NULVAL },
  {FLAGTRUE,    FALSE,  "ATY,Copyright",                    NULL,                 STRVAL ("Copyright AMD Inc. All Rights Reserved. 2005-2011") },
  {FLAGTRUE,    FALSE,  "ATY,EFIVersion",                   NULL,                 STRVAL ("01.00.3180") },
  {FLAGTRUE,    FALSE,  "ATY,Card#",                        GetRomRevisionVal,    NULVAL },
  //{FLAGTRUE,  FALSE,  "ATY,Rom#",                         NULL,                 STRVAL ("www.amd.com") },
  {FLAGNOTFAKE, FALSE,  "ATY,VendorID",                     NULL,                 WRDVAL (0x1002) },
  {FLAGNOTFAKE, FALSE,  "ATY,DeviceID",                     GetDeviceIdVal,       NULVAL },

  //{FLAGTRUE,  FALSE,  "ATY,MCLK",                         GetMclkVal,           NULVAL },
  //{FLAGTRUE,  FALSE,  "ATY,SCLK",                         GetSclkVal,           NULVAL },
  {FLAGTRUE,    FALSE,  "ATY,RefCLK",                       GetRefclkVal,         DWRVAL (0x0a8c) },

  {FLAGTRUE,    FALSE,  "ATY,PlatformInfo",                 GetPlatformInfoVal,   NULVAL },
  {FLAGOLD,     FALSE,  "compatible",                       GetNamePciVal,        NULVAL },
  {FLAGTRUE,    FALSE,  "name",                             GetNameParentVal,     NULVAL },
  {FLAGTRUE,    FALSE,  "device_type",                      GetNameParentVal,     NULVAL },
  {FLAGTRUE,    FALSE,  "model",                            GetModelVal,          STRVAL ("ATI Radeon")},
  //{FLAGTRUE,  FALSE,  "VRAM,totalsize",                   GetVramTotalSizeVal,  NULVAL },

  {FLAGTRUE,    FALSE,  NULL,                               NULL,                 NULVAL }
};

VOID
GetAtiModel (
  OUT GFX_PROPERTIES  *Gfx,
  IN  UINT32          DevId
) {
  ATI_CARD_INFO   *Info = NULL;
  UINTN           i = 0;

  do {
    Info = &ATICards[i];
    if (Info->device_id == DevId) {
      break;
    }
  } while (ATICards[i++].device_id != 0);

  //AsciiSPrint (Gfx->Model,  64, "%a", Info->model_name);
  AsciiSPrint (Gfx->Model,  64, "%a", S_ATIMODEL);
  AsciiSPrint (Gfx->Config, 64, "%a", ATICardConfigs[Info->cfg_name].name);
  Gfx->Ports = ATICardConfigs[Info->cfg_name].ports;
}

BOOLEAN
GetBootDisplayVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT32   v = 0;

  if (v || !Card->posted) {
    return FALSE;
  }

  v = 1;
  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&v;

  return TRUE;
}

BOOLEAN
GetDualLinkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT32   v = 0;

  if (v) {
    return FALSE;
  }

  v = gSettings.DualLink;
  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&v;

  return TRUE;
}

BOOLEAN
GetVramVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  return FALSE;
}

BOOLEAN
GetEdidVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT32   v = 0;

  if (!gSettings.InjectEDID || !gSettings.CustomEDID) {
    return FALSE;
  }

  v = 1;
  Val->type = kPtr;
  Val->size = 128;
  Val->data = AllocateCopyPool (Val->size, gSettings.CustomEDID);

  return TRUE;
}

STATIC CONST CHAR8 *dtyp[] = {"LCD", "CRT", "DVI", "NONE"};
STATIC UINT32 dti = 0;

BOOLEAN
GetDisplayType (
  ATI_VAL   *Val,
  INTN      Index
) {
  dti++;

  if (dti > 3) {
    dti = 0;
  }

  Val->type = kStr;
  Val->size = 4;
  Val->data = (UINT8 *)dtyp[dti];

  return TRUE;
}

BOOLEAN
GetNameVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->type = ATYName.type;
  Val->size = ATYName.size;
  Val->data = ATYName.data;

  return TRUE;
}

BOOLEAN
GetNameParentVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->type = ATYNameParent.type;
  Val->size = ATYNameParent.size;
  Val->data = ATYNameParent.data;

  return TRUE;
}

STATIC CHAR8 pciName[15];

BOOLEAN
GetNamePciVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINTN   Len = ARRAY_SIZE (pciName);

  if (!gSettings.FakeATI) { //(!Card->info->model_name || !gSettings.FakeATI)
    return FALSE;
  }

  AsciiSPrint (pciName, Len, "pci1002,%x", gSettings.FakeATI >> 16);
  AsciiStrCpyS (pciName, Len, AsciiStrToLower (pciName));

  Val->type = kStr;
  Val->size = 13;
  Val->data = (UINT8 *)&pciName[0];

  return TRUE;
}

BOOLEAN
GetModelVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  //if (!Card->info->model_name) {
  //  return FALSE;
  //}

  Val->type = kStr;
  //Val->size = (UINT32)AsciiStrLen (Card->info->model_name);
  //Val->data = (UINT8 *)Card->info->model_name;
  Val->size = (UINT32)AsciiStrLen (S_ATIMODEL);
  Val->data = (UINT8 *)S_ATIMODEL;

  return TRUE;
}

//STATIC CONST UINT32 ctm[] = {0x02, 0x10, 0x800, 0x400}; //mobile
//STATIC CONST UINT32 ctd[] = {0x04, 0x10, 0x800, 0x400}; //desktop
//STATIC UINT32 cti = 0;

KERNEL_AND_KEXT_PATCHES *CurrentPatches;

//TODO - get connectors from ATIConnectorsPatch
BOOLEAN
GetConnTypeVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT8   *ct;
  //Connector types:
  //0x10:  VGA
  //0x04:  DL DVI-I
  //0x800: HDMI
  //0x400: DisplayPort
  //0x02:  LVDS

  if ((CurrentPatches == NULL) || (CurrentPatches->KPATIConnectorsDataLen == 0)) {
    return FALSE;
  }

  ct = CurrentPatches->KPATIConnectorsPatch;

  /*
  if (gMobile) {
   ct = (UINT32 *)&ctm[0];
   } else
   ct = (UINT32 *)&ctd[0];
  */

  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&ct[Index * 16];

  //  cti++;
  //  if (cti > 3) cti = 0;

  return TRUE;
}

BOOLEAN
GetVramSizeVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  INTN     i = -1;
  UINT64   MemSize;

  i++;
  MemSize = LShiftU64 ((UINT64)Card->vram_size, 32);

  if (i == 0) {
    MemSize = MemSize | (UINT64)Card->vram_size;
  }

  Val->type = kCst;
  Val->size = 8;
  Val->data = (UINT8 *)&MemSize;

  return TRUE;
}

BOOLEAN
GetBinImageVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  if (!Card->rom) {
    return FALSE;
  }

  Val->type = kPtr;
  Val->size = Card->rom_size;
  Val->data = Card->rom;

  return TRUE;
}

BOOLEAN
GetBinImageOwr (
  ATI_VAL   *Val,
  INTN      Index
) {
  UINT32   v = 0;

  if (!gSettings.LoadVBios) {
    return FALSE;
  }

  v = 1;
  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&v;

  return TRUE;
}

BOOLEAN
GetRomRevisionVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  CHAR8   *CRev = "109-B77101-00";
  UINT8   *Rev;

  if (!Card->rom){
    Val->type = kPtr;
    Val->size = 13;
    Val->data = AllocateZeroPool (Val->size);
    if (!Val->data)
      return FALSE;

    CopyMem (Val->data, CRev, Val->size);

    return TRUE;
  }

  Rev = Card->rom + *(UINT8 *)(Card->rom + OFFSET_TO_GET_ATOMBIOS_STRINGS_START);

  Val->type = kPtr;
  Val->size = (UINT32)AsciiStrLen ((CHAR8 *)Rev);

  if ((Val->size < 3) || (Val->size > 30)) { //fool proof. Real Value 13
    Rev = (UINT8 *)CRev;
    Val->size = 13;
  }

  Val->data = AllocateZeroPool (Val->size);

  if (!Val->data) {
    return FALSE;
  }

  CopyMem (Val->data, Rev, Val->size);

  return TRUE;
}

BOOLEAN
GetDeviceIdVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->type = kCst;
  Val->size = 2;
  Val->data = (UINT8 *)&Card->pci_dev->device_id;

  return TRUE;
}

BOOLEAN
GetMclkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  return FALSE;
}

BOOLEAN
GetSclkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  return FALSE;
}

BOOLEAN
GetRefclkVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  //if (!gSettings.RefCLK) {
    return FALSE;
  //}

  //Val->type = kCst;
  //Val->size = 4;
  //Val->data = (UINT8 *)&gSettings.RefCLK;

  //return TRUE;
}

BOOLEAN
GetPlatformInfoVal (
  ATI_VAL   *Val,
  INTN      Index
) {
  Val->data = AllocateZeroPool (0x80);

  if (!Val->data) {
    return FALSE;
  }

  //  bzero (Val->data, 0x80);

  Val->type   = kPtr;
  Val->size   = 0x80;
  Val->data[0]  = 1;

  return TRUE;
}

BOOLEAN
GetVramTotalSizeVal (
  ATI_VAL   *Val,
  INTN      index
) {
  Val->type = kCst;
  Val->size = 4;
  Val->data = (UINT8 *)&Card->vram_size;

  return TRUE;
}

VOID
FreeVal (
  ATI_VAL   *Val
) {
  if (Val->type == kPtr) {
    FreePool (Val->data);
  }

  ZeroMem (Val, sizeof (ATI_VAL));
}

VOID
DevpropAddList (
  ATI_DEV_PROP    DevPropList[]
) {
  INTN      i, PNum;
  ATI_VAL   *Val = AllocateZeroPool (sizeof (ATI_VAL));

  for (i = 0; DevPropList[i].name != NULL; i++) {
    if ((DevPropList[i].flags == FLAGTRUE) || (DevPropList[i].flags & Card->flags)) {
      if (DevPropList[i].get_value && DevPropList[i].get_value (Val, 0)) {
        DevpropAddValue (Card->device, DevPropList[i].name, Val->data, Val->size);
        FreeVal (Val);

        if (DevPropList[i].all_ports) {
          for (PNum = 1; PNum < Card->ports; PNum++) {
            if (DevPropList[i].get_value (Val, PNum)) {
              DevPropList[i].name[1] = (CHAR8)(0x30 + PNum); // convert to ascii
              DevpropAddValue (Card->device, DevPropList[i].name, Val->data, Val->size);
              FreeVal (Val);
            }
          }

          DevPropList[i].name[1] = 0x30; // write back our "@0," for a next possible card
        }
      } else {
        if (DevPropList[i].default_val.type != kNul) {
          DevpropAddValue (
            Card->device, DevPropList[i].name,
            DevPropList[i].default_val.type == kCst
              ? (UINT8 *)&(DevPropList[i].default_val.data)
              : DevPropList[i].default_val.data,
            DevPropList[i].default_val.size
          );
        }

        if (DevPropList[i].all_ports) {
          for (PNum = 1; PNum < Card->ports; PNum++) {
            if (DevPropList[i].default_val.type != kNul) {
              DevPropList[i].name[1] = (CHAR8)(0x30 + PNum); // convert to ascii
              DevpropAddValue (
                Card->device, DevPropList[i].name,
                DevPropList[i].default_val.type == kCst
                  ? (UINT8 *)&(DevPropList[i].default_val.data)
                  : DevPropList[i].default_val.data,
                DevPropList[i].default_val.size
              );
            }
          }

          DevPropList[i].name[1] = 0x30; // write back our "@0," for a next possible card
        }
      }
    }
  }

  FreePool (Val);
}

BOOLEAN
ValidateROM (
  OPTION_ROM_HEADER   *RomHeader,
  PCI_DT              *Dev
) {
  OPTION_ROM_PCI_HEADER   *RomPciHeader;

  if (RomHeader->signature != 0xaa55){
    DBG ("invalid ROM signature %x\n", RomHeader->signature);
    return FALSE;
  }

  RomPciHeader = (OPTION_ROM_PCI_HEADER *)((UINT8 *)RomHeader + RomHeader->pci_header_offset);

  if (RomPciHeader->signature != 0x52494350) {
    DBG ("invalid ROM header %x\n", RomPciHeader->signature);
    return FALSE;
  }

  if (
    (RomPciHeader->vendor_id != Dev->vendor_id) ||
    (RomPciHeader->device_id != Dev->device_id)
  ){
    DBG ("invalid ROM vendor=%x deviceID=%d\n", RomPciHeader->vendor_id, RomPciHeader->device_id);
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
LoadVbiosFile (
  UINT16  VendorId,
  UINT16  DeviceId
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  UINTN         BufferLen = 0;
  CHAR16        FileName[64];
  UINT8         *Buffer = 0;

  //if we are here then TRUE
  //  if (!gSettings.LoadVBios)
  //    return FALSE;

  UnicodeSPrint (FileName, ARRAY_SIZE (FileName), L"%s\\%04x_%04x.rom", DIR_ROM, VendorId, DeviceId);
  if (FileExists (SelfRootDir, FileName)){
    DBG ("Found generic VBIOS ROM file (%04x_%04x.rom)\n", VendorId, DeviceId);
    Status = LoadFile (SelfRootDir, FileName, &Buffer, &BufferLen);
  }

  if (EFI_ERROR (Status) || (BufferLen == 0)){
    DBG ("ATI ROM not found \n");
    Card->rom_size = 0;
    Card->rom = 0;
    return FALSE;
  }

  DBG ("Loaded ROM len=%d\n", BufferLen);

  Card->rom_size = (UINT32)BufferLen;
  Card->rom = AllocateZeroPool (BufferLen);

  if (!Card->rom) {
    return FALSE;
  }

  CopyMem (Card->rom, Buffer, BufferLen);
  //  read (fd, (CHAR8 *)Card->rom, Card->rom_size);

  if (!ValidateROM ((OPTION_ROM_HEADER *)Card->rom, Card->pci_dev)) {
    DBG ("ValidateROM fails\n");
    Card->rom_size = 0;
    Card->rom = 0;
    return FALSE;
  }

  BufferLen = ((OPTION_ROM_HEADER *)Card->rom)->rom_size;
  Card->rom_size = (UINT32)(BufferLen << 9);
  DBG ("Calculated ROM len=%d\n", Card->rom_size);
  //  close (fd);
  FreePool (Buffer);

  return TRUE;
}

VOID
GetVramSize () {
  //check Card->vram_size in bytes!
  ATI_CHIP_FAMILY   ChipFamily = Card->info->chip_family;

  Card->vram_size = 128 << 20; //default 128Mb, this is minimum for OS
  if (gSettings.VRAM != 0) {
    Card->vram_size = gSettings.VRAM;
    DBG ("Set VRAM from config=%dMb\n", (INTN)RShiftU64 (Card->vram_size, 20));
    //    WRITEREG32 (Card->mmio, RADEON_CONFIG_MEMSIZE, Card->vram_size);
  } else {
    if (ChipFamily >= CHIP_FAMILY_CEDAR) {
      // size in MB on evergreen
      // XXX watch for overflow!!!
      Card->vram_size = ((UINT64)REG32 (Card->mmio, R600_CONFIG_MEMSIZE)) << 20;
      DBG ("Set VRAM for Cedar+ =%dMb\n", (INTN)RShiftU64 (Card->vram_size, 20));
    } else if (ChipFamily >= CHIP_FAMILY_R600) {
      Card->vram_size = REG32 (Card->mmio, R600_CONFIG_MEMSIZE);
    } else {
      Card->vram_size = REG32 (Card->mmio, RADEON_CONFIG_MEMSIZE);
      if (Card->vram_size == 0) {
        Card->vram_size = REG32 (Card->mmio, RADEON_CONFIG_APER_SIZE);
        //Slice - previously I successfully made Radeon9000 working
        //by writing this register
        WRITEREG32 (Card->mmio, RADEON_CONFIG_MEMSIZE, (UINT32)Card->vram_size);
      }
    }
  }

  gSettings.VRAM = Card->vram_size;
  DBG ("ATI: GetVramSize returned 0x%x\n", Card->vram_size);
}

BOOLEAN
ReadVBios (
  BOOLEAN   FromPci
) {
  OPTION_ROM_HEADER   *RomAddr;

  if (FromPci) {
    RomAddr = (OPTION_ROM_HEADER *)(UINTN)(PciConfigRead32 (Card->pci_dev, PCI_EXPANSION_ROM_BASE) & ~0x7ff);
    DBG (" @0x%x\n", RomAddr);
  } else {
    RomAddr = (OPTION_ROM_HEADER *)(UINTN)0xc0000;
  }

  if (!ValidateROM (RomAddr, Card->pci_dev)){
    DBG ("There is no ROM @0x%x\n", RomAddr);
    //gBS->Stall (3000000);
    return FALSE;
  }

  Card->rom_size = (UINT32)(RomAddr->rom_size) << 9;

  if (!Card->rom_size){
    DBG ("invalid ROM size =0\n");
    return FALSE;
  }

  Card->rom = AllocateZeroPool (Card->rom_size);

  if (!Card->rom) {
    return FALSE;
  }

  CopyMem (Card->rom, (VOID *)RomAddr, Card->rom_size);

  return TRUE;
}

BOOLEAN
ReadDisabledVbios () {
  BOOLEAN             Ret = FALSE;
  ATI_CHIP_FAMILY     ChipFamily = Card->info->chip_family;

  if (ChipFamily >= CHIP_FAMILY_RV770) {
    UINT32    ViphControl       = REG32 (Card->mmio, RADEON_VIPH_CONTROL),
              BusCntl           = REG32 (Card->mmio, RADEON_BUS_CNTL),
              D1VgaControl      = REG32 (Card->mmio, AVIVO_D1VGA_CONTROL),
              D2VgaControl      = REG32 (Card->mmio, AVIVO_D2VGA_CONTROL),
              VgaRenderControl  = REG32 (Card->mmio, AVIVO_VGA_RENDER_CONTROL),
              RomCntl           = REG32 (Card->mmio, R600_ROM_CNTL),
              CgSpllFuncCntl    = 0,
              CgSpllStatus;

    // disable VIP
    WRITEREG32 (Card->mmio, RADEON_VIPH_CONTROL, (ViphControl & ~RADEON_VIPH_EN));

    // enable the rom
    WRITEREG32 (Card->mmio, RADEON_BUS_CNTL, (BusCntl & ~RADEON_BUS_BIOS_DIS_ROM));

    // Disable VGA mode
    WRITEREG32 (Card->mmio, AVIVO_D1VGA_CONTROL, (D1VgaControl & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
    WRITEREG32 (Card->mmio, AVIVO_D2VGA_CONTROL, (D2VgaControl & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
    WRITEREG32 (Card->mmio, AVIVO_VGA_RENDER_CONTROL, (VgaRenderControl & ~AVIVO_VGA_VSTATUS_CNTL_MASK));

    if (ChipFamily == CHIP_FAMILY_RV730) {
      CgSpllFuncCntl = REG32 (Card->mmio, R600_CG_SPLL_FUNC_CNTL);

      // enable bypass mode
      WRITEREG32 (Card->mmio, R600_CG_SPLL_FUNC_CNTL, (CgSpllFuncCntl | R600_SPLL_BYPASS_EN));

      // wait for SPLL_CHG_STATUS to change to 1
      CgSpllStatus = 0;
      while (!(CgSpllStatus & R600_SPLL_CHG_STATUS))
        CgSpllStatus = REG32 (Card->mmio, R600_CG_SPLL_STATUS);

      WRITEREG32 (Card->mmio, R600_ROM_CNTL, (RomCntl & ~R600_SCK_OVERWRITE));
    } else {
      WRITEREG32 (Card->mmio, R600_ROM_CNTL, (RomCntl | R600_SCK_OVERWRITE));
    }

    Ret = ReadVBios (TRUE);

    // restore regs
    if (ChipFamily == CHIP_FAMILY_RV730) {
      WRITEREG32 (Card->mmio, R600_CG_SPLL_FUNC_CNTL, CgSpllFuncCntl);

      // wait for SPLL_CHG_STATUS to change to 1
      CgSpllStatus = 0;
      while (!(CgSpllStatus & R600_SPLL_CHG_STATUS)) {
        CgSpllStatus = REG32 (Card->mmio, R600_CG_SPLL_STATUS);
      }
    }

    WRITEREG32 (Card->mmio, RADEON_VIPH_CONTROL, ViphControl);
    WRITEREG32 (Card->mmio, RADEON_BUS_CNTL, BusCntl);
    WRITEREG32 (Card->mmio, AVIVO_D1VGA_CONTROL, D1VgaControl);
    WRITEREG32 (Card->mmio, AVIVO_D2VGA_CONTROL, D2VgaControl);
    WRITEREG32 (Card->mmio, AVIVO_VGA_RENDER_CONTROL, VgaRenderControl);
    WRITEREG32 (Card->mmio, R600_ROM_CNTL, RomCntl);
  } else if (ChipFamily >= CHIP_FAMILY_R600) {
    UINT32    ViphControl                  = REG32 (Card->mmio, RADEON_VIPH_CONTROL),
              BusCntl                      = REG32 (Card->mmio, RADEON_BUS_CNTL),
              D1VgaControl                 = REG32 (Card->mmio, AVIVO_D1VGA_CONTROL),
              D2VgaControl                 = REG32 (Card->mmio, AVIVO_D2VGA_CONTROL),
              VgaRenderControl             = REG32 (Card->mmio, AVIVO_VGA_RENDER_CONTROL),
              RomCntl                      = REG32 (Card->mmio, R600_ROM_CNTL),
              general_pwrmgt                = REG32 (Card->mmio, R600_GENERAL_PWRMGT),
              low_vid_lower_gpio_cntl       = REG32 (Card->mmio, R600_LOW_VID_LOWER_GPIO_CNTL),
              medium_vid_lower_gpio_cntl    = REG32 (Card->mmio, R600_MEDIUM_VID_LOWER_GPIO_CNTL),
              high_vid_lower_gpio_cntl      = REG32 (Card->mmio, R600_HIGH_VID_LOWER_GPIO_CNTL),
              ctxsw_vid_lower_gpio_cntl     = REG32 (Card->mmio, R600_CTXSW_VID_LOWER_GPIO_CNTL),
              lower_gpio_enable             = REG32 (Card->mmio, R600_LOWER_GPIO_ENABLE);

    // disable VIP
    WRITEREG32 (Card->mmio, RADEON_VIPH_CONTROL, (ViphControl & ~RADEON_VIPH_EN));

    // enable the rom
    WRITEREG32 (Card->mmio, RADEON_BUS_CNTL, (BusCntl & ~RADEON_BUS_BIOS_DIS_ROM));

    // Disable VGA mode
    WRITEREG32 (Card->mmio, AVIVO_D1VGA_CONTROL, (D1VgaControl & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
    WRITEREG32 (Card->mmio, AVIVO_D2VGA_CONTROL, (D2VgaControl & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
    WRITEREG32 (Card->mmio, AVIVO_VGA_RENDER_CONTROL, (VgaRenderControl & ~AVIVO_VGA_VSTATUS_CNTL_MASK));
    WRITEREG32 (Card->mmio, R600_ROM_CNTL, ((RomCntl & ~R600_SCK_PRESCALE_CRYSTAL_CLK_MASK) | (1 << R600_SCK_PRESCALE_CRYSTAL_CLK_SHIFT) | R600_SCK_OVERWRITE));
    WRITEREG32 (Card->mmio, R600_GENERAL_PWRMGT, (general_pwrmgt & ~R600_OPEN_DRAIN_PADS));
    WRITEREG32 (Card->mmio, R600_LOW_VID_LOWER_GPIO_CNTL, (low_vid_lower_gpio_cntl & ~0x400));
    WRITEREG32 (Card->mmio, R600_MEDIUM_VID_LOWER_GPIO_CNTL, (medium_vid_lower_gpio_cntl & ~0x400));
    WRITEREG32 (Card->mmio, R600_HIGH_VID_LOWER_GPIO_CNTL, (high_vid_lower_gpio_cntl & ~0x400));
    WRITEREG32 (Card->mmio, R600_CTXSW_VID_LOWER_GPIO_CNTL, (ctxsw_vid_lower_gpio_cntl & ~0x400));
    WRITEREG32 (Card->mmio, R600_LOWER_GPIO_ENABLE, (lower_gpio_enable | 0x400));

    Ret = ReadVBios (TRUE);

    // restore regs
    WRITEREG32 (Card->mmio, RADEON_VIPH_CONTROL, ViphControl);
    WRITEREG32 (Card->mmio, RADEON_BUS_CNTL, BusCntl);
    WRITEREG32 (Card->mmio, AVIVO_D1VGA_CONTROL, D1VgaControl);
    WRITEREG32 (Card->mmio, AVIVO_D2VGA_CONTROL, D2VgaControl);
    WRITEREG32 (Card->mmio, AVIVO_VGA_RENDER_CONTROL, VgaRenderControl);
    WRITEREG32 (Card->mmio, R600_ROM_CNTL, RomCntl);
    WRITEREG32 (Card->mmio, R600_GENERAL_PWRMGT, general_pwrmgt);
    WRITEREG32 (Card->mmio, R600_LOW_VID_LOWER_GPIO_CNTL, low_vid_lower_gpio_cntl);
    WRITEREG32 (Card->mmio, R600_MEDIUM_VID_LOWER_GPIO_CNTL, medium_vid_lower_gpio_cntl);
    WRITEREG32 (Card->mmio, R600_HIGH_VID_LOWER_GPIO_CNTL, high_vid_lower_gpio_cntl);
    WRITEREG32 (Card->mmio, R600_CTXSW_VID_LOWER_GPIO_CNTL, ctxsw_vid_lower_gpio_cntl);
    WRITEREG32 (Card->mmio, R600_LOWER_GPIO_ENABLE, lower_gpio_enable);
  }

  return Ret;
}

BOOLEAN
RadeonCardPosted () {
  UINT32  Reg = REG32 (Card->mmio, RADEON_CRTC_GEN_CNTL) | REG32 (Card->mmio, RADEON_CRTC2_GEN_CNTL);

  if (Reg & RADEON_CRTC_EN) {
    return TRUE;
  }

  // then check MEM_SIZE, in case something turned the crtcs off
  Reg = REG32 (Card->mmio, R600_CONFIG_MEMSIZE);

  if (Reg) {
    return TRUE;
  }

  return FALSE;
}

STATIC
BOOLEAN
InitCard (
  PCI_DT    *Dev
) {
  BOOLEAN     LoadVBios = gSettings.LoadVBios;
  CHAR8       *Name, *NameParent, *CfgName;
  UINTN       i, j, ExpansionRom = 0;
  INTN        NameLen = 0, PortsNum = 0;

  Card = AllocateZeroPool (sizeof (ATI_CARD));

  if (!Card) {
    return FALSE;
  }

  Card->pci_dev = Dev;

  for (i = 0; ATICards[i].device_id ; i++) {
    Card->info = &ATICards[i];
    if (ATICards[i].device_id == Dev->device_id) break;
  }

  for (j = 0; j < NGFX; j++) {
    if (
      (gGraphics[j].Vendor == Ati) &&
      (gGraphics[j].DeviceID == Dev->device_id)
    ) {
      //      model = gGraphics[j].Model;
      PortsNum = gGraphics[j].Ports;
      LoadVBios = gGraphics[j].LoadVBios;
      break;
    }
  }

  if (!Card->info || !Card->info->device_id || !Card->info->cfg_name) {
    DBG ("Unsupported ATI card! Device ID: [%04x:%04x] Subsystem ID: [%08x] \n",
        Dev->vendor_id, Dev->device_id, Dev->subsys_id);
    DBG ("search for brothers family\n");
    for (i = 0; ATICards[i].device_id ; i++) {
      if ((ATICards[i].device_id & ~0xf) == (Dev->device_id & ~0xf)) {
        Card->info = &ATICards[i];
        break;
      }
    }

    if (!Card->info->cfg_name) {
      DBG ("...compatible config is not found\n");
      return FALSE;
    }
  }

  Card->fb    = (UINT8 *)(UINTN)(PciConfigRead32 (Dev, PCI_BASE_ADDRESS_0) & ~0x0f);
  Card->mmio  = (UINT8 *)(UINTN)(PciConfigRead32 (Dev, PCI_BASE_ADDRESS_2) & ~0x0f);
  Card->io    = (UINT8 *)(UINTN)(PciConfigRead32 (Dev, PCI_BASE_ADDRESS_4) & ~0x03);
  Dev->regs = Card->mmio;
  ExpansionRom = PciConfigRead32 (Dev, PCI_EXPANSION_ROM_BASE); //0x30 as Chimera

  DBG ("Framebuffer @0x%08X  MMIO @0x%08X  I/O Port @0x%08X ROM Addr @0x%08X\n",
      Card->fb, Card->mmio, Card->io, ExpansionRom);

  Card->posted = RadeonCardPosted ();

  DBG ("ATI card %a\n", Card->posted ? "POSTed" : "non-POSTed");

  GetVramSize ();

  if (LoadVBios) {
    LoadVbiosFile (Dev->vendor_id, Dev->device_id);
    if (!Card->rom) {
      DBG ("reading VBIOS from %a", Card->posted ? "legacy space" : "PCI ROM");
      if (Card->posted) { // && ExpansionRom != 0)
        ReadVBios (FALSE);
      } else {
        ReadDisabledVbios ();
      }
      DBG ("\n");
    } else {
      DBG ("VideoBIOS read from file\n");
    }

  }

  if (Card->info->chip_family >= CHIP_FAMILY_CEDAR) {
    DBG ("ATI Radeon EVERGREEN family\n");
    Card->flags |= EVERGREEN;
  }

  if (Card->info->chip_family <= CHIP_FAMILY_RV670) {
    DBG ("ATI Radeon OLD family\n");
    Card->flags |= FLAGOLD;
  }

  if (gMobile) {
    DBG ("ATI Mobile Radeon\n");
    Card->flags |= FLAGMOBILE;
  }

  Card->flags |= FLAGNOTFAKE;

  NameLen = StrLen (gSettings.FBName);
  if (NameLen > 2) {  //fool proof: cfg_name is 3 character or more.
    CfgName = AllocateZeroPool (NameLen);
    UnicodeStrToAsciiStrS ((CHAR16 *)&gSettings.FBName[0], CfgName, NameLen);
    DBG ("Users config name %a\n", CfgName);
    Card->cfg_name = CfgName;
  } else {
    // use cfg_name on ATICards, to retrive the default name from ATICardConfigs,
    Card->cfg_name = ATICardConfigs[Card->info->cfg_name].name;
    PortsNum = ATICardConfigs[Card->info->cfg_name].ports;
    // which means one of the fb's or kNull
    DBG ("Framebuffer set to device's default: %a\n", Card->cfg_name);
    DBG (" N ports defaults to %d\n", PortsNum);
  }

  if (gSettings.VideoPorts != 0) {
    PortsNum = gSettings.VideoPorts;
    DBG (" use N ports setting from config.plist: %d\n", PortsNum);
  }

  if (PortsNum > 0) {
    Card->ports = (UINT8)PortsNum; // use it.
    DBG ("(AtiPorts) Nr of ports set to: %d\n", Card->ports);
  } else {
    // if (Card->cfg_name > 0) // do we want 0 ports if fb is kNull or mistyped ?

    // else, match cfg_name with ATICardConfigs list and retrive default nr of ports.
    for (i = 0; i < kCfgEnd; i++) {
      if (AsciiStrCmp (Card->cfg_name, ATICardConfigs[i].name) == 0) {
        Card->ports = ATICardConfigs[i].ports; // default
      }
    }

    DBG ("Nr of ports set to framebuffer's default: %d\n", Card->ports);
  }

  if (Card->ports == 0) {
    Card->ports = 2; //real minimum
    DBG ("Nr of ports set to min: %d\n", Card->ports);
  }

  //
  Name = AllocateZeroPool (24);
  AsciiSPrint (Name, 24, "ATY,%a", Card->cfg_name);
  ATYName.type = kStr;
  ATYName.size = (UINT32)AsciiStrLen (Name);
  ATYName.data = (UINT8 *)Name;

  NameParent = AllocateZeroPool (24);
  AsciiSPrint (NameParent, 24, "ATY,%aParent", Card->cfg_name);
  ATYNameParent.type = kStr;
  ATYNameParent.size = (UINT32)AsciiStrLen (NameParent);
  ATYNameParent.data = (UINT8 *)NameParent;

  //how can we free pool when we leave the procedure? Make all pointers global?
  return TRUE;
}

BOOLEAN
SetupAtiDevprop (
  LOADER_ENTRY  *Entry,
  PCI_DT        *Dev
) {
  CHAR8     Compatible[64], *DevPath;
  UINT32    FakeID = 0;
  INT32     i;

  if (!InitCard (Dev)) {
    return FALSE;
  }

  CurrentPatches = Entry->KernelAndKextPatches;

  // -------------------------------------------------
  // Find a better way to do this (in device_inject.c)
  if (!gDevPropString) {
    gDevPropString = DevpropCreateString ();
  }

  DevPath = GetPciDevPath (Dev);
  //Card->device = devprop_add_device (gDevPropString, DevPath);
  Card->device = DevpropAddDevicePci (gDevPropString, Dev);

  if (!Card->device) {
    return FALSE;
  }

  // -------------------------------------------------

  if (gSettings.FakeATI) {
    UINTN   Len = ARRAY_SIZE (Compatible);

    Card->flags &= ~FLAGNOTFAKE;
    Card->flags |= FLAGOLD;

    FakeID = gSettings.FakeATI >> 16;
    DevpropAddValue (Card->device, "device-id", (UINT8 *)&FakeID, 4);
    DevpropAddValue (Card->device, "ATY,DeviceID", (UINT8 *)&FakeID, 2);
    AsciiSPrint (Compatible, Len, "pci1002,%04x", FakeID);
    AsciiStrCpyS (Compatible, Len, AsciiStrToLower (Compatible));
    DevpropAddValue (Card->device, "@0,compatible", (UINT8 *)&Compatible[0], 12);
    FakeID = gSettings.FakeATI & 0xFFFF;
    DevpropAddValue (Card->device, "vendor-id", (UINT8 *)&FakeID, 4);
    DevpropAddValue (Card->device, "ATY,VendorID", (UINT8 *)&FakeID, 2);
    MsgLog (" - With FakeID: %a\n", Compatible);
  }

  if (!gSettings.NoDefaultProperties) {
    DevpropAddList (ATIDevPropList);
    if (gSettings.UseIntelHDMI) {
      DevpropAddValue (Card->device, "hda-gfx", (UINT8 *)"onboard-2", 10);
    } else {
      DevpropAddValue (Card->device, "hda-gfx", (UINT8 *)"onboard-1", 10);
    }
  } /*else {
    MsgLog (" - no default properties\n");
  }*/

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_ATI) {
        continue;
      }

      DevpropAddValue (
        Card->device,
        gSettings.AddProperties[i].Key,
        (UINT8 *)gSettings.AddProperties[i].Value,
        gSettings.AddProperties[i].ValueLen
      );
    }
  }

  DBG (
    "ATI %a %a %dMB (%a) [%04x:%04x] (subsys [%04x:%04x]) | %a\n",
    //ATIFamilyName[Card->info->chip_family], Card->info->model_name,
    ATIFamilyName[Card->info->chip_family], S_ATIMODEL,
    (UINT32)RShiftU64 (Card->vram_size, 20), Card->cfg_name,
    Dev->vendor_id, Dev->device_id,
    Dev->subsys_id.subsys.vendor_id, Dev->subsys_id.subsys.device_id,
    DevPath
  );

  //FreePool (Card->info); //TODO we can't free constant so this info should be copy of constants
  FreePool (Card);

  return TRUE;
}
