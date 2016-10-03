/*
 * ATI Graphics Card Enabler, part of the Chameleon Boot Loader Project
 *
 * Copyright 2010 by Islam M. Ahmed Zaid. All rights reserved.
 *
 */

#include "Platform.h"
#include "ati.h"

#ifndef DEBUG_ALL
#define DEBUG_ATI 1
#else
#define DEBUG_ATI DEBUG_ALL
#endif

#if DEBUG_ATI == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_ATI, __VA_ARGS__)
#endif

#define S_ATIMODEL "ATI/AMD Graphics"

static value_t aty_name;
static value_t aty_nameparent;
card_t *card;
//static value_t aty_model;

card_config_t card_configs[] = {
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

radeon_card_info_t radeon_cards[] = {

  // Earlier cards are not supported
  //
  // Layout is device_id, fake_id, chip_family_name, display name, frame buffer
  // Cards are grouped by device id  to make it easier to add new cards
  //

  // OLAND

  // Oland: R7-240, 250  - Southand Island
  // Earlier cards are not supported
  //
  // Layout is device_id, fake_id, chip_family_name, display name, frame buffer
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

const CHAR8 *chip_family_name[] = {
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

AtiDevProp ati_devprop_list[] = {
  {FLAGTRUE,    FALSE,  "@0,AAPL,boot-display",             get_bootdisplay_val,    NULVAL },
  //{FLAGTRUE,  FALSE,  "@0,ATY,EFIDisplay",                NULL,                   STRVAL("TMDSA") },

  //{FLAGTRUE,  TRUE,   "@0,AAPL,vram-memory",              get_vrammemory_val,     NULVAL },
  {FLAGTRUE,    TRUE,   "AAPL00,override-no-connect",       get_edid_val,           NULVAL },
  {FLAGNOTFAKE, TRUE,   "@0,compatible",                    get_name_val,           NULVAL },
  {FLAGTRUE,    TRUE,   "@0,connector-type",                get_conntype_val,       NULVAL },
  {FLAGTRUE,    TRUE,   "@0,device_type",                   NULL,                   STRVAL("display") },
  //{FLAGTRUE,  FALSE,  "@0,display-connect-flags",         NULL,                   DWRVAL(0) },

  //some set of properties for mobile radeons
  {FLAGMOBILE,  FALSE,  "@0,display-link-component-bits",   NULL,                   DWRVAL(6) },
  {FLAGMOBILE,  FALSE,  "@0,display-pixel-component-bits",  NULL,                   DWRVAL(6) },
  {FLAGMOBILE,  FALSE,  "@0,display-dither-support",        NULL,                   DWRVAL(0) },
  {FLAGMOBILE,  FALSE,  "@0,backlight-control",             NULL,                   DWRVAL(1) },
  {FLAGTRUE,    FALSE,  "AAPL00,Dither",                    NULL,                   DWRVAL(0) },

  //{FLAGTRUE,  TRUE,   "@0,display-type",                  NULL,                   STRVAL("NONE") },
  {FLAGTRUE,    TRUE,   "@0,name",                          get_name_val,           NULVAL },
  {FLAGTRUE,    TRUE,   "@0,VRAM,memsize",                  get_vrammemsize_val,    NULVAL },
  //{FLAGTRUE,  TRUE,   "@0,ATY,memsize",                   get_vrammemsize_val,    NULVAL },

  {FLAGTRUE,    FALSE,  "AAPL,aux-power-connected",         NULL,                   DWRVAL(1) },
  {FLAGTRUE,    FALSE,  "AAPL00,DualLink",                  get_dual_link_val,      NULVAL  },
  {FLAGMOBILE,  FALSE,  "AAPL,HasPanel",                    NULL,                   DWRVAL(1) },
  {FLAGMOBILE,  FALSE,  "AAPL,HasLid",                      NULL,                   DWRVAL(1) },
  {FLAGMOBILE,  FALSE,  "AAPL,backlight-control",           NULL,                   DWRVAL(1) },
  {FLAGTRUE,    FALSE,  "AAPL,overwrite_binimage",          get_binimage_owr,       NULVAL },
  {FLAGTRUE,    FALSE,  "ATY,bin_image",                    get_binimage_val,       NULVAL },
  {FLAGTRUE,    FALSE,  "ATY,Copyright",                    NULL,                   STRVAL("Copyright AMD Inc. All Rights Reserved. 2005-2011") },
  {FLAGTRUE,    FALSE,  "ATY,EFIVersion",                   NULL,                   STRVAL("01.00.3180") },
  {FLAGTRUE,    FALSE,  "ATY,Card#",                        get_romrevision_val,    NULVAL },
  //{FLAGTRUE,  FALSE,  "ATY,Rom#",                         NULL,                   STRVAL("www.amd.com") },
  {FLAGNOTFAKE, FALSE,  "ATY,VendorID",                     NULL,                   WRDVAL(0x1002) },
  {FLAGNOTFAKE, FALSE,  "ATY,DeviceID",                     get_deviceid_val,       NULVAL },

  //{FLAGTRUE,  FALSE,  "ATY,MCLK",                         get_mclk_val,           NULVAL },
  //{FLAGTRUE,  FALSE,  "ATY,SCLK",                         get_sclk_val,           NULVAL },
  {FLAGTRUE,    FALSE,  "ATY,RefCLK",                       get_refclk_val,         DWRVAL(0x0a8c) },

  {FLAGTRUE,    FALSE,  "ATY,PlatformInfo",                 get_platforminfo_val,   NULVAL },
  {FLAGOLD,     FALSE,  "compatible",                       get_name_pci_val,       NULVAL },
  {FLAGTRUE,    FALSE,  "name",                             get_nameparent_val,     NULVAL },
  {FLAGTRUE,    FALSE,  "device_type",                      get_nameparent_val,     NULVAL },
  {FLAGTRUE,    FALSE,  "model",                            get_model_val,          STRVAL("ATI Radeon")},
  //{FLAGTRUE,  FALSE,  "VRAM,totalsize",                   get_vramtotalsize_val,  NULVAL },

  {FLAGTRUE,    FALSE,  NULL,                               NULL,                   NULVAL }
};

VOID
get_ati_model (
  OUT GFX_PROPERTIES  *gfx,
  IN  UINT32          device_id
) {
  radeon_card_info_t    *info = NULL;
  UINTN                 i = 0;

  do {
    info = &radeon_cards[i];
    if (info->device_id == device_id) {
      break;
    }
  } while (radeon_cards[i++].device_id != 0);

  //AsciiSPrint (gfx->Model,  64, "%a", info->model_name);
  AsciiSPrint (gfx->Model,  64, "%a", S_ATIMODEL);
  AsciiSPrint (gfx->Config, 64, "%a", card_configs[info->cfg_name].name);
  gfx->Ports = card_configs[info->cfg_name].ports;
}

BOOLEAN
get_bootdisplay_val (
  value_t *val,
  INTN index
) {
  static UINT32   v = 0;

  if (v)
    return FALSE;

  if (!card->posted) {
    return FALSE;
  }

  v = 1;
  val->type = kCst;
  val->size = 4;
  val->data = (UINT8 *)&v;

  return TRUE;
}

BOOLEAN
get_dual_link_val (
  value_t   *val,
  INTN      index
) {
  static UINT32   v = 0;

  if (v) {
    return FALSE;
  }

  v = gSettings.DualLink;
  val->type = kCst;
  val->size = 4;
  val->data = (UINT8 *)&v;

  return TRUE;
}


BOOLEAN
get_vrammemory_val (
  value_t   *val,
  INTN      index
) {
  return FALSE;
}

BOOLEAN
get_edid_val (
  value_t   *val,
  INTN      index
) {
  static UINT32   v = 0;

  if (!gSettings.InjectEDID || !gSettings.CustomEDID) {
    return FALSE;
  }

  v = 1;
  val->type = kPtr;
  val->size = 128;
  val->data = AllocateCopyPool(val->size, gSettings.CustomEDID);

  return TRUE;
}

static CONST CHAR8* dtyp[] = {"LCD", "CRT", "DVI", "NONE"};
static UINT32 dti = 0;

BOOLEAN
get_display_type (
  value_t   *val,
  INTN      index
) {
  dti++;

  if (dti > 3) {
    dti = 0;
  }

  val->type = kStr;
  val->size = 4;
  val->data = (UINT8 *)dtyp[dti];

  return TRUE;
}


BOOLEAN
get_name_val (
  value_t   *val,
  INTN      index
) {
  val->type = aty_name.type;
  val->size = aty_name.size;
  val->data = aty_name.data;

  return TRUE;
}

BOOLEAN
get_nameparent_val (
  value_t   *val,
  INTN      index
) {
  val->type = aty_nameparent.type;
  val->size = aty_nameparent.size;
  val->data = aty_nameparent.data;

  return TRUE;
}

static CHAR8 pciName[15];

BOOLEAN
get_name_pci_val (
  value_t   *val,
  INTN      index
) {
  //if (!card->info->model_name || !gSettings.FakeATI) {
  if (!gSettings.FakeATI) {
    return FALSE;
  }

  AsciiSPrint(pciName, 15, "pci1002,%x", gSettings.FakeATI >> 16);
  AsciiStrCpy(pciName, AsciiStrToLower(pciName));
  val->type = kStr;
  val->size = 13;
  val->data = (UINT8 *)&pciName[0];

  return TRUE;
}


BOOLEAN
get_model_val (
  value_t   *val,
  INTN      index
) {
  //if (!card->info->model_name) {
  //  return FALSE;
  //}

  val->type = kStr;
  //val->size = (UINT32)AsciiStrLen(card->info->model_name);
  //val->data = (UINT8 *)card->info->model_name;
  val->size = (UINT32)AsciiStrLen(S_ATIMODEL);
  val->data = (UINT8 *)S_ATIMODEL;

  return TRUE;
}

//static CONST UINT32 ctm[] = {0x02, 0x10, 0x800, 0x400}; //mobile
//static CONST UINT32 ctd[] = {0x04, 0x10, 0x800, 0x400}; //desktop
//static UINT32 cti = 0;

KERNEL_AND_KEXT_PATCHES *CurrentPatches;

//TODO - get connectors from ATIConnectorsPatch
BOOLEAN
get_conntype_val (
  value_t   *val,
  INTN      index
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
   ct = (UINT32*)&ctm[0];
   } else
   ct = (UINT32*)&ctd[0];
  */

  val->type = kCst;
  val->size = 4;
  val->data = (UINT8*)&ct[index * 16];

  //  cti++;
  //  if(cti > 3) cti = 0;

  return TRUE;
}

BOOLEAN
get_vrammemsize_val (
  value_t   *val,
  INTN      index
) {
  INTN     idx = -1;
  UINT64   memsize;

  idx++;
  memsize = LShiftU64((UINT64)card->vram_size, 32);

  if (idx == 0) {
    memsize = memsize | (UINT64)card->vram_size;
  }

  val->type = kCst;
  val->size = 8;
  val->data = (UINT8 *)&memsize;

  return TRUE;
}

BOOLEAN
get_binimage_val (
  value_t   *val,
  INTN      index
) {
  if (!card->rom) {
    return FALSE;
  }

  val->type = kPtr;
  val->size = card->rom_size;
  val->data = card->rom;

  return TRUE;
}

BOOLEAN
get_binimage_owr (
  value_t   *val,
  INTN      index
) {
  static UINT32   v = 0;

  if (!gSettings.LoadVBios) {
    return FALSE;
  }

  v = 1;
  val->type = kCst;
  val->size = 4;
  val->data = (UINT8 *)&v;

  return TRUE;
}



BOOLEAN
get_romrevision_val (
  value_t   *val,
  INTN      index
) {
  CHAR8   *cRev="109-B77101-00";
  UINT8   *rev;

  if (!card->rom){
    val->type = kPtr;
    val->size = 13;
    val->data = AllocateZeroPool(val->size);
    if (!val->data)
      return FALSE;

    CopyMem(val->data, cRev, val->size);

    return TRUE;
  }

  rev = card->rom + *(UINT8 *)(card->rom + OFFSET_TO_GET_ATOMBIOS_STRINGS_START);

  val->type = kPtr;
  val->size = (UINT32)AsciiStrLen((CHAR8 *)rev);

  if ((val->size < 3) || (val->size > 30)) { //fool proof. Real value 13
    rev = (UINT8 *)cRev;
    val->size = 13;
  }

  val->data = AllocateZeroPool(val->size);

  if (!val->data) {
    return FALSE;
  }

  CopyMem(val->data, rev, val->size);

  return TRUE;
}

BOOLEAN
get_deviceid_val (
  value_t   *val,
  INTN      index
) {
  val->type = kCst;
  val->size = 2;
  val->data = (UINT8 *)&card->pci_dev->device_id;

  return TRUE;
}

BOOLEAN
get_mclk_val (
  value_t   *val,
  INTN      index
) {
  return FALSE;
}

BOOLEAN
get_sclk_val (
  value_t   *val,
  INTN      index
) {
  return FALSE;
}

BOOLEAN
get_refclk_val (
  value_t   *val,
  INTN      index
) {
  //if (!gSettings.RefCLK) {
    return FALSE;
  //}

  //val->type = kCst;
  //val->size = 4;
  //val->data = (UINT8 *)&gSettings.RefCLK;

  //return TRUE;
}

BOOLEAN
get_platforminfo_val (
  value_t   *val,
  INTN      index
) {
  val->data = AllocateZeroPool(0x80);

  if (!val->data) {
    return FALSE;
  }

  //  bzero(val->data, 0x80);

  val->type   = kPtr;
  val->size   = 0x80;
  val->data[0]  = 1;

  return TRUE;
}

BOOLEAN
get_vramtotalsize_val (
  value_t   *val,
  INTN      index
) {
  val->type = kCst;
  val->size = 4;
  val->data = (UINT8 *)&card->vram_size;

  return TRUE;
}

VOID
free_val (
  value_t   *val
) {
  if (val->type == kPtr) {
    FreePool(val->data);
  }

  ZeroMem(val, sizeof(value_t));
}

VOID
devprop_add_list (
  AtiDevProp    devprop_list[]
) {
  INTN      i, pnum;
  value_t   *val = AllocateZeroPool(sizeof(value_t));

  for (i = 0; devprop_list[i].name != NULL; i++) {
    if ((devprop_list[i].flags == FLAGTRUE) || (devprop_list[i].flags & card->flags)) {
      if (devprop_list[i].get_value && devprop_list[i].get_value(val, 0)) {
        devprop_add_value(card->device, devprop_list[i].name, val->data, val->size);
        free_val(val);

        if (devprop_list[i].all_ports) {
          for (pnum = 1; pnum < card->ports; pnum++) {
            if (devprop_list[i].get_value(val, pnum)) {
              devprop_list[i].name[1] = (CHAR8)(0x30 + pnum); // convert to ascii
              devprop_add_value(card->device, devprop_list[i].name, val->data, val->size);
              free_val(val);
            }
          }

          devprop_list[i].name[1] = 0x30; // write back our "@0," for a next possible card
        }
      } else {
        if (devprop_list[i].default_val.type != kNul) {
          devprop_add_value(
            card->device, devprop_list[i].name,
            devprop_list[i].default_val.type == kCst
              ? (UINT8 *)&(devprop_list[i].default_val.data)
              : devprop_list[i].default_val.data,
            devprop_list[i].default_val.size
          );
        }

        if (devprop_list[i].all_ports) {
          for (pnum = 1; pnum < card->ports; pnum++) {
            if (devprop_list[i].default_val.type != kNul) {
              devprop_list[i].name[1] = (CHAR8)(0x30 + pnum); // convert to ascii
              devprop_add_value(
                card->device, devprop_list[i].name,
                devprop_list[i].default_val.type == kCst
                  ? (UINT8 *)&(devprop_list[i].default_val.data)
                  : devprop_list[i].default_val.data,
                devprop_list[i].default_val.size
              );
            }
          }

          devprop_list[i].name[1] = 0x30; // write back our "@0," for a next possible card
        }
      }
    }
  }

  FreePool(val);
}

BOOLEAN
validate_rom (
  option_rom_header_t   *rom_header,
  pci_dt_t              *pci_dev
) {
  option_rom_pci_header_t   *rom_pci_header;

  if (rom_header->signature != 0xaa55){
    DBG("invalid ROM signature %x\n", rom_header->signature);
    return FALSE;
  }

  rom_pci_header = (option_rom_pci_header_t *)((UINT8 *)rom_header + rom_header->pci_header_offset);

  if (rom_pci_header->signature != 0x52494350) {
    DBG("invalid ROM header %x\n", rom_pci_header->signature);
    return FALSE;
  }

  if (
    (rom_pci_header->vendor_id != pci_dev->vendor_id) ||
    (rom_pci_header->device_id != pci_dev->device_id)
  ){
    DBG("invalid ROM vendor=%x deviceID=%d\n", rom_pci_header->vendor_id, rom_pci_header->device_id);
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
load_vbios_file (
  UINT16  vendor_id,
  UINT16  device_id
) {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  UINTN         bufferLen = 0;
  CHAR16        FileName[64], *RomPath = Basename(PoolPrint(DIR_ROM, L""));
  UINT8         *buffer = 0;

  //if we are here then TRUE
  //  if (!gSettings.LoadVBios)
  //    return FALSE;


  UnicodeSPrint(FileName, 128, L"%s\\%04x_%04x.rom", RomPath, vendor_id, device_id);
  if (FileExists(OEMDir, FileName)){
    DBG("Found generic VBIOS ROM file (%04x_%04x.rom)\n", vendor_id, device_id);
    Status = egLoadFile(OEMDir, FileName, &buffer, &bufferLen);
  }

  if (EFI_ERROR(Status)) {
    FreePool(RomPath);
    RomPath = PoolPrint(DIR_ROM, DIR_CLOVER);

    UnicodeSPrint(FileName, 128, L"%s\\%04x_%04x.rom", RomPath, vendor_id, device_id);
    if (FileExists(SelfRootDir, FileName)){
      DBG("Found generic VBIOS ROM file (%04x_%04x.rom)\n", vendor_id, device_id);
      Status = egLoadFile(SelfRootDir, FileName, &buffer, &bufferLen);
    }
  }

  FreePool(RomPath);

  if (EFI_ERROR(Status) || (bufferLen == 0)){
    DBG("ATI ROM not found \n");
    card->rom_size = 0;
    card->rom = 0;
    return FALSE;
  }

  DBG("Loaded ROM len=%d\n", bufferLen);

  card->rom_size = (UINT32)bufferLen;
  card->rom = AllocateZeroPool(bufferLen);

  if (!card->rom) {
    return FALSE;
  }

  CopyMem(card->rom, buffer, bufferLen);
  //  read(fd, (CHAR8 *)card->rom, card->rom_size);

  if (!validate_rom((option_rom_header_t *)card->rom, card->pci_dev)) {
    DBG("validate_rom fails\n");
    card->rom_size = 0;
    card->rom = 0;
    return FALSE;
  }

  bufferLen = ((option_rom_header_t *)card->rom)->rom_size;
  card->rom_size = (UINT32)(bufferLen << 9);
  DBG("Calculated ROM len=%d\n", card->rom_size);
  //  close(fd);
  FreePool(buffer);

  return TRUE;
}

void
get_vram_size() {
  //check card->vram_size in bytes!
  ati_chip_family_t chip_family = card->info->chip_family;

  card->vram_size = 128 << 20; //default 128Mb, this is minimum for OS
  if (gSettings.VRAM != 0) {
    card->vram_size = gSettings.VRAM;
    DBG("Set VRAM from config=%dMb\n", (INTN)RShiftU64(card->vram_size, 20));
    //    WRITEREG32(card->mmio, RADEON_CONFIG_MEMSIZE, card->vram_size);
  } else {
    if (chip_family >= CHIP_FAMILY_CEDAR) {
      // size in MB on evergreen
      // XXX watch for overflow!!!
      card->vram_size = ((UINT64)REG32(card->mmio, R600_CONFIG_MEMSIZE)) << 20;
      DBG("Set VRAM for Cedar+ =%dMb\n", (INTN)RShiftU64(card->vram_size, 20));
    } else if (chip_family >= CHIP_FAMILY_R600) {
      card->vram_size = REG32(card->mmio, R600_CONFIG_MEMSIZE);
    } else {
      card->vram_size = REG32(card->mmio, RADEON_CONFIG_MEMSIZE);
      if (card->vram_size == 0) {
        card->vram_size = REG32(card->mmio, RADEON_CONFIG_APER_SIZE);
        //Slice - previously I successfully made Radeon9000 working
        //by writing this register
        WRITEREG32(card->mmio, RADEON_CONFIG_MEMSIZE, (UINT32)card->vram_size);
      }
    }
  }

  gSettings.VRAM = card->vram_size;
  DBG("ATI: get_vram_size returned 0x%x\n", card->vram_size);
}

BOOLEAN
read_vbios (
  BOOLEAN   from_pci
) {
  option_rom_header_t *rom_addr;

  if (from_pci) {
    rom_addr = (option_rom_header_t *)(UINTN)(pci_config_read32(card->pci_dev, PCI_EXPANSION_ROM_BASE) & ~0x7ff);
    DBG(" @0x%x\n", rom_addr);
  } else {
    rom_addr = (option_rom_header_t *)(UINTN)0xc0000;
  }

  if (!validate_rom(rom_addr, card->pci_dev)){
    DBG("There is no ROM @0x%x\n", rom_addr);
    //   gBS->Stall(3000000);
    return FALSE;
  }

  card->rom_size = (UINT32)(rom_addr->rom_size) << 9;

  if (!card->rom_size){
    DBG("invalid ROM size =0\n");
    return FALSE;
  }

  card->rom = AllocateZeroPool(card->rom_size);

  if (!card->rom) {
    return FALSE;
  }

  CopyMem(card->rom, (void *)rom_addr, card->rom_size);

  return TRUE;
}

BOOLEAN
read_disabled_vbios () {
  BOOLEAN               ret = FALSE;
  ati_chip_family_t     chip_family = card->info->chip_family;

  if (chip_family >= CHIP_FAMILY_RV770) {
    UINT32    viph_control       = REG32(card->mmio, RADEON_VIPH_CONTROL),
              bus_cntl           = REG32(card->mmio, RADEON_BUS_CNTL),
              d1vga_control      = REG32(card->mmio, AVIVO_D1VGA_CONTROL),
              d2vga_control      = REG32(card->mmio, AVIVO_D2VGA_CONTROL),
              vga_render_control = REG32(card->mmio, AVIVO_VGA_RENDER_CONTROL),
              rom_cntl           = REG32(card->mmio, R600_ROM_CNTL),
              cg_spll_func_cntl  = 0,
              cg_spll_status;

    // disable VIP
    WRITEREG32(card->mmio, RADEON_VIPH_CONTROL, (viph_control & ~RADEON_VIPH_EN));

    // enable the rom
    WRITEREG32(card->mmio, RADEON_BUS_CNTL, (bus_cntl & ~RADEON_BUS_BIOS_DIS_ROM));

    // Disable VGA mode
    WRITEREG32(card->mmio, AVIVO_D1VGA_CONTROL, (d1vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
    WRITEREG32(card->mmio, AVIVO_D2VGA_CONTROL, (d2vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
    WRITEREG32(card->mmio, AVIVO_VGA_RENDER_CONTROL, (vga_render_control & ~AVIVO_VGA_VSTATUS_CNTL_MASK));

    if (chip_family == CHIP_FAMILY_RV730) {
      cg_spll_func_cntl = REG32(card->mmio, R600_CG_SPLL_FUNC_CNTL);

      // enable bypass mode
      WRITEREG32(card->mmio, R600_CG_SPLL_FUNC_CNTL, (cg_spll_func_cntl | R600_SPLL_BYPASS_EN));

      // wait for SPLL_CHG_STATUS to change to 1
      cg_spll_status = 0;
      while (!(cg_spll_status & R600_SPLL_CHG_STATUS))
        cg_spll_status = REG32(card->mmio, R600_CG_SPLL_STATUS);

      WRITEREG32(card->mmio, R600_ROM_CNTL, (rom_cntl & ~R600_SCK_OVERWRITE));
    } else {
      WRITEREG32(card->mmio, R600_ROM_CNTL, (rom_cntl | R600_SCK_OVERWRITE));
    }

    ret = read_vbios(TRUE);

    // restore regs
    if (chip_family == CHIP_FAMILY_RV730) {
      WRITEREG32(card->mmio, R600_CG_SPLL_FUNC_CNTL, cg_spll_func_cntl);

      // wait for SPLL_CHG_STATUS to change to 1
      cg_spll_status = 0;
      while (!(cg_spll_status & R600_SPLL_CHG_STATUS))
        cg_spll_status = REG32(card->mmio, R600_CG_SPLL_STATUS);
    }

    WRITEREG32(card->mmio, RADEON_VIPH_CONTROL, viph_control);
    WRITEREG32(card->mmio, RADEON_BUS_CNTL, bus_cntl);
    WRITEREG32(card->mmio, AVIVO_D1VGA_CONTROL, d1vga_control);
    WRITEREG32(card->mmio, AVIVO_D2VGA_CONTROL, d2vga_control);
    WRITEREG32(card->mmio, AVIVO_VGA_RENDER_CONTROL, vga_render_control);
    WRITEREG32(card->mmio, R600_ROM_CNTL, rom_cntl);
  } else if (chip_family >= CHIP_FAMILY_R600) {
      UINT32    viph_control                  = REG32(card->mmio, RADEON_VIPH_CONTROL),
                bus_cntl                      = REG32(card->mmio, RADEON_BUS_CNTL),
                d1vga_control                 = REG32(card->mmio, AVIVO_D1VGA_CONTROL),
                d2vga_control                 = REG32(card->mmio, AVIVO_D2VGA_CONTROL),
                vga_render_control            = REG32(card->mmio, AVIVO_VGA_RENDER_CONTROL),
                rom_cntl                      = REG32(card->mmio, R600_ROM_CNTL),
                general_pwrmgt                = REG32(card->mmio, R600_GENERAL_PWRMGT),
                low_vid_lower_gpio_cntl       = REG32(card->mmio, R600_LOW_VID_LOWER_GPIO_CNTL),
                medium_vid_lower_gpio_cntl    = REG32(card->mmio, R600_MEDIUM_VID_LOWER_GPIO_CNTL),
                high_vid_lower_gpio_cntl      = REG32(card->mmio, R600_HIGH_VID_LOWER_GPIO_CNTL),
                ctxsw_vid_lower_gpio_cntl     = REG32(card->mmio, R600_CTXSW_VID_LOWER_GPIO_CNTL),
                lower_gpio_enable             = REG32(card->mmio, R600_LOWER_GPIO_ENABLE);

      // disable VIP
      WRITEREG32(card->mmio, RADEON_VIPH_CONTROL, (viph_control & ~RADEON_VIPH_EN));

      // enable the rom
      WRITEREG32(card->mmio, RADEON_BUS_CNTL, (bus_cntl & ~RADEON_BUS_BIOS_DIS_ROM));

      // Disable VGA mode
      WRITEREG32(card->mmio, AVIVO_D1VGA_CONTROL, (d1vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
      WRITEREG32(card->mmio, AVIVO_D2VGA_CONTROL, (d2vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE | AVIVO_DVGA_CONTROL_TIMING_SELECT)));
      WRITEREG32(card->mmio, AVIVO_VGA_RENDER_CONTROL, (vga_render_control & ~AVIVO_VGA_VSTATUS_CNTL_MASK));
      WRITEREG32(card->mmio, R600_ROM_CNTL, ((rom_cntl & ~R600_SCK_PRESCALE_CRYSTAL_CLK_MASK) | (1 << R600_SCK_PRESCALE_CRYSTAL_CLK_SHIFT) | R600_SCK_OVERWRITE));
      WRITEREG32(card->mmio, R600_GENERAL_PWRMGT, (general_pwrmgt & ~R600_OPEN_DRAIN_PADS));
      WRITEREG32(card->mmio, R600_LOW_VID_LOWER_GPIO_CNTL, (low_vid_lower_gpio_cntl & ~0x400));
      WRITEREG32(card->mmio, R600_MEDIUM_VID_LOWER_GPIO_CNTL, (medium_vid_lower_gpio_cntl & ~0x400));
      WRITEREG32(card->mmio, R600_HIGH_VID_LOWER_GPIO_CNTL, (high_vid_lower_gpio_cntl & ~0x400));
      WRITEREG32(card->mmio, R600_CTXSW_VID_LOWER_GPIO_CNTL, (ctxsw_vid_lower_gpio_cntl & ~0x400));
      WRITEREG32(card->mmio, R600_LOWER_GPIO_ENABLE, (lower_gpio_enable | 0x400));

      ret = read_vbios(TRUE);

      // restore regs
      WRITEREG32(card->mmio, RADEON_VIPH_CONTROL, viph_control);
      WRITEREG32(card->mmio, RADEON_BUS_CNTL, bus_cntl);
      WRITEREG32(card->mmio, AVIVO_D1VGA_CONTROL, d1vga_control);
      WRITEREG32(card->mmio, AVIVO_D2VGA_CONTROL, d2vga_control);
      WRITEREG32(card->mmio, AVIVO_VGA_RENDER_CONTROL, vga_render_control);
      WRITEREG32(card->mmio, R600_ROM_CNTL, rom_cntl);
      WRITEREG32(card->mmio, R600_GENERAL_PWRMGT, general_pwrmgt);
      WRITEREG32(card->mmio, R600_LOW_VID_LOWER_GPIO_CNTL, low_vid_lower_gpio_cntl);
      WRITEREG32(card->mmio, R600_MEDIUM_VID_LOWER_GPIO_CNTL, medium_vid_lower_gpio_cntl);
      WRITEREG32(card->mmio, R600_HIGH_VID_LOWER_GPIO_CNTL, high_vid_lower_gpio_cntl);
      WRITEREG32(card->mmio, R600_CTXSW_VID_LOWER_GPIO_CNTL, ctxsw_vid_lower_gpio_cntl);
      WRITEREG32(card->mmio, R600_LOWER_GPIO_ENABLE, lower_gpio_enable);
    }

  return ret;
}

BOOLEAN
radeon_card_posted () {
  UINT32 reg = REG32(card->mmio, RADEON_CRTC_GEN_CNTL) | REG32(card->mmio, RADEON_CRTC2_GEN_CNTL);

  if (reg & RADEON_CRTC_EN) {
    return TRUE;
  }

  // then check MEM_SIZE, in case something turned the crtcs off
  reg = REG32(card->mmio, R600_CONFIG_MEMSIZE);

  if (reg) {
    return TRUE;
  }

  return FALSE;
}

static
BOOLEAN
init_card (
  pci_dt_t    *pci_dev
) {
  BOOLEAN     add_vbios = gSettings.LoadVBios;
  CHAR8       *name, *name_parent, *CfgName;
  UINTN       i, j, ExpansionRom = 0;
  INTN        NameLen = 0, n_ports = 0;

  card = AllocateZeroPool(sizeof(card_t));

  if (!card) {
    return FALSE;
  }

  card->pci_dev = pci_dev;

  for (i = 0; radeon_cards[i].device_id ; i++) {
    card->info = &radeon_cards[i];
    if (radeon_cards[i].device_id == pci_dev->device_id) break;
  }

  for (j = 0; j < NGFX; j++) {
    if ((gGraphics[j].Vendor == Ati) &&
        (gGraphics[j].DeviceID == pci_dev->device_id)) {
      //      model = gGraphics[j].Model;
      n_ports = gGraphics[j].Ports;
      add_vbios = gGraphics[j].LoadVBios;
      break;
    }
  }

  if (!card->info || !card->info->device_id || !card->info->cfg_name) {
    DBG("Unsupported ATI card! Device ID: [%04x:%04x] Subsystem ID: [%08x] \n",
        pci_dev->vendor_id, pci_dev->device_id, pci_dev->subsys_id);
    DBG("search for brothers family\n");
    for (i = 0; radeon_cards[i].device_id ; i++) {
      if ((radeon_cards[i].device_id & ~0xf) == (pci_dev->device_id & ~0xf)) {
        card->info = &radeon_cards[i];
        break;
      }
    }
    if (!card->info->cfg_name) {
      DBG("...compatible config is not found\n");
      return FALSE;
    }
  }

  card->fb    = (UINT8 *)(UINTN)(pci_config_read32(pci_dev, PCI_BASE_ADDRESS_0) & ~0x0f);
  card->mmio  = (UINT8 *)(UINTN)(pci_config_read32(pci_dev, PCI_BASE_ADDRESS_2) & ~0x0f);
  card->io    = (UINT8 *)(UINTN)(pci_config_read32(pci_dev, PCI_BASE_ADDRESS_4) & ~0x03);
  pci_dev->regs = card->mmio;
  ExpansionRom = pci_config_read32(pci_dev, PCI_EXPANSION_ROM_BASE); //0x30 as Chimera

  DBG("Framebuffer @0x%08X  MMIO @0x%08X  I/O Port @0x%08X ROM Addr @0x%08X\n",
      card->fb, card->mmio, card->io, ExpansionRom);

  card->posted = radeon_card_posted();

  DBG("ATI card %a\n", card->posted ? "POSTed" : "non-POSTed");

  get_vram_size();

  if (add_vbios) {
    load_vbios_file(pci_dev->vendor_id, pci_dev->device_id);
    if (!card->rom) {
      DBG("reading VBIOS from %a", card->posted ? "legacy space" : "PCI ROM");
      if (card->posted) { // && ExpansionRom != 0)
        read_vbios(FALSE);
      } else {
        read_disabled_vbios();
      }
      DBG("\n");
    } else {
      DBG("VideoBIOS read from file\n");
    }

  }

  if (card->info->chip_family >= CHIP_FAMILY_CEDAR) {
    DBG("ATI Radeon EVERGREEN family\n");
    card->flags |= EVERGREEN;
  }

  if (card->info->chip_family <= CHIP_FAMILY_RV670) {
    DBG("ATI Radeon OLD family\n");
    card->flags |= FLAGOLD;
  }

  if (gMobile) {
    DBG("ATI Mobile Radeon\n");
    card->flags |= FLAGMOBILE;
  }

  card->flags |= FLAGNOTFAKE;

  NameLen = StrLen(gSettings.FBName);
  if (NameLen > 2) {  //fool proof: cfg_name is 3 character or more.
    CfgName = AllocateZeroPool(NameLen);
    UnicodeStrToAsciiStr((CHAR16*)&gSettings.FBName[0], CfgName);
    DBG("Users config name %a\n", CfgName);
    card->cfg_name = CfgName;
  } else {
    // use cfg_name on radeon_cards, to retrive the default name from card_configs,
    card->cfg_name = card_configs[card->info->cfg_name].name;
    n_ports = card_configs[card->info->cfg_name].ports;
    // which means one of the fb's or kNull
    DBG("Framebuffer set to device's default: %a\n", card->cfg_name);
    DBG(" N ports defaults to %d\n", n_ports);
  }

  if (gSettings.VideoPorts != 0) {
    n_ports = gSettings.VideoPorts;
    DBG(" use N ports setting from config.plist: %d\n", n_ports);
  }

  if (n_ports > 0) {
    card->ports = (UINT8)n_ports; // use it.
    DBG("(AtiPorts) Nr of ports set to: %d\n", card->ports);
  } else {
    // if (card->cfg_name > 0) // do we want 0 ports if fb is kNull or mistyped ?

    // else, match cfg_name with card_configs list and retrive default nr of ports.
    for (i = 0; i < kCfgEnd; i++) {
      if (AsciiStrCmp(card->cfg_name, card_configs[i].name) == 0) {
        card->ports = card_configs[i].ports; // default
      }
    }

    DBG("Nr of ports set to framebuffer's default: %d\n", card->ports);
  }

  if (card->ports == 0) {
    card->ports = 2; //real minimum
    DBG("Nr of ports set to min: %d\n", card->ports);
  }

  //
  name = AllocateZeroPool(24);
  AsciiSPrint(name, 24, "ATY,%a", card->cfg_name);
  aty_name.type = kStr;
  aty_name.size = (UINT32)AsciiStrLen(name);
  aty_name.data = (UINT8 *)name;

  name_parent = AllocateZeroPool(24);
  AsciiSPrint(name_parent, 24, "ATY,%aParent", card->cfg_name);
  aty_nameparent.type = kStr;
  aty_nameparent.size = (UINT32)AsciiStrLen(name_parent);
  aty_nameparent.data = (UINT8 *)name_parent;

  //how can we free pool when we leave the procedure? Make all pointers global?
  return TRUE;
}

BOOLEAN
setup_ati_devprop (
  LOADER_ENTRY  *Entry,
  pci_dt_t      *ati_dev
) {
  CHAR8     compatible[64], *devicepath;
  UINT32    FakeID = 0;
  INT32     i;

  if (!init_card(ati_dev)) {
    return FALSE;
  }

  CurrentPatches = Entry->KernelAndKextPatches;

  // -------------------------------------------------
  // Find a better way to do this (in device_inject.c)
  if (!string) {
    string = devprop_create_string();
  }

  devicepath = get_pci_dev_path(ati_dev);
  //card->device = devprop_add_device(string, devicepath);
  card->device = devprop_add_device_pci(string, ati_dev);

  if (!card->device) {
    return FALSE;
  }

  // -------------------------------------------------

  if (gSettings.FakeATI) {
    card->flags &= ~FLAGNOTFAKE;
    card->flags |= FLAGOLD;

    FakeID = gSettings.FakeATI >> 16;
    devprop_add_value(card->device, "device-id", (UINT8*)&FakeID, 4);
    devprop_add_value(card->device, "ATY,DeviceID", (UINT8*)&FakeID, 2);
    AsciiSPrint(compatible, 64, "pci1002,%04x", FakeID);
    AsciiStrCpy(compatible, AsciiStrToLower(compatible));
    devprop_add_value(card->device, "@0,compatible", (UINT8*)&compatible[0], 12);
    FakeID = gSettings.FakeATI & 0xFFFF;
    devprop_add_value(card->device, "vendor-id", (UINT8*)&FakeID, 4);
    devprop_add_value(card->device, "ATY,VendorID", (UINT8*)&FakeID, 2);
  }

  if (!gSettings.NoDefaultProperties) {
    devprop_add_list(ati_devprop_list);
    if (gSettings.UseIntelHDMI) {
      devprop_add_value(card->device, "hda-gfx", (UINT8*)"onboard-2", 10);
    } else {
      devprop_add_value(card->device, "hda-gfx", (UINT8*)"onboard-1", 10);
    }
  } else {
    DBG("ATI: No default properties injected\n");
  }

  if (gSettings.NrAddProperties != 0xFFFE) {
    for (i = 0; i < gSettings.NrAddProperties; i++) {
      if (gSettings.AddProperties[i].Device != DEV_ATI) {
        continue;
      }

      devprop_add_value(
        card->device,
        gSettings.AddProperties[i].Key,
        (UINT8*)gSettings.AddProperties[i].Value,
        gSettings.AddProperties[i].ValueLen
      );
    }
  }

  DBG("ATI %a %a %dMB (%a) [%04x:%04x] (subsys [%04x:%04x]) | %a\n",
      //chip_family_name[card->info->chip_family], card->info->model_name,
      chip_family_name[card->info->chip_family], S_ATIMODEL,
      (UINT32)RShiftU64(card->vram_size, 20), card->cfg_name,
      ati_dev->vendor_id, ati_dev->device_id,
      ati_dev->subsys_id.subsys.vendor_id, ati_dev->subsys_id.subsys.device_id,
      devicepath);

  //  FreePool(card->info); //TODO we can't free constant so this info should be copy of constants
  FreePool(card);

  return TRUE;
}
