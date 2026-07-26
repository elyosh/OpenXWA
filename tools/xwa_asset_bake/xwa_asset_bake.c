/*
 * xwa_asset_bake — offline remaster-asset baker.
 *
 * Reads source art through Aeron VFS, decodes it with the shared independent
 * XWA format library, and emits modern assets through imgbake.
 * No original renderer, no framebuffer scrape — a standalone host CLI
 * It does not link or invoke recovered game loaders.
 *
 * PNG output is straight-alpha source art for artist remastering;
 * KTX2 (BC7, sRGB, PMA, mipped) is the only format the game loads.
 */

#include "bake_fonts.h"
#include "bake_flight_fonts.h"
#include "bake_sprites.h"

#include "aeron/vfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char* argv0) {
	fprintf(stderr,
			"usage: %s [--assets <dir>] <sprites|fonts|flight-fonts> <out-root> [options]\n"
			"\n"
			"  --assets <dir>   game data root (default: game_data)\n"
			"\n"
			"fonts: bake TIMES<size>.ABP frontend fonts to HD atlas pairs\n"
			"  <out-root>/fonts/font<size>.{png,fnt} (TFNT v2, RM_SCALE=4x).\n"
			"  --ttf <file>   rasterize this face (classic = GDI Times New\n"
			"                 Roman) at matched HD metrics as the runtime\n"
			"                 atlas; the classic NN atlas then lands under\n"
			"                 fonts/reference/ for font_tune comparison.\n"
			"  Hand-tune / replace with TIE's font_tune GUI (same format).\n"
			"\n"
			"flight-fonts: bake the classic hardware HUD font group to\n"
			"  <out-root>/flight/fonts/font_tier_{0,1,2}.{ktx2,fnt}.\n"
			"\n"
			"sprites options:\n"
			"  --png            write straight-alpha PNGs (artist source)\n"
			"  --ktx2           write BC7 KTX2 (game input)\n"
			"                   (neither flag = both formats)\n"
			"  --quality <q>    BC7 quality: fast | med | uber (default: med)\n"
			"  --no-zstd        disable KTX2 zstd supercompression\n"
			"  --no-scale       skip the SVGA->4K upscale (classic 1:1 out)\n"
			"  --list <substr>  only bake frontres files whose dir/name\n"
			"                   contains <substr>; skips the DAT pass\n"
			"  --group <id>     only bake DAT sprite group <id>\n"
			"  --dat-only       skip the frontres pass (resdata only)\n",
			argv0);
	return 2;
}

int main(int argc, char** argv) {
	const char* asset_root = "game_data";

	BakeSpriteOptions opt;
	memset(&opt, 0, sizeof opt);
	opt.zstd         = true;
	opt.scale        = true;
	opt.bc7_quality  = KTX2_BC7_QUALITY_MED;
	opt.group_filter = -1;

	const char* command      = NULL;
	const char* ttf_path     = NULL;
	int         explicit_fmt = 0;

	for (int i = 1; i < argc; i++) {
		const char* a = argv[i];
		if (strcmp(a, "--assets") == 0 && i + 1 < argc) {
			asset_root = argv[++i];
		} else if (strcmp(a, "--png") == 0) {
			opt.write_png = true;
			explicit_fmt  = 1;
		} else if (strcmp(a, "--ktx2") == 0) {
			opt.write_ktx2 = true;
			explicit_fmt   = 1;
		} else if (strcmp(a, "--quality") == 0 && i + 1 < argc) {
			const char* q = argv[++i];
			if (strcmp(q, "fast") == 0) {
				opt.bc7_quality = KTX2_BC7_QUALITY_FAST;
			} else if (strcmp(q, "med") == 0) {
				opt.bc7_quality = KTX2_BC7_QUALITY_MED;
			} else if (strcmp(q, "uber") == 0) {
				opt.bc7_quality = KTX2_BC7_QUALITY_UBER;
			} else {
				return usage(argv[0]);
			}
		} else if (strcmp(a, "--no-zstd") == 0) {
			opt.zstd = false;
		} else if (strcmp(a, "--no-scale") == 0) {
			opt.scale = false;
		} else if (strcmp(a, "--list") == 0 && i + 1 < argc) {
			opt.list_filter = argv[++i];
		} else if (strcmp(a, "--group") == 0 && i + 1 < argc) {
			opt.group_filter = atoi(argv[++i]);
		} else if (strcmp(a, "--dat-only") == 0) {
			opt.dat_only = true;
		} else if (strcmp(a, "--ttf") == 0 && i + 1 < argc) {
			ttf_path = argv[++i];
		} else if (a[0] == '-') {
			fprintf(stderr, "unknown option: %s\n", a);
			return usage(argv[0]);
		} else if (!command) {
			command = a;
		} else if (!opt.out_root) {
			opt.out_root = a;
		} else {
			return usage(argv[0]);
		}
	}
	const int cmd_sprites = command && strcmp(command, "sprites") == 0;
	const int cmd_fonts   = command && strcmp(command, "fonts") == 0;
	const int cmd_flight_fonts = command && strcmp(command, "flight-fonts") == 0;
	if ((!cmd_sprites && !cmd_fonts && !cmd_flight_fonts) || !opt.out_root) {
		return usage(argv[0]);
	}
	if (!explicit_fmt) {
		opt.write_png  = true;
		opt.write_ktx2 = true;
	}

	AeronVfsConfig cfg;
	memset(&cfg, 0, sizeof cfg);
	cfg.org_name   = "xwa";
	cfg.app_name   = "xwa_asset_bake";
	cfg.asset_root = asset_root;
	AeronVfs* vfs  = AeronVfs_Create(&cfg);
	if (!vfs) {
		fprintf(stderr, "xwa_asset_bake: VFS create failed (asset_root=%s)\n", asset_root);
		return 1;
	}
	if (!AeronVfs_SetRootOptions(vfs, AERON_VFS_ROOT_ASSET,
								 AERON_VFS_ROOT_OPTION_CASE_INSENSITIVE_READ_LOOKUP)) {
		fprintf(stderr, "xwa_asset_bake: VFS asset lookup configuration failed\n");
		AeronVfs_Destroy(vfs);
		return 1;
	}
	if (cmd_fonts) {
		BakeFontsOptions fopt = { .out_root = opt.out_root, .scale = opt.scale ? 4 : 1,
								  .ttf_path = ttf_path };
		const int        baked = BakeFonts_Run(&fopt, vfs);
		printf("bake done: %d fonts\n", baked);
		AeronVfs_Destroy(vfs);
		return baked > 0 ? 0 : 1;
	}

	if (cmd_flight_fonts) {
		BakeFlightFontsOptions fopt = {
			.out_root = opt.out_root, .scale = opt.scale ? 4 : 1,
			.zstd = opt.zstd, .bc7_quality = opt.bc7_quality,
		};
		const int baked = BakeFlightFonts_Run(&fopt, vfs);
		printf("bake done: %d flight font tiers\n", baked);
		AeronVfs_Destroy(vfs);
		return baked == 3 ? 0 : 1;
	}

	BakeStats stats;
	int       ok = BakeSprites_Run(&opt, vfs, &stats);
	printf("bake done: %d sprites, %d atlases, %d frame dirs, %d skipped, %d failures\n",
		   stats.sprites, stats.atlases, stats.frame_dirs, stats.skipped, stats.failures);
	AeronVfs_Destroy(vfs);
	return ok ? 0 : 1;
}
