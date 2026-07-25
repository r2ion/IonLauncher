#include "rtech/rui/workshop_thumbnail_atlas.h"
#include "rtech/rui/workshop_thumbnail_service.h"
#include "vscript/squirrel/squirrel.h"

ADD_SQFUNC("bool", NSMWSInitializeThumbnailAtlas, "", "Initializes the fixed ModWorkshop thumbnail texture atlas.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CWorkshopThumbnailService::Get().RepaintVisible();
	g_pSquirrel[context]->pushbool(sqvm, CWorkshopThumbnailAtlas::Get().IsReady());
	return SQRESULT_NOTNULL;
}
