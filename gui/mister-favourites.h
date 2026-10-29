/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GUI_MISTER_FAVOURITES_H
#define GUI_MISTER_FAVOURITES_H

#include "common/str.h"
#include "common/ustr.h"

namespace GUI {

class MetadataParser;

/**
 * The games marked as favourites for dnkLauncher, the hybrid ports' frontend
 * (hybrid/docs/dnklauncher.md of ScummVM_MiSTer, "Systems"): its ScummVM
 * System lists them. A favourite is dnk_favourite=true in the game's section
 * of scummvm.ini; for each, ScummVM writes into dnkLauncher's
 * games/dnk/dnkLauncher/scummvm/ its card (<target>.shot: the game's icon at
 * 192x192 in the middle of 256x192, clear around it, as dnkLauncher's
 * pictures are kept) and its line of index.txt.
 *
 * Only while dnkLauncher is installed (its folder beside ScummVM's): without
 * it there is nothing to mark, no star and no files.
 */
bool favouritesAvailable();
bool isFavourite(const Common::String &target);
// Marks the game or takes the mark away, and writes dnkLauncher's files
bool toggleFavourite(const Common::String &target, const MetadataParser &metadata);
// dnkLauncher's files as the settings say: index.txt written again when it
// changed, the cards missing made, those of games no longer marked removed
void syncFavourites(const MetadataParser &metadata);
// What goes before every title of the launcher's list while dnkLauncher is
// installed, two characters: a star and a space for a favourite, two spaces
// for the others (the list's quick select skips them)
Common::U32String favouritePrefix(const Common::String &target);

} // End of namespace GUI

#endif
