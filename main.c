#include <libk/string.h>
#include <libk/stdio.h>
#include <libk/ctype.h>

#include <vitasdk.h>
#include <taihen.h>

#include "display.h"
#include "config.h"
#include "tools.h"

#define MAX_INJECTS_NUM 5

// sceDisplaySetFrameBuf hook
static SceUID sceDisplaySetFrameBuf_hookid;
static tai_hook_ref_t sceDisplaySetFrameBuf_hookref = {0};

// eboot patches
static uint8_t injects_num = 0;
static SceUID injects[MAX_INJECTS_NUM] = {0};

static char titleid[16];
static tai_module_info_t info = {0};

static uint8_t supported_game = 0;
static uint8_t timer = 150;

static VG_Config config;

void injectData(SceUID modid, int segidx, uint32_t offset, const void *data, size_t size) {
	injects[injects_num] = taiInjectData(modid, segidx, offset, data, size);
	injects_num++;
}

int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {

	updateFramebuf(pParam);

	if (timer > 0) {
		int y = 5;
		drawStringF(5, y, "%s: %s", titleid,
					config.game_enabled  == FEATURE_ENABLED ? "patched" : "disabled");

		if (config.game_enabled == FEATURE_ENABLED) {
			if (config.fb_res_enabled == FEATURE_ENABLED) {
				y += 20;
				drawStringF(5, y, "Framebuffer: %dx%d", config.fb_width, config.fb_height);
			}
			if (config.ib_res_enabled == FEATURE_ENABLED) {
				y += 20;
				drawStringF(5, y, "Internal: %dx%d", config.ib_width, config.ib_height);
			}
			if (config.fps_enabled == FEATURE_ENABLED) {
				y += 20;
				drawStringF(5, y, "FPS cap: %d", config.fps == FPS_30 ? 30 : 60);
			}
		}

		timer--;
	} else {
		// Release no longer used hook
		taiHookRelease(sceDisplaySetFrameBuf_hookid, sceDisplaySetFrameBuf_hookref);
	}

	return TAI_CONTINUE(int, sceDisplaySetFrameBuf_hookref, pParam, sync);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {

	// Getting eboot.bin info
	info.size = sizeof(tai_module_info_t);
	taiGetModuleInfo(TAI_MAIN_MODULE, &info);

	// Getting app titleid
	sceAppMgrAppParamGetString(0, 12, titleid, 256);

	// Parse config
	config = config_parse(titleid);
	if (config.enabled == FEATURE_DISABLED) {
		return SCE_KERNEL_START_SUCCESS;
	}

	if (strncmp(titleid, "PCSF00243", 9) == 0 || // Killzone Mercenary [EUR] [1.12]
			strncmp(titleid, "PCSF00403", 9) == 0 || // Killzone Mercenary [EUR] [1.12]
			strncmp(titleid, "PCSA00107", 9) == 0 || // Killzone Mercenary [USA] [1.12]
			strncmp(titleid, "PCSC00045", 9) == 0 || // Killzone Mercenary [JPN] [1.12]
			strncmp(titleid, "PCSD00071", 9) == 0) { // Killzone Mercenary [ASA] [1.12]
		config_set_unsupported(FEATURE_UNSUPPORTED, FEATURE_ENABLED, FEATURE_ENABLED, &config);
		config_set_default(FEATURE_DISABLED, FEATURE_DISABLED, FEATURE_DISABLED, &config);
		supported_game = 1;

		if (config.game_enabled == FEATURE_ENABLED && config.ib_res_enabled == FEATURE_ENABLED) {
			uint32_t data32_w_h_w_h[4] = {
				config.ib_width, config.ib_height, config.ib_width, config.ib_height
			};
			uint8_t nop_nop_nop_nop[8] = {
				0x00, 0xBF, 0x00, 0xBF, 0x00, 0xBF, 0x00, 0xBF
			};

			// seg000:8115A5C8  BL  sub_8104DFC6
			// seg000:8115A5CC  BL  sub_8104F55E
			injectData(info.modid, 0, 0x15A5C8, &nop_nop_nop_nop, sizeof(nop_nop_nop_nop));

			// dword_81AA1728  DCD 0x3C0  ; DATA XREF: sub_8104F55E
			// dword_81AA172C  DCD 0x220  ; DATA XREF: sub_8104F55E
			// dword_81AA1730  DCD 0x3C0  ; DATA XREF: sub_8104DFC6
			// dword_81AA1734  DCD 0x220  ; DATA XREF: sub_8104DFC6
			injectData(info.modid, 1, 0xD728, &data32_w_h_w_h, sizeof(data32_w_h_w_h));
		}
		if (config.game_enabled == FEATURE_ENABLED && config.fps_enabled == FEATURE_ENABLED) {
			uint32_t data32_vblank = config.fps == FPS_60 ? 0x1 : 0x2;

			// dword_819706A4  DCD 2  ; DATA XREF: seg000:8104F722
			injectData(info.modid, 0, 0x9706A4, &data32_vblank, sizeof(data32_vblank));
		}
	}
	else if (strncmp(titleid, "PCSB00245", 9) == 0 || // Persona 4 Golden [EUR]
			 strncmp(titleid, "PCSE00120", 9) == 0 || // Persona 4 Golden [USA]
			 strncmp(titleid, "PCSG00004", 9) == 0 || // Persona 4 Golden [JPN] [1.01]
			 strncmp(titleid, "PCSG00563", 9) == 0 || // Persona 4 Golden [JPN]
			 strncmp(titleid, "PCSH00021", 9) == 0) { // Persona 4 Golden [ASA]
		config_set_unsupported(FEATURE_UNSUPPORTED, FEATURE_ENABLED, FEATURE_UNSUPPORTED, &config);
		config_set_default(FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_DISABLED, &config);
		supported_game = 1;

		if (config.game_enabled == FEATURE_ENABLED && config.ib_res_enabled == FEATURE_ENABLED) {
			float float_w_h[2] = {
				(float)config.ib_width, (float)config.ib_height
			};
			uint32_t offset_w_h;

			if (strncmp(titleid, "PCSB00245", 9) == 0) {
				offset_w_h = 0xDBCFC;
			} else if (strncmp(titleid, "PCSE00120", 9) == 0) {
				offset_w_h = 0xDBCEC;
			} else if (strncmp(titleid, "PCSG00004", 9) == 0 ||
					strncmp(titleid, "PCSG00563", 9) == 0) {
				offset_w_h = 0xDBD9C;
			} else if (strncmp(titleid, "PCSH00021", 9) == 0) {
				offset_w_h = 0xF1C50;
			}

			injectData(info.modid, 1, offset_w_h, &float_w_h, sizeof(float_w_h));
		}
	}
	else if (strncmp(titleid, "PCSB00204", 9) == 0) { // WRC 3: FIA World Rally Championship [EUR] [1.01]
		config_set_unsupported(FEATURE_UNSUPPORTED, FEATURE_ENABLED, FEATURE_UNSUPPORTED, &config);
		config_set_default(FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_DISABLED, &config);
		supported_game = 1;

		if (config.game_enabled == FEATURE_ENABLED && config.ib_res_enabled == FEATURE_ENABLED) {
			uint8_t movs_r5_width[4], movs_r6_height[4];
			make_thumb2_t2_mov(5, 1, config.ib_width, movs_r5_width);
			make_thumb2_t2_mov(6, 1, config.ib_height, movs_r6_height);

			injectData(info.modid, 0, 0xAC430A, &movs_r5_width, sizeof(movs_r5_width));
			injectData(info.modid, 0, 0xAC4310, &movs_r6_height, sizeof(movs_r6_height));
		}
	}
	else if (strncmp(titleid, "PCSB00345", 9) == 0 || // WRC 4: FIA World Rally Championship [EUR] [1.01]
			strncmp(titleid, "PCSE00411", 9) == 0) { // WRC 4: FIA World Rally Championship [USA]
		config_set_unsupported(FEATURE_UNSUPPORTED, FEATURE_ENABLED, FEATURE_UNSUPPORTED, &config);
		config_set_default(FEATURE_DISABLED, FEATURE_ENABLED, FEATURE_DISABLED, &config);
		supported_game = 1;

		if (config.game_enabled == FEATURE_ENABLED && config.ib_res_enabled == FEATURE_ENABLED) {
			uint8_t movs_r0_width[4], movs_r4_height[4];
			uint32_t offset_w = 0, offset_h = 0;
			make_thumb2_t2_mov(0, 1, config.ib_width, movs_r0_width);
			make_thumb2_t2_mov(4, 1, config.ib_height, movs_r4_height);

			if (strncmp(titleid, "PCSB00345", 9) == 0) {
				offset_w = 0xAC297C; offset_h = 0xAC2982;
			} else if (strncmp(titleid, "PCSE00411", 9) == 0) {
				offset_w = 0xAC46C4; offset_h = 0xAC46CA;
			}

			injectData(info.modid, 0, offset_w, &movs_r0_width, sizeof(movs_r0_width));
			injectData(info.modid, 0, offset_h, &movs_r4_height, sizeof(movs_r4_height));
		}
	}

	// If no features are enabled, mark game as disabled
	if (supported_game &&
			config.fb_res_enabled != FEATURE_ENABLED &&
			config.ib_res_enabled != FEATURE_ENABLED &&
			config.fps_enabled != FEATURE_ENABLED) {
		config.game_enabled = FEATURE_DISABLED;
	}

	if (supported_game &&
			config.osd_enabled == FEATURE_ENABLED &&
			config.game_osd_enabled == FEATURE_ENABLED) {
		sceDisplaySetFrameBuf_hookid = taiHookFunctionImport(
				&sceDisplaySetFrameBuf_hookref,
				TAI_MAIN_MODULE,
				TAI_ANY_LIBRARY,
				0x7A410B64,
				sceDisplaySetFrameBuf_patched);
		timer = 150; // Show OSD for 150 frames
	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {

	if (supported_game &&
			config.osd_enabled == FEATURE_ENABLED &&
			config.game_osd_enabled == FEATURE_ENABLED)
		taiHookRelease(sceDisplaySetFrameBuf_hookid, sceDisplaySetFrameBuf_hookref);

	while (injects_num-- > 0) {
		taiInjectRelease(injects[injects_num]);
	}

	return SCE_KERNEL_STOP_SUCCESS;
}
