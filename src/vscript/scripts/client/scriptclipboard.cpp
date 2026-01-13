#include "vscript/squirrel/squirrel.h"

ADD_SQFUNC("void", NSCopyToClipboard, "string text", "", ScriptContext::UI)
{
	const SQChar* text = g_pSquirrel[context]->getstring(sqvm, 1);

	if (!OpenClipboard(nullptr))
		return SQRESULT_NULL;

	EmptyClipboard();

	size_t len = strlen(text) + 1;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	if (hMem)
	{
		void* pMem = GlobalLock(hMem);
		if (pMem)
		{
			memcpy(pMem, text, len);
			GlobalUnlock(hMem);
			SetClipboardData(CF_TEXT, hMem);
		}
		else
		{
			GlobalFree(hMem);
		}
	}

	CloseClipboard();

	return SQRESULT_NULL;
}

ADD_SQFUNC("string", NSPasteFromClipboard, "", "", ScriptContext::UI)
{
	if (!OpenClipboard(nullptr))
	{
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == nullptr)
	{
		CloseClipboard();
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	char* pText = static_cast<char*>(GlobalLock(hData));
	if (pText == nullptr)
	{
		CloseClipboard();
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	std::string result(pText);

	GlobalUnlock(hData);
	CloseClipboard();

	g_pSquirrel[context]->pushstring(sqvm, result.c_str());
	return SQRESULT_NOTNULL;
}
