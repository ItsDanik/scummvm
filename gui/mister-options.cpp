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

#include "gui/mister-options.h"

#include "base/plugins.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/gui_options.h"
#include "common/rendermode.h"
#include "common/translation.h"
#include "engines/metaengine.h"
#include "graphics/hotspot_renderer.h"
#include "gui/browser.h"
#include "gui/gui-manager.h"
#include "gui/launcher.h"
#include "gui/message.h"
#include "gui/options.h"
#include "gui/themebrowser.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"
#include "gui/widgets/edittext.h"
#include "gui/widgets/popup.h"
#include "gui/widgets/scrollcontainer.h"

namespace GUI {

enum {
	kChooseThemeCmd   = 'MOth',
	kGameBrowserCmd   = 'MOgp',
	kEnableHotspotCmd = 'MOhs'
};

// As in GlobalOptionsDialog
static const char *const savePeriodLabels[] = { _s("Never"), _s("Every 5 mins"), _s("Every 10 mins"), _s("Every 15 mins"), _s("Every 30 mins"), nullptr };
static const int savePeriodValues[] = { 0, 5 * 60, 10 * 60, 15 * 60, 30 * 60, -1 };

// The game's Roland GS line: its own setting or the global one
enum {
	kGSGlobal = 0,
	kGSOn = 1,
	kGSOff = 2
};

bool MisterOptionsDialog::fullOptions() {
	return ConfMan.hasKey("mister_full_options") && ConfMan.getBool("mister_full_options");
}

MisterOptionsDialog::MisterOptionsDialog(Mode mode, const Common::String &domain, MetaEngine *metaEngine, LauncherDialog *launcher)
	: Dialog("MisterOptions"), _mode(mode), _domain(domain), _launcher(launcher), _hasOptions(true),
	  _engineHeight(0), _listWidth(0), _engineOptions(nullptr),
	  _curTheme(nullptr), _guiLanguagePopUp(nullptr), _guiUseGameLanguage(nullptr), _kineticScrolling(nullptr),
	  _returnToLauncher(nullptr), _confirmExit(nullptr), _autosavePopUp(nullptr), _enableGSCheckbox(nullptr),
	  _description(nullptr), _gamePath(nullptr), _renderModePopUp(nullptr), _enableHotspots(nullptr),
	  _hotspotMarkerDesc(nullptr), _hotspotMarkerPopUp(nullptr), _showHotspotText(nullptr), _enableGSPopUp(nullptr) {

	if (_mode == kModeGlobal)
		_domain = Common::ConfigManager::kApplicationDomain;

	Common::U32String title;
	if (_mode == kModeGlobal) {
		title = Common::U32String("MiSTer ScummVM Options");
	} else {
		Common::String description = ConfMan.get("description", _domain);
		title = description.empty() ? Common::U32String(_domain) : Common::U32String(description);
	}
	_title = new StaticTextWidget(this, "MisterOptions.Title", title);
	_title->setAlign(Graphics::kTextAlignCenter);

	_list = new ScrollContainerWidget(this, "MisterOptions.List", "MisterOptions_List");
	_list->setBackgroundType(ThemeEngine::kWidgetBackgroundNo);
	_list->setTarget(this);

	if (_mode == kModeGlobal)
		buildGlobal();
	else if (_mode == kModeGame)
		buildGame();
	else
		buildInGame(metaEngine);

	new ButtonWidget(this, "MisterOptions.Cancel", _("~C~ancel"), Common::U32String(), kCloseCmd);
	new ButtonWidget(this, "MisterOptions.Ok", _("~O~K"), Common::U32String(), kOKCmd);
}

void MisterOptionsDialog::addHeading(const Common::U32String &text) {
	Row row;
	row.type = kRowHeading;
	row.name = Common::String::format("Heading%u", _rows.size());
	row.label = text;
	row.labelWidget = new StaticTextWidget(_list, "MisterOptions_List." + row.name, text);
	row.labelWidget->setFontColor(ThemeEngine::kFontColorOverride);
	row.stacked = false;
	_rows.push_back(row);
}

CheckboxWidget *MisterOptionsDialog::addCheckbox(const Common::String &name, const Common::U32String &label, const Common::U32String &tooltip, uint32 cmd) {
	Row row;
	row.type = kRowCheckbox;
	row.name = name;
	row.label = label;
	row.labelWidget = nullptr;
	row.stacked = false;
	_rows.push_back(row);
	return new CheckboxWidget(_list, "MisterOptions_List." + name, label, tooltip, cmd);
}

StaticTextWidget *MisterOptionsDialog::addLabel(RowType type, const Common::String &name, const Common::U32String &label) {
	Row row;
	row.type = type;
	row.name = name;
	row.label = label;
	row.labelWidget = new StaticTextWidget(_list, "MisterOptions_List." + name + "Desc", label);
	row.stacked = false;
	_rows.push_back(row);
	return row.labelWidget;
}

PopUpWidget *MisterOptionsDialog::addPopUp(const Common::String &name, const Common::U32String &label, const Common::U32String &tooltip) {
	addLabel(kRowPopUp, name, label);
	return new PopUpWidget(_list, "MisterOptions_List." + name, tooltip);
}

void MisterOptionsDialog::buildGlobal() {
	const bool lowres = g_gui.useLowResGUI();

	addHeading(_("Interface"));

	Row theme;
	theme.type = kRowButton;
	theme.name = "Theme";
	theme.label = _("Theme:");
	theme.labelWidget = nullptr;
	theme.stacked = false;
	_rows.push_back(theme);
	new ButtonWidget(_list, "MisterOptions_List.ThemeButton", theme.label, Common::U32String(), kChooseThemeCmd);
	_curTheme = new StaticTextWidget(_list, "MisterOptions_List.Theme", g_gui.theme()->getThemeName());

#ifdef USE_TRANSLATION
	_guiLanguagePopUp = addPopUp("GuiLanguage", _("GUI language:"), _("Language of ScummVM GUI"));
#ifdef USE_DETECTLANG
	_guiLanguagePopUp->appendEntry(_("<default>"), Common::kTranslationAutodetectId);
#endif
	_guiLanguagePopUp->appendEntry("English", Common::kTranslationBuiltinId);
	_guiLanguagePopUp->appendEntry("", 0);
	Common::TLangArray languages = TransMan.getSupportedLanguageNames();
	for (Common::TLangArray::iterator lang = languages.begin(); lang != languages.end(); ++lang)
		_guiLanguagePopUp->appendEntry(lang->name, lang->id);

	if (ConfMan.hasKey("gui_language") && !ConfMan.get("gui_language").empty())
		_guiLanguagePopUp->setSelectedTag(TransMan.parseLanguage(ConfMan.get("gui_language")));
	else
#ifdef USE_DETECTLANG
		_guiLanguagePopUp->setSelectedTag(Common::kTranslationAutodetectId);
#else
		_guiLanguagePopUp->setSelectedTag(Common::kTranslationBuiltinId);
#endif

	_guiUseGameLanguage = addCheckbox("GuiUseGameLanguage",
		lowres ? _("GUI in the game's language") : _("Switch the ScummVM GUI language to the game language"),
		_("When starting a game, change the ScummVM GUI language to the game language. "
		"That way, if a game uses the ScummVM save and load dialogs, they are "
		"in the same language as the game."));
	if (ConfMan.hasKey("gui_use_game_language"))
		_guiUseGameLanguage->setState(ConfMan.getBool("gui_use_game_language", _domain));
#endif

	_kineticScrolling = addCheckbox("KineticScrolling", _("Enable kinetic scrolling in lists"),
		_("Enable smooth, momentum-based scrolling in list widgets."));
	_kineticScrolling->setState(ConfMan.getBool("gui_kinetic_scrolling", _domain));

	addHeading(_("Leaving a game"));

	_returnToLauncher = addCheckbox("ReturnToLauncher",
		lowres ? _("Return to the launcher") : _("Return to the launcher when leaving a game"),
		_("Return to the launcher when leaving a game instead of closing ScummVM\n(this feature is not supported by all games)."));
	_returnToLauncher->setState(ConfMan.getBool("gui_return_to_launcher_at_exit", _domain));

	_confirmExit = addCheckbox("ConfirmExit", _("Ask for confirmation on exit"),
		_("Ask for permission when closing ScummVM or leaving a game."));
	_confirmExit->setState(ConfMan.getBool("confirm_exit", _domain));

	addHeading(_("Saving"));

	_autosavePopUp = addPopUp("Autosave", _("Autosave:"));
	const int period = ConfMan.getInt("autosave_period");
	int selected = 0;
	for (int i = 0; savePeriodLabels[i]; i++) {
		_autosavePopUp->appendEntry(_(savePeriodLabels[i]), savePeriodValues[i]);
		if (savePeriodValues[i] == period)
			selected = i;
	}
	_autosavePopUp->setSelected(selected);

	addHeading(_("Music"));

	_enableGSCheckbox = addCheckbox("EnableGS", _("Roland GS mappings (experimental)"),
		_("Maps MT-32 instruments to their Roland GS equivalents (for games with MT-32 music on a General MIDI synthesizer)"));
	_enableGSCheckbox->setState(ConfMan.getBool("enable_gs", _domain));
}

void MisterOptionsDialog::buildGame() {
	EngineMan.upgradeTargetIfNecessary(_domain);
	QualifiedGameDescriptor qgd = EngineMan.findTarget(_domain);
	const Plugin *enginePlugin = PluginMan.findEnginePlugin(qgd.engineId);

	const Common::String guioptions = parseGameGUIOptions(ConfMan.get("guioptions", _domain));

	addHeading(_("This game"));

	Common::String description(ConfMan.get("description", _domain));
	if (description.empty() && !qgd.description.empty())
		description = qgd.description;
	addLabel(kRowPopUp, "Desc", _("Name:"));
	_description = new EditTextWidget(_list, "MisterOptions_List.Desc", description, _("Full title of the game"));

	// The ID names its saved games and settings: shown, not changed
	addLabel(kRowValue, "Id", _("ID:"));
	StaticTextWidget *id = new StaticTextWidget(_list, "MisterOptions_List.Id", Common::U32String(_domain),
		_("Short game identifier used for referring to saved games and running the game from the command line"));
	id->setFontColor(ThemeEngine::kFontColorAlternate);

	Row path;
	path.type = kRowButton;
	path.name = "GamePath";
	path.label = _("Game Path:");
	path.labelWidget = nullptr;
	path.stacked = false;
	_rows.push_back(path);
	new ButtonWidget(_list, "MisterOptions_List.GamePathButton", path.label, Common::U32String(), kGameBrowserCmd);
	_gamePath = new PathWidget(_list, "MisterOptions_List.GamePath", ConfMan.getPath("path", _domain));

	if (enginePlugin) {
		MetaEngine &metaEngine = enginePlugin->get<MetaEngine>();
		metaEngine.registerDefaultSettings(_domain);
		addEngineOptions(metaEngine);
	} else {
		warning("Engine Plugin for target \"%s\" not found! Game specific settings might be missing.", _domain.c_str());
	}

	// Render mode, only for a game that has more than one
	if (guioptions.contains(GUIO_RENDER_PREFIX)) {
		Common::Array<Common::RenderMode> modes;
		for (const Common::RenderModeDescription *rm = Common::g_renderModes; rm->code; ++rm)
			if (guioptions.contains(Common::renderMode2GUIO(rm->id)))
				modes.push_back(rm->id);

		if (!modes.empty()) {
			addHeading(_("Picture"));
			_renderModePopUp = addPopUp("RenderMode", _("Render mode:"), _("Special dithering modes supported by some games"));
			_renderModePopUp->appendEntry(_("<default>"), Common::kRenderDefault);
			_renderModePopUp->appendEntry(Common::U32String());
			for (const Common::RenderModeDescription *rm = Common::g_renderModes; rm->code; ++rm)
				if (guioptions.contains(Common::renderMode2GUIO(rm->id)))
					_renderModePopUp->appendEntry(_(rm->description), rm->id);

			_renderModePopUp->setSelectedTag(Common::kRenderDefault);
			if (ConfMan.hasKey("render_mode", _domain))
				_renderModePopUp->setSelectedTag(Common::parseRenderMode(ConfMan.get("render_mode", _domain)));
		}
	}

	addHeading(_("Hotspots"));

	const bool hotspots = ConfMan.getBool("enable_hotspots", _domain);
	_enableHotspots = addCheckbox("EnableHotspots", _("Enable hotspot display"),
		_("Show interactive hotspots when the hotspot key is pressed"), kEnableHotspotCmd);
	_enableHotspots->setState(hotspots);

	_hotspotMarkerDesc = addLabel(kRowPopUp, "HotspotMarker", _("Hotspot marker:"));
	_hotspotMarkerPopUp = new PopUpWidget(_list, "MisterOptions_List.HotspotMarker");
	_hotspotMarkerPopUp->appendEntry(_("Point"), Graphics::kMarkerPoint);
	_hotspotMarkerPopUp->appendEntry(_("Square"), Graphics::kMarkerSquare);
	_hotspotMarkerPopUp->appendEntry(_("Crosshair"), Graphics::kMarkerCrosshair);
	_hotspotMarkerPopUp->setSelectedTag(ConfMan.getInt("hotspot_marker", _domain));

	_showHotspotText = addCheckbox("ShowHotspotText", _("Show hotspot labels"),
		_("Display text labels next to hotspot markers"));
	_showHotspotText->setState(ConfMan.hasKey("show_hotspot_text", _domain) ? ConfMan.getBool("show_hotspot_text", _domain) : true);

	_hotspotMarkerDesc->setEnabled(hotspots);
	_hotspotMarkerPopUp->setEnabled(hotspots);
	_showHotspotText->setEnabled(hotspots);

	// Roland GS, for games with MIDI music
	if (!guioptions.contains(GUIO_NOMIDI) && !guioptions.contains(GUIO_NOMUSIC) &&
		(guioptions.contains(GUIO_MIDIMT32) || guioptions.contains(GUIO_MIDIGM))) {
		addHeading(_("Music"));
		_enableGSPopUp = addPopUp("EnableGS", _("Roland GS mappings (experimental)"),
			_("Maps MT-32 instruments to their Roland GS equivalents (for games with MT-32 music on a General MIDI synthesizer)"));
		_enableGSPopUp->appendEntry(_("As in Global Options"), kGSGlobal);
		_enableGSPopUp->appendEntry(_("On"), kGSOn);
		_enableGSPopUp->appendEntry(_("Off"), kGSOff);
		if (ConfMan.hasKey("enable_gs", _domain))
			_enableGSPopUp->setSelectedTag(ConfMan.getBool("enable_gs", _domain) ? kGSOn : kGSOff);
		else
			_enableGSPopUp->setSelectedTag(kGSGlobal);
	}
}

void MisterOptionsDialog::buildInGame(MetaEngine *metaEngine) {
	if (metaEngine)
		addEngineOptions(*metaEngine);

	if (!_engineOptions) {
		_hasOptions = false;
		addHeading(_("This game has no options to change while it runs."));
		_rows.back().labelWidget->setFontColor(ThemeEngine::kFontColorNormal);
	}
}

void MisterOptionsDialog::addEngineOptions(MetaEngine &metaEngine) {
	_engineOptions = metaEngine.buildEngineOptionsWidget(_list, "MisterOptions_List.Engine", _domain);
	if (!_engineOptions)
		return;
	_engineOptions->setParentDialog(this);

	// The in-game window is the engine's options alone, without a heading
	if (_mode != kModeInGame)
		addHeading(_("Game options"));

	Row row;
	row.type = kRowEngine;
	row.name = "Engine";
	row.labelWidget = nullptr;
	row.stacked = false;
	_rows.push_back(row);
}

void MisterOptionsDialog::defineLayouts() {
	ThemeEval &layouts = *g_gui.xmlEval();
	const bool lowres = g_gui.useLowResGUI();
	const int pad = lowres ? 8 : 16;
	const int gap = lowres ? 6 : 10;
	const int lineHeight = layouts.getVar("Globals.Line.Height", 16);
	const int buttonHeight = layouts.getVar("Globals.Button.Height", lineHeight);
	const int buttonPad = lowres ? 8 : 16;

	// The labels' column: as wide as the widest label, at most 45% of the
	// list; a longer label goes on a line of its own above its box
	const int maxLabel = _listWidth * 45 / 100;
	int labelWidth = 0;
	for (uint i = 0; i < _rows.size(); i++) {
		Row &row = _rows[i];
		int w;
		if (row.type == kRowPopUp || row.type == kRowValue)
			w = g_gui.getStringWidth(row.label);
		else if (row.type == kRowButton)
			w = g_gui.getStringWidth(row.label) + buttonPad;
		else
			continue;
		row.stacked = row.type != kRowButton && w > maxLabel;
		if (!row.stacked)
			labelWidth = MAX(labelWidth, w);
	}
	labelWidth = MIN(MAX(labelWidth, layouts.getVar("Globals.Button.Width", 0)), MAX(maxLabel, 1));

	// Where ScummVM's options are: the screen, 16 or 32 pixels in
	layouts.addDialog("MisterOptions", "screen", -1, -1, lowres ? 16 : 32);
	layouts.setVar("Dialog.MisterOptions.Shading", layouts.getVar("Dialog.GlobalOptions.Shading", 0));
	layouts.addLayout(ThemeLayout::kLayoutVertical, pad / 2).addPadding(pad, pad, pad, 0);
	layouts.addWidget("Title", "", -1, lineHeight);
	layouts.addWidget("List", "ScrollContainerWidget", -1, -1);
	layouts.addLayout(ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, pad);
	layouts.addSpace();
	layouts.addWidget("Cancel", "Button");
	layouts.addWidget("Ok", "Button");
	layouts.closeLayout();
	layouts.closeLayout().closeDialog();

	layouts.addDialog("MisterOptions_List", "MisterOptions.List");
	layouts.addLayout(ThemeLayout::kLayoutVertical, lowres ? 4 : 6).addPadding(0, gap / 2, 2, 2);
	for (uint i = 0; i < _rows.size(); i++) {
		const Row &row = _rows[i];
		switch (row.type) {
		case kRowHeading:
			if (i > 0)
				layouts.addSpace(lowres ? 4 : 8);
			layouts.addWidget(row.name, "", -1, lineHeight);
			break;
		case kRowCheckbox:
			layouts.addWidget(row.name, "Checkbox");
			break;
		case kRowPopUp:
		case kRowValue:
			if (row.stacked)
				layouts.addWidget(row.name + "Desc", "", -1, lineHeight);
			layouts.addLayout(ThemeLayout::kLayoutHorizontal, gap, ThemeLayout::kItemAlignCenter).addPadding(0, 0, 0, 0);
			if (row.stacked)
				layouts.addSpace(labelWidth);
			else
				layouts.addWidget(row.name + "Desc", "", labelWidth, lineHeight);
			if (row.type == kRowPopUp)
				layouts.addWidget(row.name, "PopUp");
			else
				layouts.addWidget(row.name, "", -1, lineHeight);
			layouts.closeLayout();
			break;
		case kRowButton:
			layouts.addLayout(ThemeLayout::kLayoutHorizontal, gap, ThemeLayout::kItemAlignCenter).addPadding(0, 0, 0, 0);
			layouts.addWidget(row.name + "Button", "", labelWidth, buttonHeight);
			layouts.addWidget(row.name, "", -1, lineHeight);
			layouts.closeLayout();
			break;
		case kRowEngine:
			layouts.addWidget(row.name, "", -1, _engineHeight);
			break;
		}
	}
	layouts.closeLayout().closeDialog();
}

void MisterOptionsDialog::reflowLayout() {
	// The layouts are made here, again whenever the theme is loaded (the
	// launcher's size switched, another theme or GUI language). The labels'
	// column needs the list's width and the rows under the engine's options
	// their height, both known after a reflow: a second one puts them right.
	for (int pass = 0; pass < 3; pass++) {
		defineLayouts();
		Dialog::reflowLayout();

		const int listWidth = _list->getWidth();
		const int engineHeight = _engineOptions ? _engineOptions->getHeight() : 0;
		if (listWidth == _listWidth && engineHeight == _engineHeight)
			break;
		_listWidth = listWidth;
		_engineHeight = engineHeight;
	}

	for (uint i = 0; i < _rows.size(); i++) {
		const Row &row = _rows[i];
		if (row.labelWidget && row.type != kRowHeading)
			row.labelWidget->setAlign(row.stacked ? Graphics::kTextAlignStart : Graphics::kTextAlignEnd);
	}
}

void MisterOptionsDialog::open() {
	Dialog::open();
	setResult(0);

	if (_engineOptions)
		_engineOptions->load();
}

void MisterOptionsDialog::close() {
	if (getResult() > 0)
		apply();

	Dialog::close();
}

void MisterOptionsDialog::handleCommand(CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	case kOKCmd:
		setResult(1);
		close();
		break;

	case kChooseThemeCmd: {
		ThemeBrowser browser;
		if (browser.runModal() > 0) {
			_newTheme = browser.getSelected();
			_curTheme->setLabel(browser.getSelectedName());
		}
		break;
	}

	case kGameBrowserCmd: {
		BrowserDialog browser(_("Select directory with game data"), true);
		if (browser.runModal() > 0) {
			Common::FSNode dir(browser.getResult());
			_gamePath->setLabel(dir.getPath());
		}
		g_gui.scheduleTopDialogRedraw();
		break;
	}

	case kEnableHotspotCmd: {
		// also sent by its setState() in buildGame(), before the others are there
		if (!_showHotspotText)
			break;
		const bool enabled = _enableHotspots->getState();
		_hotspotMarkerDesc->setEnabled(enabled);
		_hotspotMarkerPopUp->setEnabled(enabled);
		_showHotspotText->setEnabled(enabled);
		g_gui.scheduleTopDialogRedraw();
		break;
	}

	default:
		Dialog::handleCommand(sender, cmd, data);
	}
}

void MisterOptionsDialog::apply() {
	if (_mode == kModeGlobal)
		applyGlobal();
	else if (_mode == kModeGame)
		applyGame();
	else if (_engineOptions)
		_engineOptions->save();

	ConfMan.flushToDisk();
}

void MisterOptionsDialog::applyGlobal() {
	bool reloadTheme = false;
	bool rebuild = false;

	const Common::String oldThemeId = g_gui.theme()->getThemeId();
	const Common::String oldThemeName = g_gui.theme()->getThemeName();
	if (!_newTheme.empty() && _newTheme != oldThemeId) {
		ConfMan.set("gui_theme", _newTheme, _domain);
		reloadTheme = true;
	}

#ifdef USE_TRANSLATION
	const Common::String oldLang = ConfMan.get("gui_language");
	const Common::String newLang = TransMan.getLangById(_guiLanguagePopUp->getSelectedTag());
	if (newLang != oldLang) {
		TransMan.setLanguage(newLang);
		ConfMan.set("gui_language", newLang, _domain);
		reloadTheme = true;
		rebuild = true;
	}
	const bool wantsBuiltinLang = TransMan.currentIsBuiltinLanguage();

	ConfMan.setBool("gui_use_game_language", _guiUseGameLanguage->getState(), _domain);
#endif

	ConfMan.setBool("gui_kinetic_scrolling", _kineticScrolling->getState(), _domain);
	ConfMan.setBool("gui_return_to_launcher_at_exit", _returnToLauncher->getState(), _domain);
	ConfMan.setBool("confirm_exit", _confirmExit->getState(), _domain);

	const int autosavePeriod = _autosavePopUp->getSelectedTag();
	if (GlobalOptionsDialog::updateAutosavePeriod(autosavePeriod))
		ConfMan.setInt("autosave_period", autosavePeriod, _domain);
	else
		ConfMan.setInt("autosave_period", 0, _domain);

	ConfMan.setBool("enable_gs", _enableGSCheckbox->getState(), _domain);

	if (reloadTheme) {
		const Common::String themeId = _newTheme.empty() ? oldThemeId : _newTheme;
		const ThemeEngine::GraphicsMode gfxMode = (ThemeEngine::GraphicsMode)g_gui.theme()->getGraphicsMode();
		if (!g_gui.loadNewTheme(themeId, gfxMode, true)) {
			// As GlobalOptionsDialog: back to the theme and language before
			Common::U32String errorMessage = _("Theme cannot be loaded!");
			ConfMan.set("gui_theme", oldThemeId, _domain);
			_curTheme->setLabel(oldThemeName);
#ifdef USE_TRANSLATION
			if (!wantsBuiltinLang && TransMan.currentIsBuiltinLanguage())
				errorMessage = _("Theme does not support selected language!");
			TransMan.setLanguage(oldLang);
			ConfMan.set("gui_language", oldLang, _domain);
#endif
			g_gui.loadNewTheme(oldThemeId, gfxMode, true);
			MessageDialog error(errorMessage);
			error.runModal();
		}
	}
	_newTheme.clear();

	if (rebuild) {
		g_gui.setLanguageRTL();
		if (_launcher)
			_launcher->rebuild();
	}
}

void MisterOptionsDialog::applyGame() {
	ConfMan.set("description", _description->getEditString(), _domain);

	Common::Path gamePath(_gamePath->getLabel());
	if (!gamePath.empty())
		ConfMan.setPath("path", gamePath, _domain);

	if (_engineOptions)
		_engineOptions->save();

	if (_renderModePopUp) {
		const int mode = (int32)_renderModePopUp->getSelectedTag();
		if (mode <= 0)
			ConfMan.removeKey("render_mode", _domain);
		else
			ConfMan.set("render_mode", Common::getRenderModeCode((Common::RenderMode)mode), _domain);
	}

	ConfMan.setBool("enable_hotspots", _enableHotspots->getState(), _domain);
	ConfMan.setInt("hotspot_marker", _hotspotMarkerPopUp->getSelectedTag(), _domain);
	ConfMan.setBool("show_hotspot_text", _showHotspotText->getState(), _domain);

	if (_enableGSPopUp) {
		switch (_enableGSPopUp->getSelectedTag()) {
		case kGSOn:
			ConfMan.setBool("enable_gs", true, _domain);
			break;
		case kGSOff:
			ConfMan.setBool("enable_gs", false, _domain);
			break;
		default:
			ConfMan.removeKey("enable_gs", _domain);
			break;
		}
	}
}

} // End of namespace GUI
