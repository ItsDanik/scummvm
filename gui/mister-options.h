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

#ifndef GUI_MISTER_OPTIONS_H
#define GUI_MISTER_OPTIONS_H

#include "gui/dialog.h"
#include "common/array.h"
#include "common/str.h"
#include "common/ustr.h"

class MetaEngine;

namespace GUI {

class ButtonWidget;
class CheckboxWidget;
class EditTextWidget;
class LauncherDialog;
class OptionsContainerWidget;
class PathWidget;
class PopUpWidget;
class ScrollContainerWidget;
class StaticTextWidget;

/**
 * The MiSTer's options (Danik, 2026-10-28): one list in categories, no tabs,
 * only what a player of the MiSTer port should change. The rest is the core's
 * OSD (volumes, text and speech, the pointer, music) or stays as the port sets
 * it. Three of them: the launcher's global options, a game's options (the
 * launcher's Game Options), and the running game's (the Global Main Menu's
 * Options, the engine's own options alone).
 *
 * mister_full_options=true in scummvm.ini brings ScummVM's own dialogs back.
 */
class MisterOptionsDialog : public Dialog {
public:
	enum Mode {
		kModeGlobal,
		kModeGame,
		kModeInGame
	};

	/**
	 * @param domain      the game's domain (kModeGame, kModeInGame)
	 * @param metaEngine  kModeInGame: the running engine's, or null if it
	 *                    cannot change its options while it runs
	 * @param launcher    kModeGlobal: rebuilt when the GUI language changes
	 */
	MisterOptionsDialog(Mode mode, const Common::String &domain, MetaEngine *metaEngine = nullptr, LauncherDialog *launcher = nullptr);

	/** ScummVM's own options dialogs instead of these (mister_full_options). */
	static bool fullOptions();

	/** Whether there is anything in it (kModeInGame: the engine has options). */
	bool hasOptions() const { return _hasOptions; }

	const Common::String &getDomain() const { return _domain; }

	void open() override;
	void close() override;
	void reflowLayout() override;
	void handleCommand(CommandSender *sender, uint32 cmd, uint32 data) override;

private:
	enum RowType {
		kRowHeading,
		kRowCheckbox,
		kRowPopUp,     // label, then a popup or a text field
		kRowValue,     // label, then a text that cannot be changed
		kRowButton,    // a button in the labels' column, then a text
		kRowEngine     // the engine's options
	};

	struct Row {
		RowType type;
		Common::String name;
		Common::U32String label;
		StaticTextWidget *labelWidget;
		bool stacked;  // the label on a line of its own, above its box
	};

	void addHeading(const Common::U32String &text);
	CheckboxWidget *addCheckbox(const Common::String &name, const Common::U32String &label, const Common::U32String &tooltip = Common::U32String(), uint32 cmd = 0);
	PopUpWidget *addPopUp(const Common::String &name, const Common::U32String &label, const Common::U32String &tooltip = Common::U32String());
	StaticTextWidget *addLabel(RowType type, const Common::String &name, const Common::U32String &label);

	void buildGlobal();
	void buildGame();
	void buildInGame(MetaEngine *metaEngine);
	void addEngineOptions(MetaEngine &metaEngine);

	void defineLayouts();
	void apply();
	void applyGlobal();
	void applyGame();

	Mode _mode;
	Common::String _domain;
	LauncherDialog *_launcher;
	bool _hasOptions;

	StaticTextWidget *_title;
	ScrollContainerWidget *_list;
	Common::Array<Row> _rows;
	int _engineHeight;
	int _listWidth;
	OptionsContainerWidget *_engineOptions;

	// Global
	Common::String _newTheme;
	StaticTextWidget *_curTheme;
	PopUpWidget *_guiLanguagePopUp;
	CheckboxWidget *_guiUseGameLanguage;
	CheckboxWidget *_kineticScrolling;
	CheckboxWidget *_returnToLauncher;
	CheckboxWidget *_confirmExit;
	PopUpWidget *_autosavePopUp;
	CheckboxWidget *_enableGSCheckbox;

	// Game
	EditTextWidget *_description;
	PathWidget *_gamePath;
	PopUpWidget *_renderModePopUp;
	CheckboxWidget *_enableHotspots;
	StaticTextWidget *_hotspotMarkerDesc;
	PopUpWidget *_hotspotMarkerPopUp;
	CheckboxWidget *_showHotspotText;
	PopUpWidget *_enableGSPopUp;
};

} // End of namespace GUI

#endif
