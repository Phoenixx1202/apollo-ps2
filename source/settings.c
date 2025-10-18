#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include <ahx_rpc.h>

#include "types.h"
#include "menu.h"
#include "saves.h"
#include "common.h"
#include "icons.h"


char *strcasestr(const char *, const char *);
static const char* ext_src[MAX_USB_DEVICES+1] = {"mass:/", "host:/", "cdfs:/", NULL};
static const char* sort_opt[] = {"Desativado", "Por Nome", "Por ID do Título", "Por Tipo", NULL};

menu_option_t menu_options[] = {
	{ .name = "\nMúsica de Fundo", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.music, 
		.callback = music_callback 
	},
	{ .name = "Animações do Menu", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.doAni, 
		.callback = ani_callback 
	},
	{ .name = "Ordenar Dados Salvos", 
		.options = (char**) sort_opt,
		.type = APP_OPTION_LIST,
		.value = &apollo_config.doSort, 
		.callback = sort_callback 
	},
	{ .name = "Fonte de Dados Salvos Externos",
		.options = (char**) ext_src,
		.type = APP_OPTION_LIST,
		.value = &apollo_config.storage,
		.callback = owner_callback
	},
/*
	{ .name = "Version Update Check", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.update, 
		.callback = update_callback 
	},
	{ .name = "Update Application Data", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = upd_appdata_callback 
	},
*/
	{ .name = "Limpar Cache Local", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = clearcache_callback 
	},
	{ .name = "Habilitar Log de Depuração",
		.options = NULL,
		.type = APP_OPTION_CALL,
		.value = NULL,
		.callback = log_callback 
	},
	{ .name = NULL }
};


void music_callback(int sel)
{
	apollo_config.music = !sel;

	if(apollo_config.music)
		AHX_Play();
	else
		AHX_Pause();
}

void sort_callback(int sel)
{
	apollo_config.doSort = sel;
}

void ani_callback(int sel)
{
	apollo_config.doAni = !sel;
}

void clearcache_callback(int sel)
{
	LOG("Limpando pasta '%s'...", APOLLO_LOCAL_CACHE);
	clean_directory(APOLLO_LOCAL_CACHE);

	show_message("Pasta do Cache Local Limpa:\n" APOLLO_LOCAL_CACHE);
}

void upd_appdata_callback(int sel)
{
	int i;

	if (!http_download(ONLINE_PATCH_URL, "apollo-psp-update.zip", APOLLO_LOCAL_CACHE "appdata.zip", 1))
	{
		show_message("Erro! Não é possível baixar o pacote de atualização de dados!");
		return;
	}

	if ((i = extract_zip(APOLLO_LOCAL_CACHE "appdata.zip", APOLLO_DATA_PATH)) > 0)
		show_message("%d Arquivos de dados foram atualizados com êxito!", i);
	else
		show_message("Erro! Falha ao extrair o pacote de atualização de dados!");

	unlink_secure(APOLLO_LOCAL_CACHE "appdata.zip");
}

void update_callback(int sel)
{
    apollo_config.update = !sel;

    if (!apollo_config.update)
        return;

	LOG("Verificando a versão mais recente do Apollo em %s", APOLLO_UPDATE_URL);

	if (!http_download(APOLLO_UPDATE_URL, NULL, APOLLO_LOCAL_CACHE "ver.check", 0))
	{
		LOG("A solicitação HTTP para %s falhou", APOLLO_UPDATE_URL);
		return;
	}

	char *buffer;
	long size = 0;

	buffer = readTextFile(APOLLO_LOCAL_CACHE "ver.check", &size);

	if (!buffer)
		return;

	LOG("%ld bytes recebidos", size);

	static const char find[] = "\"name\":\"Apollo Save Tool v";
	const char* start = strstr(buffer, find);
	if (!start)
	{
		LOG("Nome não encontrado");
		goto end_update;
	}

	LOG("Nome encontrado");
	start += sizeof(find) - 1;

	char* end = strchr(start, '"');
	if (!end)
	{
		LOG("Fim do nome não encontrado");
		goto end_update;
	}
	*end = 0;
	LOG("A versão mais recente é %s", start);

	if (strcasecmp(APOLLO_VERSION, start) == 0)
	{
		LOG("Não precisa atualizar");
		goto end_update;
	}

	start = strstr(end+1, "\"browser_download_url\":\"");
	if (!start)
		goto end_update;

	start += 24;
	end = strchr(start, '"');
	if (!end)
	{
		LOG("URL de download não encontrada");
		goto end_update;
	}

	*end = 0;
	LOG("A URL de download é %s", start);

	if (show_dialog(DIALOG_TYPE_YESNO, "Nova versão disponível! Baixar atualização?"))
	{
		if (http_download(start, NULL, "ms0:/APOLLO/apollo-psp.zip", 1))
			show_message("Atualização baixada para ms0:/APOLLO/apollo-psp.zip");
		else
			show_message("Erro de Download!");
	}

end_update:
	free(buffer);
	return;
}

void owner_callback(int sel)
{
	apollo_config.storage = sel;
}

void log_callback(int sel)
{
	dbglogger_init_mode(FILE_LOGGER, USB_PATH "apollo.log", 1);
	show_message("Log de Depuração Habilitado!\n\n" USB_PATH "apollo.log");
}

int save_app_settings(app_config_t* config)
{
	char filePath[256];

	LOG("Apollo Save Tool v%s - Patch Engine v%s", APOLLO_VERSION, APOLLO_LIB_VERSION);
	snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "icon.sys");
	if (mkdirs(filePath) != SUCCESS)
	{
		LOG("sceSaveDataMount2 ERROR");
		return 0;
	}

	LOG("Salvando Configurações...");
	if (file_exists(filePath) != SUCCESS)
	{
		uLong destLen = size_icon_sys;
		Bytef *dest = malloc(destLen);

		uncompress(dest, &destLen, zicon_sys, sizeof(zicon_sys));
		write_buffer(filePath, dest, destLen);
		free(dest);

		snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "icon.ico");
		destLen = size_icon_ico;
		dest = malloc(destLen);
		uncompress(dest, &destLen, zicon_ico, sizeof(zicon_ico));
		write_buffer(filePath, dest, destLen);
		free(dest);
	}

	snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "SETTINGS.BIN");
	if (write_buffer(filePath, (uint8_t*) config, sizeof(app_config_t)) < 0)
	{
		LOG("Erro ao Salvar Configurações!");
		return 0;
	}

	return 1;
}

int load_app_settings(app_config_t* config)
{
	char filePath[256];
	app_config_t* file_data;
	size_t file_size;

	config->user_id = 0;

	snprintf(filePath, sizeof(filePath), "%s%s%s", MC0_PATH, "APOLLO-99PS2/", "SETTINGS.BIN");

	LOG("Carregando Configurações...");
	if (read_buffer(filePath, (uint8_t**) &file_data, &file_size) == SUCCESS && file_size == sizeof(app_config_t))
	{
		file_data->user_id = config->user_id;
		memcpy(config, file_data, file_size);

		LOG("Configurações carregadas: ID do Usuário (%08x)", config->user_id);
		free(file_data);
	}
	else
	{
		LOG("Configurações não encontradas, usando padrões");
		save_app_settings(config);
		return 0;
	}

	return 1;
}
