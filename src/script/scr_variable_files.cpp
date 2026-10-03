#include <universal/q_shared.h>
#include "scr_main.h"
#include "scr_animtree.h"
#include "scr_variable.h"
#include "scr_stringlist.h"
#include "scr_memorytree.h"
#include "scr_vm.h"
#include "scr_compiler.h"

#include <qcommon/qcommon.h>
#include <universal/com_files.h>
#include "scr_parser.h"
#include <database/database.h>
#include <universal/q_parse.h>
#include <cstdint>
#include "scr_bytecode.h"

#undef GetObject
#undef FindObject

void __cdecl Scr_AddFields_LoadObj(const char *path, const char *extension)
{
	char filename[68]; // [esp+10h] [ebp-58h] BYREF
	int numFiles; // [esp+58h] [ebp-10h] BYREF
	char *targetPos; // [esp+5Ch] [ebp-Ch]
	int i; // [esp+60h] [ebp-8h]
	const char **files; // [esp+64h] [ebp-4h]

	files = FS_ListFiles(path, extension, FS_LIST_PURE_ONLY, &numFiles);
	scrVarPub.fieldBuffer = TempMalloc(0);
	*scrVarPub.fieldBuffer = 0;
	for (i = 0; i < numFiles; ++i)
	{
		snprintf(filename, ARRAYSIZE(filename), "%s/%s", path, files[i]);
		if (strlen(filename) >= 0x40)
			MyAssertHandler(".\\script\\scr_variable.cpp", 5191, 0, "%s", "strlen( filename ) < MAX_QPATH");
		Scr_AddFieldsForFile(filename);
	}
	if (files)
		FS_FreeFileList(files);
	targetPos = TempMalloc(1);
	*targetPos = 0;
}

void  Scr_AddFields(char const* path, char const* extension)
{
	if (IsFastFileLoad())
		Scr_AddFields_FastFile(path, extension);
	else
		Scr_AddFields_LoadObj(path, extension);
}

char* Scr_GetSourceFile_FastFile(char const* filename)
{
	const char* v1; // eax
	RawFile* rawfile; // [esp+4h] [ebp-4h]

	rawfile = DB_FindXAssetHeader(ASSET_TYPE_RAWFILE, filename).rawfile;
	if (!rawfile)
	{
		v1 = va("cannot find %s", filename);
		Com_Error(ERR_DROP, v1);
	}
	return (char*)rawfile->buffer;
}

char* Scr_GetSourceFile(char const* filename)
{
	const char* v1; // eax
	char* sourceBuffer; // [esp+0h] [ebp-Ch]
	int len; // [esp+4h] [ebp-8h]
	int f; // [esp+8h] [ebp-4h] BYREF

	len = FS_FOpenFileByMode((char*)filename, &f, FS_READ);
	if (len < 0)
	{
		v1 = va("cannot find %s", filename);
		Com_Error(ERR_DROP, v1);
	}
	sourceBuffer = (char*)Hunk_AllocateTempMemoryHigh(len + 1, "Scr_LoadAnimTreeInternal");
	FS_Read((unsigned char*)sourceBuffer, len, f);
	sourceBuffer[len] = 0;
	FS_FCloseFile(f);
	return sourceBuffer;
}

char *__cdecl Scr_GetSourceFile_LoadObj(const char *filename)
{
	const char *v1; // eax
	char *sourceBuffer; // [esp+0h] [ebp-Ch]
	int len; // [esp+4h] [ebp-8h]
	int f; // [esp+8h] [ebp-4h] BYREF

	len = FS_FOpenFileByMode((char*)filename, &f, FS_READ);
	if (len < 0)
	{
		v1 = va("cannot find %s", filename);
		Com_Error(ERR_DROP, v1);
	}
	sourceBuffer = (char*)Hunk_AllocateTempMemoryHigh(len + 1, "Scr_LoadAnimTreeInternal");
	FS_Read((unsigned char*)sourceBuffer, len, f);
	sourceBuffer[len] = 0;
	FS_FCloseFile(f);
	return sourceBuffer;
}

void  Scr_AddFieldsForFile(char const* filename)
{
	const char* SourceFile_FastFile_DONE; // eax
	const char* v2; // eax
	const char* v3; // eax
	char v4; // [esp+3h] [ebp-9Dh]
	char* v5; // [esp+8h] [ebp-98h]
	char* v6; // [esp+Ch] [ebp-94h]
	int v7; // [esp+10h] [ebp-90h]
	int tempType[2]; // [esp+78h] [ebp-28h] BYREF
	int len; // [esp+80h] [ebp-20h]
	int size; // [esp+84h] [ebp-1Ch]
	char* targetPos; // [esp+88h] [ebp-18h]
	uint32_t index; // [esp+8Ch] [ebp-14h]
	int type; // [esp+90h] [ebp-10h]
	const char* sourcePos; // [esp+94h] [ebp-Ch] BYREF
	char* token; // [esp+98h] [ebp-8h]
	int i; // [esp+9Ch] [ebp-4h]

	Hunk_CheckTempMemoryHighClear();
	if (IsFastFileLoad())
		SourceFile_FastFile_DONE = (const char*)Scr_GetSourceFile_FastFile(filename);
	else
		SourceFile_FastFile_DONE = Scr_GetSourceFile_LoadObj(filename);
	sourcePos = SourceFile_FastFile_DONE;
	Com_BeginParseSession("Scr_AddFields");
	for (targetPos = TempMalloc(0); ; *targetPos = 0)
	{
		token = (char*)Com_Parse(&sourcePos);
		if (!sourcePos)
			break;
		if (!strcmp(token, "float"))
		{
			type = VAR_FLOAT;
		}
		else if (!strcmp(token, "int"))
		{
			type = VAR_INTEGER;
		}
		else if (!strcmp(token, "string"))
		{
			type = VAR_STRING;
		}
		else
		{
			if (strcmp(token, "vector"))
			{
				v2 = va("Unknown type %s in %s", token, filename);
				Com_Error(ERR_DROP, v2);
				return;
			}
			type = VAR_VECTOR;
		}
		token = (char*)Com_Parse(&sourcePos);
		if (!sourcePos)
		{
			v3 = va("missing field name in %s", filename);
			Com_Error(ERR_DROP, v3);
		}
		v7 = strlen(token);
		len = v7 + 1;
		for (i = v7; i >= 0; --i)
			token[i] = tolower(token[i]);
		index = SL_GetCanonicalString(token);
		if (Scr_FindField(token, tempType))
			Com_Error(ERR_DROP, "duplicate key %s in %s", token, filename);
		TempMemorySetPos(targetPos);
		size = len + 4;
		targetPos = TempMalloc(len + 4);
		v6 = token;
		v5 = targetPos;

		do
		{
			v4 = *v6;
			*v5++ = *v6++;
		} while (v4);

		targetPos += len;
        Scr_WriteBytecode<uint16_t>(targetPos, index);
		targetPos += 2;
		*targetPos++ = type;
	}
	Com_EndParseSession();
	Hunk_ClearTempMemoryHigh();
}

void  Scr_AddFields_FastFile(char const* path, char const* extension)
{
	char filename[64]; // [esp+0h] [ebp-48h] BYREF
	char* targetPos; // [esp+44h] [ebp-4h]

	scrVarPub.fieldBuffer = TempMalloc(0);
	*scrVarPub.fieldBuffer = 0;
	snprintf(filename, ARRAYSIZE(filename), "%s/%s.%s", path, "keys", extension);
	Scr_AddFieldsForFile(filename);
	targetPos = TempMalloc(1);
	*targetPos = 0;
}
