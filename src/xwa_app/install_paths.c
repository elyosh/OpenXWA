#include "install_paths.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char* const k_userConfigPath = "config.yaml";

static int install_paths_error(char* error, size_t error_size, const char* message, const char* detail) {
	if (error && error_size) {
		snprintf(error, error_size, message, detail ? detail : "");
	}
	return 0;
}

static int install_paths_optional_path(const AeronConfigFile* config, const char* key, char* out,
									   size_t capacity, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, key);
	const char* value;

	if (!node) {
		out[0] = '\0';
		return 1;
	}
	value = AeronConfigNode_String(node, NULL);
	if (!value) {
		return install_paths_error(error, error_size, "invalid path setting '%s'", key);
	}
	if (!value[0]) {
		out[0] = '\0';
		return 1;
	}
	if (strlen(value) >= capacity) {
		return install_paths_error(error, error_size, "configured path is too long: '%s'", key);
	}
	snprintf(out, capacity, "%s", value);
	return 1;
}

static int install_paths_use_mod(const AeronConfigFile* config, int* out, char* error, size_t error_size) {
	const AeronConfigNode* node = AeronConfigFile_GetNode(config, "paths.use_mod");

	if (!node) {
		return 1;
	}
	if (AeronConfigNode_Type(node) != AERON_CONFIG_BOOL) {
		return install_paths_error(error, error_size, "invalid 'paths.use_mod': expected %s", "boolean");
	}
	*out = AeronConfigNode_Bool(node, 1);
	return 1;
}

int XwaInstallPaths_CheckVersion(const AeronConfigFile* config, const char* path, char* error,
								 size_t error_size) {
	const AeronConfigNode* version = AeronConfigFile_GetNode(config, "version");
	int64_t value;

	if (!version) {
		return 1;
	}
	value = AeronConfigNode_Type(version) == AERON_CONFIG_INT ? AeronConfigNode_Int(version, 0) : 0;
	if (value < 1 || value > XWA_CONFIG_VERSION) {
		return install_paths_error(error, error_size, "unsupported configuration version in %s", path);
	}
	return 1;
}

int XwaInstallPaths_Read(const AeronConfigFile* config, XwaInstallPaths* out, char* error,
						 size_t error_size) {
	const AeronConfigNode* paths = AeronConfigFile_GetNode(config, "paths");

	memset(out, 0, sizeof *out);
	out->use_mod = 1;
	if (paths && AeronConfigNode_Type(paths) != AERON_CONFIG_MAP) {
		return install_paths_error(error, error_size, "'paths' must be a mapping in %s", k_userConfigPath);
	}
	return install_paths_optional_path(config, "paths.game_data", out->game_data, sizeof out->game_data,
									   error, error_size) &&
		   install_paths_optional_path(config, "paths.mod", out->mod, sizeof out->mod, error, error_size) &&
		   install_paths_optional_path(config, "paths.mods", out->mods, sizeof out->mods, error,
									   error_size) &&
		   install_paths_use_mod(config, &out->use_mod, error, error_size);
}

/* Loads the user's config.yaml and checks its root and version. */
static int install_paths_load_user(AeronVfs* vfs, AeronConfigFile** config, char* error, size_t error_size) {
	if (!AeronConfigFile_LoadYaml(vfs, AERON_VFS_ROOT_USER, k_userConfigPath, config)) {
		return install_paths_error(error, error_size, "user configuration is invalid: %s", k_userConfigPath);
	}
	if (AeronConfigNode_Type(AeronConfigFile_Root(*config)) != AERON_CONFIG_MAP) {
		AeronConfigFile_Destroy(*config);
		*config = NULL;
		return install_paths_error(error, error_size, "user configuration root must be a mapping: %s",
								   k_userConfigPath);
	}
	if (!XwaInstallPaths_CheckVersion(*config, k_userConfigPath, error, error_size)) {
		AeronConfigFile_Destroy(*config);
		*config = NULL;
		return 0;
	}
	return 1;
}

int XwaInstallPaths_Load(AeronVfs* vfs, XwaInstallPaths* out, char* error, size_t error_size) {
	AeronConfigFile* config = NULL;
	int valid;

	if (!vfs || !out) {
		return install_paths_error(error, error_size, "cannot load %s", k_userConfigPath);
	}
	memset(out, 0, sizeof *out);
	out->use_mod = 1;
	if (!AeronVfs_Exists(vfs, AERON_VFS_ROOT_USER, k_userConfigPath)) {
		return 1;
	}
	if (!install_paths_load_user(vfs, &config, error, error_size)) {
		return 0;
	}
	valid = XwaInstallPaths_Read(config, out, error, error_size);
	AeronConfigFile_Destroy(config);
	return valid;
}

/* Rereads the user's config.yaml for a save, or creates it when it is
 * missing. */
static int install_paths_prepare_user_document(AeronVfs* vfs, AeronConfigFile** document, char* error,
											   size_t error_size) {
	AeronConfigError config_error = { 0 };

	if (AeronVfs_Exists(vfs, AERON_VFS_ROOT_USER, k_userConfigPath)) {
		if (!AeronConfigFile_LoadYamlEx(vfs, AERON_VFS_ROOT_USER, k_userConfigPath, document,
										&config_error)) {
			return install_paths_error(error, error_size, "could not update user configuration: %s",
									   config_error.message);
		}
		if (AeronConfigNode_Type(AeronConfigFile_Root(*document)) != AERON_CONFIG_MAP) {
			AeronConfigFile_Destroy(*document);
			*document = NULL;
			return install_paths_error(error, error_size, "user configuration root must be a mapping: %s",
									   k_userConfigPath);
		}
		if (!XwaInstallPaths_CheckVersion(*document, k_userConfigPath, error, error_size)) {
			AeronConfigFile_Destroy(*document);
			*document = NULL;
			return 0;
		}
		return 1;
	}
	if (!AeronConfigFile_CreateMap(AERON_VFS_ROOT_USER, k_userConfigPath, document, &config_error) ||
		!AeronConfigFile_SetInt(*document, "version", XWA_CONFIG_VERSION, &config_error)) {
		AeronConfigFile_Destroy(*document);
		*document = NULL;
		return install_paths_error(error, error_size, "could not create user configuration: %s",
								   config_error.message);
	}
	return 1;
}

static int install_paths_save_user_document(AeronVfs* vfs, AeronConfigFile* document, char* error,
											size_t error_size) {
	AeronConfigError config_error = { 0 };
	int saved = AeronConfigFile_SaveYaml(vfs, document, &config_error);

	AeronConfigFile_Destroy(document);
	if (!saved) {
		return install_paths_error(error, error_size, "could not save user configuration: %s",
								   config_error.message);
	}
	return 1;
}

static int install_paths_save_string(AeronVfs* vfs, const char* key, const char* value, char* error,
									 size_t error_size) {
	AeronConfigFile* document = NULL;
	AeronConfigError config_error = { 0 };

	if (!vfs || !value || !value[0] || strlen(value) >= XWA_INSTALL_PATH_CAPACITY) {
		return install_paths_error(error, error_size, "cannot save invalid path to %s", k_userConfigPath);
	}
	if (!install_paths_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (!AeronConfigFile_SetString(document, key, value, &config_error)) {
		AeronConfigFile_Destroy(document);
		return install_paths_error(error, error_size, "could not update user configuration: %s",
								   config_error.message);
	}
	return install_paths_save_user_document(vfs, document, error, error_size);
}

int XwaInstallPaths_SaveGameData(AeronVfs* vfs, const char* game_data, char* error, size_t error_size) {
	return install_paths_save_string(vfs, "paths.game_data", game_data, error, error_size);
}

int XwaInstallPaths_SaveMods(AeronVfs* vfs, const char* mods, char* error, size_t error_size) {
	return install_paths_save_string(vfs, "paths.mods", mods, error, error_size);
}

int XwaInstallPaths_SaveMod(AeronVfs* vfs, const char* mod, int use_mod, char* error, size_t error_size) {
	AeronConfigFile* document = NULL;
	AeronConfigError config_error = { 0 };
	int updated;

	if (!vfs || (mod && strlen(mod) >= XWA_INSTALL_PATH_CAPACITY)) {
		return install_paths_error(error, error_size, "cannot save invalid path to %s", k_userConfigPath);
	}
	if (!install_paths_prepare_user_document(vfs, &document, error, error_size)) {
		return 0;
	}
	if (mod && mod[0]) {
		updated = AeronConfigFile_SetString(document, "paths.mod", mod, &config_error);
	} else {
		updated = !AeronConfigFile_Has(document, "paths.mod") ||
				  AeronConfigFile_Remove(document, "paths.mod", &config_error);
	}
	updated = updated && AeronConfigFile_SetBool(document, "paths.use_mod", use_mod != 0, &config_error);
	if (!updated) {
		AeronConfigFile_Destroy(document);
		return install_paths_error(error, error_size, "could not update user configuration: %s",
								   config_error.message);
	}
	return install_paths_save_user_document(vfs, document, error, error_size);
}
