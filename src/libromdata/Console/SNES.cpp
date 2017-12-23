/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * SNES.cpp: Super Nintendo ROM image reader.                              *
 *                                                                         *
 * Copyright (c) 2016-2017 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "SNES.hpp"
#include "librpbase/RomData_p.hpp"

#include "data/NintendoPublishers.hpp"
#include "snes_structs.h"
#include "CopierFormats.h"

// librpbase
#include "librpbase/common.h"
#include "librpbase/byteswap.h"
#include "librpbase/TextFuncs.hpp"
#include "librpbase/SystemRegion.hpp"
#include "librpbase/file/IRpFile.hpp"
#include "librpbase/file/FileSystem.hpp"
#include "libi18n/i18n.h"
using namespace LibRpBase;

// C includes. (C++ namespace)
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstring>

// C++ includes.
#include <string>
#include <vector>
using std::string;
using std::vector;

namespace LibRomData {

class SNESPrivate : public RomDataPrivate
{
	public:
		SNESPrivate(SNES *q, IRpFile *file);

	private:
		typedef RomDataPrivate super;
		RP_DISABLE_COPY(SNESPrivate)

	public:
		/**
		 * Is the specified SNES/SFC ROM header valid?
		 * @param romHeader SNES/SFC ROM header to check.
		 * @param isHiROM True if the header was read from a HiROM address; false if not.
		 * @return True if the SNES/SFC ROM header is valid; false if not.
		 */
		static bool isSnesRomHeaderValid(const SNES_RomHeader *romHeader, bool isHiROM);

		/**
		 * Is the specified BS-X ROM header valid?
		 * @param romHeader BS-X ROM header to check.
		 * @param isHiROM True if the header was read from a HiROM address; false if not.
		 * @return True if the BS-X ROM header is valid; false if not.
		 */
		static bool isBsxRomHeaderValid(const SNES_RomHeader *romHeader, bool isHiROM);

	public:
		enum SNES_RomType {
			ROM_UNKNOWN = -1,	// Unknown ROM type.
			ROM_SNES = 0,		// SNES/SFC ROM image.
			ROM_BSX = 1,		// BS-X ROM image.
		};

		// ROM type.
		int romType;

		// ROM header.
		// NOTE: Must be byteswapped on access.
		SNES_RomHeader romHeader;
		uint32_t header_address;
};

/** SNESPrivate **/

SNESPrivate::SNESPrivate(SNES *q, IRpFile *file)
	: super(q, file)
	, romType(ROM_UNKNOWN)
	, header_address(0)
{
	// Clear the ROM header struct.
	memset(&romHeader, 0, sizeof(romHeader));
}

/**
 * Is the specified SNES/SFC ROM header valid?
 * @param romHeader SNES/SFC ROM header to check.
 * @param isHiROM True if the header was read from a HiROM address; false if not.
 * @return True if the SNES/SFC ROM header is valid; false if not.
 */
bool SNESPrivate::isSnesRomHeaderValid(const SNES_RomHeader *romHeader, bool isHiROM)
{
	// Game title: Should be ASCII.
	for (int i = 0; i < ARRAY_SIZE(romHeader->snes.title); i++) {
		if (romHeader->snes.title[i] & 0x80) {
			// Invalid character.
			return false;
		}
	}

	// Is the ROM mapping byte valid?
	switch (romHeader->snes.rom_mapping) {
		case SNES_ROMMAPPING_LoROM:
		case SNES_ROMMAPPING_LoROM_FastROM:
		case SNES_ROMMAPPING_ExLoROM:
			if (isHiROM) {
				// LoROM mapping at a HiROM address.
				// Not valid.
				return false;
			}
			// Valid ROM mapping byte.
			break;

		case SNES_ROMMAPPING_HiROM:
		case SNES_ROMMAPPING_HiROM_FastROM:
		case SNES_ROMMAPPING_ExHiROM:
			if (!isHiROM) {
				// HiROM mapping at a LoROM address.
				// Not valid.
				return false;
			}

			// Valid ROM mapping byte.
			break;

		default:
			// Not valid.
			return false;
	}

	// Is the ROM type byte valid?
	// TODO: Check if any other types exist.
	if ( (romHeader->snes.rom_type & SNES_ROMTYPE_ROM_MASK) > SNES_ROMTYPE_ROM_BATT_ENH ||
	    ((romHeader->snes.rom_type & SNES_ROMTYPE_ENH_MASK) >= 0x50 &&
	     (romHeader->snes.rom_type & SNES_ROMTYPE_ENH_MASK) <= 0xD0))
	{
		// Not a valid ROM type.
		return false;
	}

	// Check the extended header.
	if (romHeader->snes.old_publisher_code == 0x33) {
		// Extended header should be present.
		// New publisher code and game ID must be alphanumeric.
		if (!isalnum(romHeader->snes.ext.new_publisher_code[0]) ||
		    !isalnum(romHeader->snes.ext.new_publisher_code[1]))
		{
			// New publisher code is invalid.
			return false;
		}

		// Game ID must contain alphanumeric characters or a space.
		for (int i = 0; i < ARRAY_SIZE(romHeader->snes.ext.id4); i++) {
			// ID4 should be in the format "SMWJ" or "MW  ".
			if (isalnum(romHeader->snes.ext.id4[i])) {
				// Alphanumeric character.
				continue;
			} else if (romHeader->snes.ext.id4[i] == ' ' && i >= 2) {
				// Some game IDs are two characters,
				// and the remaining characters are spaces.
				continue;
			}

			// Invalid character.
			return false;
		}
	}

	// ROM header appears to be valid.
	return true;
}

/**
 * Is the specified BS-X ROM header valid?
 * @param romHeader BS-X ROM header to check.
 * @param isHiROM True if the header was read from a HiROM address; false if not.
 * @return True if the BS-X ROM header is valid; false if not.
 */
bool SNESPrivate::isBsxRomHeaderValid(const SNES_RomHeader *romHeader, bool isHiROM)
{
	// TODO: Game title may be ASCII or Shift-JIS.
	// For now, just make sure the first byte isn't 0.
	if (romHeader->bsx.title[0] == 0) {
		// Title is empty.
		return false;
	}

	// Is the ROM mapping byte valid?
	switch (romHeader->bsx.rom_mapping) {
		case SNES_ROMMAPPING_LoROM:
		case SNES_ROMMAPPING_LoROM_FastROM:
			if (isHiROM) {
				// LoROM mapping at a HiROM address.
				// Not valid.
				return false;
			}
			// Valid ROM mapping byte.
			break;

		case SNES_ROMMAPPING_HiROM:
		case SNES_ROMMAPPING_HiROM_FastROM:
			if (!isHiROM) {
				// HiROM mapping at a LoROM address.
				// Not valid.
				return false;
			}

			// Valid ROM mapping byte.
			break;

		case SNES_ROMMAPPING_ExLoROM:
		case SNES_ROMMAPPING_ExHiROM:
		default:
			// Not valid.
			// (ExLoROM/ExHiROM is not valid for BS-X.)
			return false;
	}

	// Old publisher code must be either 0x33 or 0x00.
	// 0x00 indicates the file was deleted.
	if (romHeader->bsx.old_publisher_code != 0x33 &&
	    romHeader->bsx.old_publisher_code != 0x00)
	{
		// Invalid old publisher code.
		return false;
	}

	// New publisher code must be alphanumeric.
	if (!isalnum(romHeader->bsx.ext.new_publisher_code[0]) ||
	    !isalnum(romHeader->bsx.ext.new_publisher_code[1]))
	{
		// New publisher code is invalid.
		return false;
	}

	// ROM header appears to be valid.
	// TODO: Check other BS-X fields.
	return true;
}

/** SNES **/

/**
 * Read a Super Nintendo ROM image.
 *
 * A ROM file must be opened by the caller. The file handle
 * will be dup()'d and must be kept open in order to load
 * data from the ROM.
 *
 * To close the file, either delete this object or call close().
 *
 * NOTE: Check isValid() to determine if this is a valid ROM.
 *
 * @param file Open ROM image.
 */
SNES::SNES(IRpFile *file)
	: super(new SNESPrivate(this, file))
{
	RP_D(SNES);
	d->className = "SNES";

	if (!d->file) {
		// Could not dup() the file handle.
		return;
	}

	// TODO: Only allow supported file extensions.
	bool isCopierHeader = false;

	// TODO: BS-X heuristics.
	// For now, assuming that if the file extension starts with
	// ".b", it's a BS-X ROM image.
	const string filename = file->filename();
	if (!filename.empty()) {
		const char *ext = FileSystem::file_ext(filename);
		if (ext[0] == '.' && tolower(ext[1]) == 'b') {
			// BS-X ROM image.
			d->romType = SNESPrivate::ROM_BSX;
		}
	}

	if (d->romType == SNESPrivate::ROM_UNKNOWN) {
		// SNES ROMs don't necessarily have a header at the start of the file.
		// Therefore, we need to do a few reads and guessing.

		// Check if a copier header is present.
		SMD_Header smdHeader;
		d->file->rewind();
		size_t size = d->file->read(&smdHeader, sizeof(smdHeader));
		if (size != sizeof(smdHeader))
			return;

		if (smdHeader.id[0] == 0xAA && smdHeader.id[1] == 0xBB) {
			// TODO: Check page count?

			// Check that reserved fields are 0.
			bool areFieldsZero = true;
			for (int i = ARRAY_SIZE(smdHeader.reserved1)-1; i >= 0; i--) {
				if (smdHeader.reserved1[i] != 0) {
					areFieldsZero = false;
					break;
				}
			}
			if (!areFieldsZero) {
				for (int i = ARRAY_SIZE(smdHeader.reserved2)-1; i >= 0; i--) {
					if (smdHeader.reserved2[i] != 0) {
						areFieldsZero = false;
						break;
					}
				}
			}

			// If the fields are zero, this is a copier header.
			isCopierHeader = areFieldsZero;
		}

		if (!isCopierHeader) {
			// Check for "GAME DOCTOR SF ".
			// (UCON64 uses "GAME DOCTOR SF 3", but there's multiple versions.)
			static const char gdsf3[] = "GAME DOCTOR SF ";
			if (!memcmp(&smdHeader, gdsf3, sizeof(gdsf3)-1)) {
				// Game Doctor ROM header.
				isCopierHeader = true;
			} else {
				// Check for "SUPERUFO".
				static const char superufo[] = "SUPERUFO";
				const uint8_t *u8ptr = reinterpret_cast<const uint8_t*>(&smdHeader);
				if (!memcmp(&u8ptr[8], superufo, sizeof(superufo)-8)) {
					// Super UFO ROM header.
					isCopierHeader = true;
				}
			}
		}
	}

	// TODO: BS-X memory pack ROMs have the following at 0x7F00 or 0xFF00:
	// {'M', 0, 'P', 0, 0, 0, 0x79}

	// Header addresses to check.
	// If a copier header is detected, use index 1,
	// which checks +512 offsets first.
	static const uint32_t all_header_addresses[2][4] = {
		// Non-headered first.
		{0x7FB0, 0xFFB0, 0x7FB0+512, 0xFFB0+512},
		// Headered first.
		{0x7FB0+512, 0xFFB0+512, 0x7FB0, 0xFFB0},
	};

	d->header_address = 0;
	const uint32_t *pHeaderAddress = &all_header_addresses[isCopierHeader][0];
	for (int i = 0; i < 4; i++, pHeaderAddress++) {
		if (*pHeaderAddress == 0)
			break;

		size_t size = d->file->seekAndRead(*pHeaderAddress, &d->romHeader, sizeof(d->romHeader));
		if (size != sizeof(d->romHeader)) {
			// Seek and/or read error.
			continue;
		}

		if (d->romType == SNESPrivate::ROM_BSX) {
			// Check for a valid BS-X ROM header.
			if (d->isBsxRomHeaderValid(&d->romHeader, (i & 1))) {
				// ROM header is valid.
				d->header_address = *pHeaderAddress;
				break;
			}
		} else {
			// Check for a valid SNES/SFC ROM header.
			if (d->isSnesRomHeaderValid(&d->romHeader, (i & 1))) {
				// ROM header is valid.
				d->header_address = *pHeaderAddress;
				d->romType = SNESPrivate::ROM_SNES;
				break;
			}
		}
	}

	if (d->header_address == 0) {
		// No ROM header.
		d->romType = SNESPrivate::ROM_UNKNOWN;
		d->isValid = false;
		return;
	}

	// ROM header found.
	d->isValid = true;
}

/** ROM detection functions. **/

/**
 * Is a ROM image supported by this class?
 * @param info DetectInfo containing ROM detection information.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int SNES::isRomSupported_static(const DetectInfo *info)
{
	assert(info != nullptr);
	if (!info) {
		// Either no detection information was specified,
		// or the file extension is missing.
		return -1;
	}

	// SNES ROMs don't necessarily have a header at the start of the file.
	// Therefore, we're using the file extension.
	if (info->ext && info->ext[0] != 0) {
		const char *const *exts = supportedFileExtensions_static();
		if (!exts) {
			// Should not happen...
			return SNESPrivate::ROM_UNKNOWN;
		}
		for (; *exts != nullptr; exts++) {
			if (!strcasecmp(info->ext, *exts)) {
				// File extension is supported.
				if (*exts[1] == 'b') {
					// BS-X extension.
					return SNESPrivate::ROM_BSX;
				} else {
					// SNES/SFC extension.
					return SNESPrivate::ROM_SNES;
				}
			}
		}
	}

	// TODO: BS-X heuristics.

	if (info->header.addr == 0 && info->header.size >= 0x200) {
		// Check for "GAME DOCTOR SF ".
		// (UCON64 uses "GAME DOCTOR SF 3", but there's multiple versions.)
		static const char gdsf3[] = "GAME DOCTOR SF ";
		if (!memcmp(info->header.pData, gdsf3, sizeof(gdsf3)-1)) {
			// Game Doctor ROM header.
			return 0;
		}

		// Check for "SUPERUFO".
		static const char superufo[] = "SUPERUFO";
		if (!memcmp(&info->header.pData[8], superufo, sizeof(superufo)-8)) {
			// Super UFO ROM header.
			return 0;
		}
	}

	// Not supported.
	return -1;
}

/**
 * Is a ROM image supported by this object?
 * @param info DetectInfo containing ROM detection information.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int SNES::isRomSupported(const DetectInfo *info) const
{
	return isRomSupported_static(info);
}

/**
 * Get the name of the system the loaded ROM is designed for.
 * @param type System name type. (See the SystemName enum.)
 * @return System name, or nullptr if type is invalid.
 */
const char *SNES::systemName(unsigned int type) const
{
	RP_D(const SNES);
	if (!d->isValid || !isSystemNameTypeValid(type))
		return nullptr;

	// sysNames[] bitfield:
	// - Bits 0-1: Type. (short, long, abbreviation)
	// - Bits 2-3: Region.
	unsigned int idx = (type & SYSNAME_TYPE_MASK);

	// Localized SNES/SFC system names.
	static const char *const sysNames[16] = {
		// Japan: Super Famicom
		"Nintendo Super Famicom", "Super Famicom", "SFC", nullptr,
		// South Korea: Super Comboy
		"Hyundai Super Comboy", "Super Comboy", "SCB", nullptr,
		// Worldwide: Super NES
		"Super Nintendo Entertainment System", "Super NES", "SNES", nullptr,
		// Reserved.
		nullptr, nullptr, nullptr, nullptr
	};

	// BS-X system names.
	static const char *const sysNames_BSX[4] = {
		"Satellaview BS-X", "Satellaview", "BS-X", nullptr
	};

	switch (d->romType) {
		case SNESPrivate::ROM_SNES:
			// SNES/SFC ROM image. Handled later.
			break;
		case SNESPrivate::ROM_BSX:
			// BS-X was only released in Japan, so no
			// localization is necessary.
			return sysNames_BSX[idx];
		default:
			// Unknown.
			return nullptr;
	}

	bool foundRegion = false;
	switch (d->romHeader.snes.destination_code) {
		case SNES_DEST_JAPAN:
			idx |= (0 << 2);
			foundRegion = true;
			break;
		case SNES_DEST_SOUTH_KOREA:
			idx |= (1 << 2);
			foundRegion = true;
			break;

		case SNES_DEST_ALL:
		case SNES_DEST_OTHER_X:
		case SNES_DEST_OTHER_Y:
		case SNES_DEST_OTHER_Z:
			// Use the system locale.
			break;

		default: 
			if (d->romHeader.snes.destination_code <= SNES_DEST_AUSTRALIA) {
				idx |= (2 << 2);
				foundRegion = true;
			}
			break;
	}

	if (!foundRegion) {
		// Check the system locale.
		switch (SystemRegion::getCountryCode()) {
			case 'JP':
				idx |= (0 << 2);
				break;
			case 'KR':
				idx |= (1 << 2);
				break;
			default:
				idx |= (2 << 2);
				break;
		}
	}

	return sysNames[idx];
}

/**
 * Get a list of all supported file extensions.
 * This is to be used for file type registration;
 * subclasses don't explicitly check the extension.
 *
 * NOTE: The extensions do not include the leading dot,
 * e.g. "bin" instead of ".bin".
 *
 * NOTE 2: The array and the strings in the array should
 * *not* be freed by the caller.
 *
 * @return NULL-terminated array of all supported file extensions, or nullptr on error.
 */
const char *const *SNES::supportedFileExtensions_static(void)
{
	static const char *const exts[] = {
		".smc", ".swc", ".sfc",
		".fig", ".ufo",

		// BS-X
		".bs", ".bsx",

		nullptr
	};
	return exts;
}

/**
 * Get a list of all supported file extensions.
 * This is to be used for file type registration;
 * subclasses don't explicitly check the extension.
 *
 * NOTE: The extensions do not include the leading dot,
 * e.g. "bin" instead of ".bin".
 *
 * NOTE 2: The array and the strings in the array should
 * *not* be freed by the caller.
 *
 * @return NULL-terminated array of all supported file extensions, or nullptr on error.
 */
const char *const *SNES::supportedFileExtensions(void) const
{
	return supportedFileExtensions_static();
}

/**
 * Load field data.
 * Called by RomData::fields() if the field data hasn't been loaded yet.
 * @return Number of fields read on success; negative POSIX error code on error.
 */
int SNES::loadFieldData(void)
{
	RP_D(SNES);
	if (d->fields->isDataLoaded()) {
		// Field data *has* been loaded...
		return 0;
	} else if (!d->file || !d->file->isOpen()) {
		// File isn't open.
		return -EBADF;
	} else if (!d->isValid) {
		// Unknown save file type.
		return -EIO;
	}

	// ROM file header is read and byteswapped in the constructor.
	const SNES_RomHeader *const romHeader = &d->romHeader;
	d->fields->reserve(7); // Maximum of 7 fields.

	// Title.
	// TODO: Space elimination.
	d->fields->addField_string(C_("SNES", "Title"),
		latin1_to_utf8(romHeader->snes.title, sizeof(romHeader->snes.title)));

	// Game ID.
	// NOTE: Only valid if the old publisher code is 0x33.
	if (romHeader->snes.old_publisher_code == 0x33) {
		string id6(romHeader->snes.ext.id4, sizeof(romHeader->snes.ext.id4));
		if (romHeader->snes.ext.id4[2] == ' ' && romHeader->snes.ext.id4[3] == ' ') {
			// Two-character ID.
			// Don't append the publisher.
			id6.resize(2);
		} else {
			// Four-character ID.
			// Append the publisher.
			id6.append(romHeader->snes.ext.new_publisher_code, sizeof(romHeader->snes.ext.new_publisher_code));
		}
		d->fields->addField_string(C_("SNES", "Game ID"),
			latin1_to_utf8(id6.data(), (int)id6.size()));
	} else {
		// No game ID.
		d->fields->addField_string(C_("SNES", "Game ID"), C_("SNES", "Unknown"));
	}

	// Publisher.
	const char* publisher;
	if (romHeader->snes.old_publisher_code == 0x33) {
		publisher = NintendoPublishers::lookup(romHeader->snes.ext.new_publisher_code);
	} else {
		publisher = NintendoPublishers::lookup_old(romHeader->snes.old_publisher_code);
	}
	d->fields->addField_string(C_("SNES", "Publisher"),
		publisher ? publisher : C_("SNES", "Unknown"));

	// ROM mapping.
	const char *rom_mapping;
	switch (romHeader->snes.rom_mapping) {
		case SNES_ROMMAPPING_LoROM:
			rom_mapping = C_("SNES|ROMMapping", "LoROM");
			break;
		case SNES_ROMMAPPING_HiROM:
			rom_mapping = C_("SNES|ROMMapping", "HiROM");
			break;
		case SNES_ROMMAPPING_LoROM_FastROM:
			rom_mapping = C_("SNES|ROMMapping", "LoROM+FastROM");
			break;
		case SNES_ROMMAPPING_HiROM_FastROM:
			rom_mapping = C_("SNES|ROMMapping", "HiROM+FastROM");
			break;
		case SNES_ROMMAPPING_ExLoROM:
			rom_mapping = C_("SNES|ROMMapping", "ExLoROM");
			break;
		case SNES_ROMMAPPING_ExHiROM:
			rom_mapping = C_("SNES|ROMMapping", "ExHiROM");
			break;
		default:
			rom_mapping = nullptr;
			break;
	}
	if (rom_mapping) {
		d->fields->addField_string(C_("SNES", "ROM Mapping"), rom_mapping);
	} else {
		// Unknown ROM mapping.
		d->fields->addField_string(C_("SNES", "ROM Mapping"),
			rp_sprintf(C_("SNES", "Unknown (0x%02X)"), romHeader->snes.rom_mapping));
	}

	// Cartridge HW.
	// TODO: Make this translatable.
	static const char *const hw_base_tbl[16] = {
		"ROM", "ROM, RAM", "ROM, RAM, Battery",
		"ROM, ", "ROM, RAM, ", "ROM, RAM, Battery, ",
		"ROM, Battery, ", nullptr,

		nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr
	};
	static const char *const hw_enh_tbl[16] = {
		"DSP-1", "Super FX", "OBC-1", "SA-1",
		"S-DD1", "Unknown", "Unknown", "Unknown",
		"Unknown", "Unknown", "Unknown", "Unknown",
		"Unknown", "Unknown", "Other", "Custom Chip"
	};

	const char *const hw_base = hw_base_tbl[romHeader->snes.rom_type & SNES_ROMTYPE_ROM_MASK];
	if (hw_base) {
		if ((romHeader->snes.rom_type & SNES_ROMTYPE_ROM_MASK) >= SNES_ROMTYPE_ROM_ENH) {
			// Enhancement chip.
			const char *const hw_enh = hw_enh_tbl[(romHeader->snes.rom_type & SNES_ROMTYPE_ENH_MASK) >> 4];
			string str(hw_base);
			str.append(hw_enh);
			d->fields->addField_string(C_("SNES", "Cartridge HW"), str);
		} else {
			// No enhancement chip.
			d->fields->addField_string(C_("SNES", "Cartridge HW"), hw_base);
		}
	} else {
		// Unknown cartridge HW.
		d->fields->addField_string(C_("SNES", "Cartridge HW"), C_("SNES", "Unknown"));
	}

 	// Region
	static const char *const region_tbl[0x15] = {
		NOP_C_("Region", "Japan"),
		NOP_C_("Region", "North America"),
		NOP_C_("Region", "Europe"),
		NOP_C_("Region", "Scandinavia"),
		nullptr, nullptr,
		NOP_C_("Region", "France"),
		NOP_C_("Region", "Netherlands"),
		NOP_C_("Region", "Spain"),
		NOP_C_("Region", "Germany"),
		NOP_C_("Region", "Italy"),
		NOP_C_("Region", "China"),
		nullptr,
		NOP_C_("Region", "South Korea"),
		NOP_C_("Region", "All"),
		NOP_C_("Region", "Canada"),
		NOP_C_("Region", "Brazil"),
		NOP_C_("Region", "Australia"),
		NOP_C_("Region", "Other"),
		NOP_C_("Region", "Other"),
		NOP_C_("Region", "Other"),
	};
	const char *const region = (romHeader->snes.destination_code < ARRAY_SIZE(region_tbl)
		? dpgettext_expr(RP_I18N_DOMAIN, "Region", region_tbl[romHeader->snes.destination_code])
		: nullptr);
	d->fields->addField_string(C_("SNES", "Region"),
		region ? region : C_("SNES", "Unknown"));

	// Revision
	d->fields->addField_string_numeric(C_("SNES", "Revision"),
		romHeader->snes.version, RomFields::FB_DEC, 2);

	// TODO: Other fields.

	// Finished reading the field data.
	return (int)d->fields->count();
}

}
