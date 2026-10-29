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

// dnkLauncher's files are written with stdio: the folder is on the SD card
// beside ScummVM's own, not one of ScummVM's paths
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "gui/mister-favourites.h"

#include "common/array.h"
#include "common/config-manager.h"
#include "common/language.h"
#include "common/platform.h"
#include "common/stream.h"
#include "common/util.h"
#include "graphics/surface.h"
#include "gui/gui-manager.h"
#include "gui/MetadataParser.h"
#include "gui/ThemeEngine.h"
#include "engines/metaengine.h"
#include "image/png.h"

#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GUI {

static const char *const kFavouriteKey = "dnk_favourite";

// The card: dnkLauncher's screenshot size, the icon in the middle
enum {
	kCardW = 256,
	kCardH = 192,
	kIconSize = 192
};

// dnkLauncher's ScummVM folder: games/dnk/dnkLauncher/scummvm beside the
// program's games/dnk/ScummVM, when dnkLauncher is there. DNK_SCUMMVM_DIR
// names another (tests on the PC).
static const Common::String &dnkDir() {
	static Common::String dir;
	static bool looked = false;

	if (looked)
		return dir;
	looked = true;
	const char *env = getenv("DNK_SCUMMVM_DIR");
	if (env && *env) {
		dir = env;
		return dir;
	}
	char path[PATH_MAX];
	const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (len <= 0)
		return dir;
	path[len] = '\0';
	// games/dnk/ScummVM/ScummVM -> games/dnk
	for (int i = 0; i < 2; i++) {
		char *slash = strrchr(path, '/');
		if (!slash)
			return dir;
		*slash = '\0';
	}
	const Common::String launcher = Common::String(path) + "/dnkLauncher";
	struct stat st;
	if (stat(launcher.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
		dir = launcher + "/scummvm";
	return dir;
}

bool favouritesAvailable() {
	return !dnkDir().empty();
}

bool isFavourite(const Common::String &target) {
	const Common::ConfigManager::Domain *domain = ConfMan.getDomain(target);
	Common::String value;
	return domain && domain->tryGetVal(kFavouriteKey, value) && value == "true";
}

// Before the list's titles: the theme's star (make_theme.py of ScummVM_MiSTer
// puts Cozette's at 0x80, a place Latin-1 leaves empty), '*' in other themes
Common::U32String favouritePrefix(const Common::String &target) {
	if (!favouritesAvailable())
		return Common::U32String();
	if (!isFavourite(target))
		return Common::U32String("  ");
	Common::U32String mark;
	if (g_gui.theme() && g_gui.theme()->getThemeId() == "misterhybrid")
		mark += (Common::u32char_type_t)0x80;
	else
		mark += '*';
	return mark + " ";
}

// A field of index.txt: no separator, no line break
static Common::String field(const Common::String &s) {
	Common::String out(s);
	for (uint i = 0; i < out.size(); i++)
		if (out[i] == '|' || out[i] == '\n' || out[i] == '\r')
			out.setChar(' ', i);
	return out;
}

// The game's folder from the root of the card (games/dnk/ScummVM/<game>/) or
// of a USB drive (usb0/...)
static Common::String shortPath(const Common::String &path) {
	if (path.hasPrefix("/media/fat/"))
		return path.substr(11);
	if (path.hasPrefix("/media/"))
		return path.substr(7);
	return path;
}

// What dnkLauncher shows of a game: the title, publisher, year and series of
// ScummVM's metadata (the icon pack's games.xml, companies.xml, series.xml),
// the engine, and the platform and language of this copy, as the launcher's
// artwork view has them
static Common::String indexLine(const Common::String &target, const Common::ConfigManager::Domain &domain,
                                const MetadataParser &metadata) {
	Common::String engineid, gameid;
	domain.tryGetVal("engineid", engineid);
	domain.tryGetVal("gameid", gameid);

	MetadataGame game;
	metadata._gameInfo.tryGetVal(engineid + ":" + gameid, game);   // as gui/launcher.cpp keys them
	MetadataCompany company;
	if (!game.company_id.empty())
		metadata._companyInfo.tryGetVal(game.company_id, company);
	MetadataSeries series;
	if (!game.series_id.empty())
		metadata._seriesInfo.tryGetVal(game.series_id, series);
	MetadataEngine engine;
	metadata._engineInfo.tryGetVal(engineid, engine);

	const Common::Platform platform = Common::parsePlatform(domain.getValOrDefault("platform"));
	const Common::Language language = Common::parseLanguage(domain.getValOrDefault("language"));

	Common::String line;
	line += field(target) + "|";
	line += field(game.name.empty() ? domain.getValOrDefault("description") : game.name) + "|";
	line += field(company.name) + "|";
	line += field(game.year) + "|";
	line += field(series.name) + "|";
	line += field(engine.name.empty() ? engineid : engine.name) + "|";
	line += field(platform != Common::kPlatformUnknown ? Common::getPlatformDescription(platform) : "") + "|";
	line += field(language != Common::UNK_LANG ? Common::getLanguageDescription(language) : "") + "|";
	line += field(shortPath(Common::Path::fromConfig(domain.getValOrDefault("path")).toString('/')));
	return line;
}

// The icon of a game (<engine>-<game>) or of its engine (<engine>) as 8-bit
// RGBA, the largest the port's pack has: icons143/ (made for the grid at 3 a
// row), else icons/ (tools/make_icons.py of ScummVM_MiSTer)
static Graphics::Surface *loadIcon(const Common::String &name) {
	static const char *const dirs[] = { "icons143/", "icons/" };
	Graphics::Surface *out = nullptr;

	g_gui.lockIconsSet();
	for (int i = 0; i < ARRAYSIZE(dirs) && !out; i++) {
		const Common::Path path(Common::String(dirs[i]) + name + ".png");
		if (!g_gui.getIconsSet().hasFile(path))
			continue;
		Common::SeekableReadStream *stream = g_gui.getIconsSet().createReadStreamForMember(path);
		Image::PNGDecoder decoder;
		if (stream && decoder.loadStream(*stream) && decoder.getSurface()) {
			const Graphics::Palette &pal = decoder.getPalette();
			out = decoder.getSurface()->convertTo(Graphics::PixelFormat::createFormatRGBA32(),
			                                      pal.empty() ? nullptr : pal.data(), pal.size());
		}
		delete stream;
	}
	g_gui.unlockIconsSet();
	return out;
}

// Lanczos (a = 3) weights for one axis, from n source pixels to m
struct Weights {
	Common::Array<int> first, count;
	Common::Array<float> w;
	int taps;
};

static float lanczos(float x) {
	if (x == 0)
		return 1;
	if (x <= -3 || x >= 3)
		return 0;
	const float px = (float)M_PI * x;
	return 3 * sinf(px) * sinf(px / 3) / (px * px);
}

static void weights(Weights &out, int n, int m) {
	const float scale = (float)n / m;
	const float support = 3 * MAX(scale, 1.0f);
	const float step = MAX(scale, 1.0f);
	out.taps = (int)ceilf(support * 2) + 1;
	out.first.resize(m);
	out.count.resize(m);
	out.w.resize(m * out.taps);
	for (int i = 0; i < m; i++) {
		const float centre = (i + 0.5f) * scale;
		const int lo = MAX((int)floorf(centre - support), 0);
		const int hi = MIN((int)ceilf(centre + support), n);
		float sum = 0;
		int k = 0;
		for (int j = lo; j < hi && k < out.taps; j++, k++) {
			const float v = lanczos((j + 0.5f - centre) / step);
			out.w[i * out.taps + k] = v;
			sum += v;
		}
		for (int j = 0; j < k; j++)
			out.w[i * out.taps + j] /= sum == 0 ? 1 : sum;
		out.first[i] = lo;
		out.count[i] = k;
	}
}

// The icon fitted into kIconSize x kIconSize in the middle of the card:
// Lanczos in linear light on premultiplied colours, so the transparent pixels
// around an icon give its edges no colour (dnklauncher_art.py's way)
static void scaleIcon(const Graphics::Surface &src, Common::Array<float> &card) {
	const int sw = src.w, sh = src.h;
	const float r = MIN((float)kIconSize / sw, (float)kIconSize / sh);
	const int dw = MAX((int)(sw * r + 0.5f), 1), dh = MAX((int)(sh * r + 0.5f), 1);
	const int ox = (kCardW - dw) / 2, oy = (kCardH - dh) / 2;

	float linear[256];
	for (int i = 0; i < 256; i++)
		linear[i] = powf(i / 255.0f, 2.2f);

	// premultiplied linear RGBA of the source
	Common::Array<float> in(sw * sh * 4);
	for (int y = 0; y < sh; y++) {
		for (int x = 0; x < sw; x++) {
			uint8 a, cr, cg, cb;
			src.format.colorToARGB(src.getPixel(x, y), a, cr, cg, cb);
			const float al = a / 255.0f;
			float *p = &in[(y * sw + x) * 4];
			p[0] = linear[cr] * al;
			p[1] = linear[cg] * al;
			p[2] = linear[cb] * al;
			p[3] = al;
		}
	}

	Weights wx, wy;
	weights(wx, sw, dw);
	weights(wy, sh, dh);
	// across, then down
	Common::Array<float> mid(dw * sh * 4);
	for (int y = 0; y < sh; y++) {
		for (int x = 0; x < dw; x++) {
			float acc[4] = { 0, 0, 0, 0 };
			for (int k = 0; k < wx.count[x]; k++) {
				const float w = wx.w[x * wx.taps + k];
				const float *p = &in[(y * sw + wx.first[x] + k) * 4];
				for (int c = 0; c < 4; c++)
					acc[c] += p[c] * w;
			}
			memcpy(&mid[(y * dw + x) * 4], acc, sizeof(acc));
		}
	}
	card.resize(kCardW * kCardH * 4);
	for (uint i = 0; i < card.size(); i++)
		card[i] = 0;
	for (int y = 0; y < dh; y++) {
		for (int x = 0; x < dw; x++) {
			float acc[4] = { 0, 0, 0, 0 };
			for (int k = 0; k < wy.count[y]; k++) {
				const float w = wy.w[y * wy.taps + k];
				const float *p = &mid[((wy.first[y] + k) * dw + x) * 4];
				for (int c = 0; c < 4; c++)
					acc[c] += p[c] * w;
			}
			float *o = &card[((oy + y) * kCardW + ox + x) * 4];
			const float a = CLIP(acc[3], 0.0f, 1.0f);
			for (int c = 0; c < 3; c++)
				o[c] = a > 1e-4f ? powf(CLIP(acc[c] / a, 0.0f, 1.0f), 1 / 2.2f) : 0;
			o[3] = a;
		}
	}
}

// dnkLauncher's picture with alpha (hybrid/tools/dnklauncher_art.py's
// write()): "DNKI", width, height, 2 (alpha), then the colours as RGB565
// dithered (its 4x4 Bayer matrix), then the alpha a byte a pixel
static bool writeCard(const Common::String &file, const Common::Array<float> &card) {
	static const int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
	const Common::String tmp = file + ".tmp";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (!f)
		return false;
	const byte head[12] = { 'D', 'N', 'K', 'I', kCardW & 0xFF, kCardW >> 8, kCardH & 0xFF, kCardH >> 8, 2, 0, 0, 0 };
	bool ok = fwrite(head, sizeof(head), 1, f) == 1;
	Common::Array<byte> row(kCardW * 2);
	for (int y = 0; y < kCardH && ok; y++) {
		for (int x = 0; x < kCardW; x++) {
			const float *p = &card[(y * kCardW + x) * 4];
			const float d = bayer[y & 3][x & 3] / 16.0f - 15 / 32.0f;
			const int r = CLIP((int)floorf(p[0] * 31 + 0.5f + d), 0, 31);
			const int g = CLIP((int)floorf(p[1] * 63 + 0.5f + d), 0, 63);
			const int b = CLIP((int)floorf(p[2] * 31 + 0.5f + d), 0, 31);
			const uint16 v = (r << 11) | (g << 5) | b;
			row[x * 2] = v & 0xFF;
			row[x * 2 + 1] = v >> 8;
		}
		ok = fwrite(row.data(), row.size(), 1, f) == 1;
	}
	for (int y = 0; y < kCardH && ok; y++) {
		for (int x = 0; x < kCardW; x++)
			row[x] = (byte)(card[(y * kCardW + x) * 4 + 3] * 255 + 0.5f);
		ok = fwrite(row.data(), kCardW, 1, f) == 1;
	}
	ok = fclose(f) == 0 && ok;
	if (!ok || rename(tmp.c_str(), file.c_str()) != 0) {
		unlink(tmp.c_str());
		return false;
	}
	return true;
}

static void makeCard(const Common::String &file, const Common::ConfigManager::Domain &domain) {
	Common::String engineid, gameid;
	domain.tryGetVal("engineid", engineid);
	domain.tryGetVal("gameid", gameid);
	Graphics::Surface *icon = loadIcon(engineid + "-" + gameid);
	if (!icon)
		icon = loadIcon(engineid);
	Common::Array<float> card;
	if (icon) {
		scaleIcon(*icon, card);
		icon->free();
		delete icon;
	} else {
		// no icon: a clear card, dnkLauncher's panel behind it
		card.resize(kCardW * kCardH * 4);
		for (uint i = 0; i < card.size(); i++)
			card[i] = 0;
	}
	if (!writeCard(file, card))
		warning("dnkLauncher: cannot write %s", file.c_str());
}

static Common::String readFile(const Common::String &file) {
	Common::String out;
	FILE *f = fopen(file.c_str(), "rb");
	if (!f)
		return out;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out += Common::String(buf, n);
	fclose(f);
	return out;
}

void syncFavourites(const MetadataParser &metadata) {
	const Common::String &dir = dnkDir();
	if (dir.empty())
		return;

	Common::Array<Common::String> targets;
	Common::String index("# The games marked as favourites in ScummVM, for dnkLauncher: "
	                     "target|title|publisher|year|series|engine|platform|language|path\n");
	const Common::ConfigManager::DomainMap &domains = ConfMan.getGameDomains();
	for (Common::ConfigManager::DomainMap::const_iterator i = domains.begin(); i != domains.end(); ++i) {
		if (isFavourite(i->_key))
			targets.push_back(i->_key);
	}
	Common::sort(targets.begin(), targets.end());
	for (uint i = 0; i < targets.size(); i++)
		index += indexLine(targets[i], *ConfMan.getDomain(targets[i]), metadata) + "\n";

	// written only when it changed: the SD card is mounted "sync"
	const Common::String indexFile = dir + "/index.txt";
	if (readFile(indexFile) != index) {
		mkdir(dir.c_str(), 0755);
		const Common::String tmp = indexFile + ".tmp";
		FILE *f = fopen(tmp.c_str(), "wb");
		bool ok = f && fwrite(index.c_str(), index.size(), 1, f) == 1;
		ok = f && fclose(f) == 0 && ok;
		if (!ok || rename(tmp.c_str(), indexFile.c_str()) != 0) {
			unlink(tmp.c_str());
			warning("dnkLauncher: cannot write %s", indexFile.c_str());
		}
	}

	for (uint i = 0; i < targets.size(); i++) {
		const Common::String card = dir + "/" + targets[i] + ".shot";
		if (access(card.c_str(), F_OK) != 0)
			makeCard(card, *ConfMan.getDomain(targets[i]));
	}

	// the cards of games no longer marked, or no longer in ScummVM
	DIR *d = opendir(dir.c_str());
	if (!d)
		return;
	while (struct dirent *e = readdir(d)) {
		Common::String name(e->d_name);
		if (!name.hasSuffix(".shot"))
			continue;
		const Common::String target = name.substr(0, name.size() - 5);
		if (Common::find(targets.begin(), targets.end(), target) == targets.end())
			unlink((dir + "/" + name).c_str());
	}
	closedir(d);
}

bool toggleFavourite(const Common::String &target, const MetadataParser &metadata) {
	Common::ConfigManager::Domain *domain = ConfMan.getDomain(target);
	if (!domain || !favouritesAvailable())
		return false;
	const bool on = !isFavourite(target);
	if (on)
		domain->setVal(kFavouriteKey, "true");
	else
		domain->erase(kFavouriteKey);
	ConfMan.flushToDisk();
	syncFavourites(metadata);
	return on;
}

} // End of namespace GUI
