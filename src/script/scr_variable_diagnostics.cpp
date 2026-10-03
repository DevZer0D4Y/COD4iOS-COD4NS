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

#undef GetObject
#undef FindObject

int VariableInfoFunctionCompare(const void *left, const void *right)
{
    const auto *a = static_cast<const VariableDebugInfo *>(left);
    const auto *b = static_cast<const VariableDebugInfo *>(right);
    const auto byFile = VariableInfoFileNameCompare(left, right);
    if (byFile) return byFile;
    if (!a->functionName) return b->functionName ? 1 : 0;
    return b->functionName ? I_stricmp(a->functionName, b->functionName) : -1;
}

void  Scr_DumpScriptVariables(bool spreadsheet,
	bool summary,
	bool total,
	bool functionSummary,
	bool lineSort,
	const char* fileName,
	const char* functionName,
	int minCount)
{
	uint32_t NumScriptVars; // eax
	const char* pos; // [esp+0h] [ebp-24h]
	int(__cdecl * VariableInfoCompareCallBack)(const void*, const void*); // [esp+4h] [ebp-20h]
	uint32_t index; // [esp+8h] [ebp-1Ch]
	VariableDebugInfo* pInfo; // [esp+Ch] [ebp-18h]
	VariableDebugInfo* pInfoa; // [esp+Ch] [ebp-18h]
	VariableDebugInfo* pInfob; // [esp+Ch] [ebp-18h]
	signed int num; // [esp+10h] [ebp-14h]
	int filteredCount; // [esp+14h] [ebp-10h]
	int i; // [esp+18h] [ebp-Ch]
	int ia; // [esp+18h] [ebp-Ch]
	VariableDebugInfo* infoArray; // [esp+1Ch] [ebp-8h]
	int count; // [esp+20h] [ebp-4h]

	if (scrVarDebugPub
		&& (scrVarPub.developer || !spreadsheet && !fileName && !functionName && !lineSort && !functionSummary && !minCount))
	{
		infoArray = (VariableDebugInfo*)Z_TryVirtualAlloc(sizeof(VariableDebugInfo) * 0x18000, "Scr_DumpScriptVariables", 0);
		if (infoArray)
		{
			num = 0;
			for (index = 0; index < 0x18000; ++index)
			{
				pos = scrVarDebugPub->varUsage[index];
				if (pos)
				{
					pInfo = &infoArray[num];
					if (!fileName || Scr_PrevCodePosFileNameMatches((char*)pos, fileName))
					{
						if (functionName || functionSummary)
							pInfo->functionName = Scr_PrevCodePosFunctionName((char *)pos);
						else
							pInfo->functionName = 0;
						if (!functionName || pInfo->functionName && I_stristr(pInfo->functionName, functionName))
						{
							pInfo->pos = pos;
							pInfo->fileName = Scr_PrevCodePosFileName((char *)pos);
							pInfo->varUsage = 1;
							++num;
						}
					}
				}
			}
			if (total)
			{
				Com_Printf(CON_CHANNEL_DONT_FILTER, "num vars:          %d\n", num);
				Z_VirtualFree(infoArray);
			}
			else
			{
				if (summary)
				{
					VariableInfoCompareCallBack = VariableInfoFileNameCompare;
					qsort(infoArray, num, sizeof(VariableDebugInfo), VariableInfoFileNameCompare);
				}
				else if (functionSummary)
				{
					VariableInfoCompareCallBack = VariableInfoFunctionCompare;
					qsort(infoArray, num, sizeof(VariableDebugInfo), VariableInfoFunctionCompare);
				}
				else
				{
					VariableInfoCompareCallBack = VariableInfoPositionCompare;
					qsort(infoArray, num, sizeof(VariableDebugInfo), VariableInfoPositionCompare);
				}
				i = 0;
				while (i < num)
				{
					pInfoa = &infoArray[i];
					do
					{
						++pInfoa->varUsage;
						--infoArray[i++].varUsage;
					} while (i < num && !VariableInfoCompareCallBack(pInfoa, &infoArray[i]));
				}
				if (lineSort)
					qsort(infoArray, num, sizeof(VariableDebugInfo), VariableInfoFileLineCompare);
				else
					qsort(infoArray, num, sizeof(VariableDebugInfo), VariableInfoCountCompare);
				Com_Printf(CON_CHANNEL_PARSERSCRIPT, "********************************\n");
				if (spreadsheet)
				{
					if (summary)
					{
						Com_Printf(CON_CHANNEL_DONT_FILTER, "count\tfile\n");
					}
					else if (functionSummary)
					{
						Com_Printf(CON_CHANNEL_DONT_FILTER, "count\tfile\tfunction\n");
					}
					else
					{
						Com_Printf(CON_CHANNEL_DONT_FILTER, "count\tfile\tline\tsource\tcol\n");
					}
				}
				count = 0;
				filteredCount = 0;
				for (ia = 0; ia < num; ++ia)
				{
					pInfob = &infoArray[ia];
					if (pInfob->varUsage)
					{
						count += pInfob->varUsage;
						if (pInfob->varUsage >= minCount)
						{
							filteredCount += pInfob->varUsage;
							if (spreadsheet)
							{
								Com_Printf(CON_CHANNEL_DONT_FILTER, "%d\t", pInfob->varUsage);
								Scr_PrintPrevCodePosSpreadSheet(CON_CHANNEL_DONT_FILTER, (char *)pInfob->pos, summary, functionSummary);
							}
							else
							{
								if (summary)
									MyAssertHandler(".\\script\\scr_variable.cpp", 746, 0, "%s", "!summary");
								Com_Printf(CON_CHANNEL_DONT_FILTER, "count: %d\n", pInfob->varUsage);
								Scr_PrintPrevCodePos(CON_CHANNEL_DONT_FILTER, (char*)pInfob->pos, 0);
							}
						}
					}
				}
				if (num != count)
					MyAssertHandler(".\\script\\scr_variable.cpp", 753, 0, "%s", "num == count");
				Com_Printf(CON_CHANNEL_DONT_FILTER, "********************************\n");
				Com_Printf(CON_CHANNEL_DONT_FILTER, "num vars:          %d\n", filteredCount);
				NumScriptVars = Scr_GetNumScriptVars();
				Com_Printf(CON_CHANNEL_DONT_FILTER, "num unlisted vars: %d\n", NumScriptVars - filteredCount);
				Com_Printf(CON_CHANNEL_DONT_FILTER, "********************************\n");
				Z_VirtualFree(infoArray);
			}
		}
		else
		{
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "Cannot dump script variables: out of memory\n");
		}
	}
}

void Scr_DumpScriptThreads(void)
{
	double ThreadUsage; // st7
	double ObjectUsage; // st7
	uint32_t NumScriptVars; // eax
	uint32_t NumScriptThreads; // eax
	int j; // [esp+0h] [ebp-DCh]
	int ja; // [esp+0h] [ebp-DCh]
	uint32_t classnum; // [esp+4h] [ebp-D8h]
	const char* pos; // [esp+8h] [ebp-D4h]
	ThreadDebugInfo info; // [esp+Ch] [ebp-D0h]
	const char* buf; // [esp+A0h] [ebp-3Ch]
	int size; // [esp+A4h] [ebp-38h]
	VariableValueInternal* entryValue; // [esp+A8h] [ebp-34h]
	ThreadDebugInfo* pInfo; // [esp+ACh] [ebp-30h]
	int num; // [esp+B0h] [ebp-2Ch]
	uint8_t type; // [esp+B7h] [ebp-25h]
	VariableUnion u; // [esp+B8h] [ebp-24h]
	int i; // [esp+BCh] [ebp-20h]
	const VariableStackBuffer* stackBuf; // [esp+C0h] [ebp-1Ch]
	uint32_t entId; // [esp+C4h] [ebp-18h]
	ThreadDebugInfo* infoArray; // [esp+C8h] [ebp-14h]
	int count; // [esp+CCh] [ebp-10h]
	float endonUsage; // [esp+D0h] [ebp-Ch]
	uint32_t id; // [esp+D4h] [ebp-8h]
	float varUsage; // [esp+D8h] [ebp-4h]

	num = 0;
	for (id = 1; id < 0xFFFE; ++id)
	{
		entryValue = &scrVarGlob.variableList[id + VARIABLELIST_CHILD_BEGIN];
		if ((entryValue->w.status & VAR_STAT_MASK) != 0 && (entryValue->w.status & VAR_MASK) == VAR_STACK)
			++num;
	}
	if (num)
	{
		infoArray = (ThreadDebugInfo*)Z_TryVirtualAlloc(sizeof(ThreadDebugInfo) * num, "Scr_DumpScriptThreads", 0);
		if (infoArray)
		{
			num = 0;
			for (id = 1; id < 0xFFFE; ++id)
			{
				entryValue = &scrVarGlob.variableList[id + VARIABLELIST_CHILD_BEGIN];
				if ((entryValue->w.status & VAR_STAT_MASK) != 0 && (entryValue->w.status & VAR_MASK) == VAR_STACK)
				{
					pInfo = &infoArray[num++];
					info.posSize = 0;
					stackBuf = entryValue->u.u.stackValue;
					size = stackBuf->size;
					pos = stackBuf->pos;
					buf = stackBuf->buf;
					while (size)
					{
						--size;
						type = *buf++;
						u = Scr_ReadStackValue(buf);
						buf += SCR_STACK_VALUE_BYTES;
						if (type == VAR_CODEPOS)
							info.pos[info.posSize++] = u.codePosValue;
					}
					info.pos[info.posSize++] = pos;
					ThreadUsage = Scr_GetThreadUsage(stackBuf, &pInfo->endonUsage);
					pInfo->varUsage = ThreadUsage;
					pInfo->posSize = info.posSize--;
					for (j = 0; j < pInfo->posSize; ++j)
						pInfo->pos[j] = info.pos[info.posSize - j];
				}
			}
			qsort(infoArray, num, sizeof(ThreadDebugInfo), ThreadInfoCompare);
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "********************************\n");
			varUsage = 0.0;
			endonUsage = 0.0;
			i = 0;
			while (i < num)
			{
				pInfo = &infoArray[i];
				count = 0;
				info.varUsage = 0.0;
				info.endonUsage = 0.0;
				do
				{
					++count;
					info.varUsage = info.varUsage + infoArray[i].varUsage;
					info.endonUsage = info.endonUsage + infoArray[i++].endonUsage;
				} while (i < num && !ThreadInfoCompare((uint32*)pInfo, (uint32*)&infoArray[i]));
				varUsage = varUsage + info.varUsage;
				endonUsage = endonUsage + info.endonUsage;
				Com_Printf(CON_CHANNEL_PARSERSCRIPT, "count: %d, var usage: %d, endon usage: %d\n", count, (int)info.varUsage, (int)info.endonUsage);
				Scr_PrintPrevCodePos(CON_CHANNEL_PARSERSCRIPT, (char*)pInfo->pos[0], 0);
				for (ja = 1; ja < pInfo->posSize; ++ja)
				{
					Com_Printf(CON_CHANNEL_PARSERSCRIPT, "called from:\n");
					Scr_PrintPrevCodePos(CON_CHANNEL_PARSERSCRIPT, (char*)pInfo->pos[ja], 0);
				}
			}
			Z_VirtualFree(infoArray);
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "********************************\n");
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "var usage: %d, endon usage: %d\n", (int)varUsage, (int)endonUsage);
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "\n");
			for (classnum = 0; classnum < CLASS_NUM_COUNT; ++classnum)
			{
				if (g_classMap[classnum].entArrayId)
				{
					info.varUsage = 0.0;
					count = 0;
					for (entId = FindFirstSibling(g_classMap[classnum].entArrayId); entId; entId = FindNextSibling(entId))
					{
						++count;
					if ((scrVarGlob.variableList[entId + VARIABLELIST_CHILD_BEGIN].w.status & VAR_MASK) == VAR_POINTER)
						{
							ObjectUsage = Scr_GetObjectUsage(scrVarGlob.variableList[entId + VARIABLELIST_CHILD_BEGIN].u.u.stringValue);
							info.varUsage = ObjectUsage + info.varUsage;
						}
					}
					Com_Printf(
						CON_CHANNEL_PARSERSCRIPT,
						"ent type '%s'... count: %d, var usage: %d\n",
						g_classMap[classnum].name,
						count,
						(int)info.varUsage);
				}
			}
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "********************************\n");
			NumScriptVars = Scr_GetNumScriptVars();
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "num vars:    %d\n", NumScriptVars);
			NumScriptThreads = Scr_GetNumScriptThreads();
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "num threads: %d\n", NumScriptThreads);
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "********************************\n");
		}
		else
		{
			Com_Printf(CON_CHANNEL_PARSERSCRIPT, "Cannot dump script threads: out of memory\n");
		}
	}
}

int ThreadInfoCompare(const void *left, const void *right)
{
    const auto *a = static_cast<const ThreadDebugInfo *>(left);
    const auto *b = static_cast<const ThreadDebugInfo *>(right);
    for (int i = 0; i < a->posSize && i < b->posSize; ++i)
    {
        const auto first = reinterpret_cast<std::uintptr_t>(a->pos[i]);
        const auto second = reinterpret_cast<std::uintptr_t>(b->pos[i]);
        if (first != second) return first > second ? 1 : -1;
    }
    return a->posSize > b->posSize ? 1 : a->posSize < b->posSize ? -1 : 0;
}

int VariableInfoFileNameCompare(const void *left, const void *right)
{
    const auto *a = static_cast<const VariableDebugInfo *>(left);
    const auto *b = static_cast<const VariableDebugInfo *>(right);
    if (!a->fileName) return b->fileName ? 1 : 0;
    return b->fileName ? I_stricmp(a->fileName, b->fileName) : -1;
}

int VariableInfoCountCompare(const void *left, const void *right)
{
    const auto a = static_cast<const VariableDebugInfo *>(left)->varUsage;
    const auto b = static_cast<const VariableDebugInfo *>(right)->varUsage;
    return a > b ? 1 : a < b ? -1 : 0;
}

int VariableInfoFileLineCompare(const void *left, const void *right)
{
    const auto byFile = VariableInfoFileNameCompare(left, right);
    return byFile ? byFile : VariableInfoPositionCompare(left, right);
}

int VariableInfoPositionCompare(const void *left, const void *right)
{
    const auto a = reinterpret_cast<std::uintptr_t>(static_cast<const VariableDebugInfo *>(left)->pos);
    const auto b = reinterpret_cast<std::uintptr_t>(static_cast<const VariableDebugInfo *>(right)->pos);
    return a > b ? 1 : a < b ? -1 : 0;
}
