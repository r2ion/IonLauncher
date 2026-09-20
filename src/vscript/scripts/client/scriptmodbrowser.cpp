#include "modsystem/modinstaller.h"
#include "modsystem/modbrowser.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <limits>
#include <mutex>
#include <string>

class CModBrowserSquirrel final
{
  public:
    static int SquirrelInteger(uint64_t value);
    static std::string OperationId(const ModInstallOperationSnapshot* operation);
    static void EnsureCallbacks();

    template <ScriptContext context>
    static void PushCatalogEntry(HSQUIRRELVM sqvm, const ModBrowserEntry& entry, const ModInstallOperationSnapshot* operation,
                                 std::string_view operationId);
    template <ScriptContext context> static void PushPageSnapshot(HSQUIRRELVM sqvm);
    template <ScriptContext context> static void PushDetailsSnapshot(HSQUIRRELVM sqvm);
    template <ScriptContext context> static void PushOperationSnapshot(HSQUIRRELVM sqvm);
    template <ScriptContext context> static void PushInventorySnapshot(HSQUIRRELVM sqvm);
    template <ScriptContext context> static SQRESULT RequestOperation(HSQUIRRELVM sqvm, ModInstallAction action);

  private:
    static void OnPageChanged(uint64_t generation);
    static void OnDetailsChanged(const std::string& id);
    static void OnUpdatesChanged(uint64_t generation, int updateCount, ModInventoryUpdateStage stage);
    static void OnOperationChanged();

    inline static std::once_flag s_CallbacksInitialized;
};

int CModBrowserSquirrel::SquirrelInteger(uint64_t value)
{
    return static_cast<int>(std::min<uint64_t>(value, std::numeric_limits<int>::max()));
}

std::string CModBrowserSquirrel::OperationId(const ModInstallOperationSnapshot* operation)
{
    if (!operation)
        return {};
    if (operation->source == ModSource::ModWorkshop)
        return operation->modId ? CModBrowserService::BuildId(operation->source, std::to_string(operation->modId)) : std::string();
    return CModBrowserService::BuildId(operation->source, operation->packageId);
}

void CModBrowserSquirrel::OnPageChanged(uint64_t generation)
{
    SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
    if (squirrel && squirrel->m_pSQVM)
        squirrel->AsyncCall("NSUICodeCallback_ModBrowserPageChanged", SquirrelInteger(generation));
}

void CModBrowserSquirrel::OnDetailsChanged(const std::string& id)
{
    SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
    if (squirrel && squirrel->m_pSQVM)
        squirrel->AsyncCall("NSUICodeCallback_ModBrowserDetailsChanged", id);
}


void CModBrowserSquirrel::OnUpdatesChanged(uint64_t generation, int updateCount, ModInventoryUpdateStage stage)
{
    SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
    if (squirrel && squirrel->m_pSQVM)
        squirrel->AsyncCall("NSUICodeCallback_ModBrowserUpdatesChanged", SquirrelInteger(generation), updateCount, static_cast<int>(stage));
}

void CModBrowserSquirrel::OnOperationChanged()
{
    SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
    if (squirrel && squirrel->m_pSQVM)
        squirrel->AsyncCall("NSUICodeCallback_ModBrowserOperationChanged");
}

void CModBrowserSquirrel::EnsureCallbacks()
{
    std::call_once(s_CallbacksInitialized, []
    {
        CModBrowserService::Get().SetPageChangedCallback(OnPageChanged);
        CModBrowserService::Get().SetDetailsChangedCallback(OnDetailsChanged);
        CModBrowserService::Get().SetUpdatesChangedCallback(OnUpdatesChanged);
        CModInstallService::Get().SetOperationChangedCallback(OnOperationChanged);
    });
}

template <ScriptContext context>
void CModBrowserSquirrel::PushCatalogEntry(HSQUIRRELVM sqvm, const ModBrowserEntry& entry,
                                           const ModInstallOperationSnapshot* operation, std::string_view operationId)
{
    const bool operationMatches = operation && operationId == entry.id;
    g_pSquirrel[context]->pushnewstructinstance(sqvm, 20);
    g_pSquirrel[context]->pushstring(sqvm, entry.id.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 0);
    g_pSquirrel[context]->pushstring(sqvm, entry.name.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 1);
    g_pSquirrel[context]->pushstring(sqvm, entry.author.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 2);
    const std::string& summary = entry.shortDescription.empty() ? entry.description : entry.shortDescription;
    g_pSquirrel[context]->pushstring(sqvm, summary.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 3);
    g_pSquirrel[context]->pushstring(sqvm, entry.version.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 4);
    g_pSquirrel[context]->pushinteger(sqvm, SquirrelInteger(entry.downloads));
    g_pSquirrel[context]->sealstructslot(sqvm, 5);
    g_pSquirrel[context]->pushinteger(sqvm, SquirrelInteger(entry.likes));
    g_pSquirrel[context]->sealstructslot(sqvm, 6);
    g_pSquirrel[context]->pushinteger(sqvm, SquirrelInteger(entry.views));
    g_pSquirrel[context]->sealstructslot(sqvm, 7);
    g_pSquirrel[context]->pushstring(sqvm, entry.thumbnailUrl.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 8);
    g_pSquirrel[context]->pushbool(sqvm, entry.installed);
    g_pSquirrel[context]->sealstructslot(sqvm, 9);
    g_pSquirrel[context]->pushinteger(sqvm, entry.updateState);
    g_pSquirrel[context]->sealstructslot(sqvm, 10);
    g_pSquirrel[context]->pushinteger(sqvm, operationMatches ? static_cast<int>(operation->state) : static_cast<int>(ModInstallOperationState::Idle));
    g_pSquirrel[context]->sealstructslot(sqvm, 11);
    g_pSquirrel[context]->pushbool(sqvm, entry.approved);
    g_pSquirrel[context]->sealstructslot(sqvm, 12);
    g_pSquirrel[context]->pushbool(sqvm, entry.suspended);
    g_pSquirrel[context]->sealstructslot(sqvm, 13);
    g_pSquirrel[context]->pushstring(sqvm, entry.pageUrl.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 14);
    g_pSquirrel[context]->pushbool(sqvm, entry.hasDownload && entry.approved && !entry.suspended && !entry.disableModManagers);
    g_pSquirrel[context]->sealstructslot(sqvm, 15);
    g_pSquirrel[context]->pushstring(sqvm, entry.selectedFileId.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 16);
    g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(entry.source));
    g_pSquirrel[context]->sealstructslot(sqvm, 17);
    g_pSquirrel[context]->pushstring(sqvm, entry.thumbnailFallbackUrl.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 18);
    g_pSquirrel[context]->pushstring(sqvm, entry.thumbnailVersion.c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 19);
}

template <ScriptContext context> void CModBrowserSquirrel::PushPageSnapshot(HSQUIRRELVM sqvm)
{
    const auto snapshot = CModBrowserService::Get().GetPageSnapshot();
    const auto operation = CModInstallService::Get().GetSnapshot();
    const std::string operationId = OperationId(operation.get());
    g_pSquirrel[context]->pushnewstructinstance(sqvm, 11);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->state) : static_cast<int>(ModBrowserLoadState::Idle));
    g_pSquirrel[context]->sealstructslot(sqvm, 0);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? SquirrelInteger(snapshot->generation) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 1);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->search.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 2);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->sort.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 3);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? snapshot->requestedPage : 1);
    g_pSquirrel[context]->sealstructslot(sqvm, 4);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot && snapshot->page ? snapshot->page->metadata.currentPage : 1);
    g_pSquirrel[context]->sealstructslot(sqvm, 5);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot && snapshot->page ? snapshot->page->metadata.lastPage : 1);
    g_pSquirrel[context]->sealstructslot(sqvm, 6);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot && snapshot->page ? snapshot->page->metadata.total : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 7);
    g_pSquirrel[context]->pushbool(sqvm, snapshot && snapshot->fromCache);
    g_pSquirrel[context]->sealstructslot(sqvm, 8);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->error.message.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 9);
    g_pSquirrel[context]->newarray(sqvm, 0);
    if (snapshot && snapshot->page)
    {
        for (const ModBrowserEntry& entry : snapshot->page->entries)
        {
            PushCatalogEntry<context>(sqvm, entry, operation.get(), operationId);
            g_pSquirrel[context]->arrayappend(sqvm, -2);
        }
    }
    g_pSquirrel[context]->sealstructslot(sqvm, 10);
}

template <ScriptContext context> void CModBrowserSquirrel::PushDetailsSnapshot(HSQUIRRELVM sqvm)
{
    const auto snapshot = CModBrowserService::Get().GetDetailsSnapshot();
    const ModBrowserDetails* details = snapshot && snapshot->details ? snapshot->details.get() : nullptr;
    g_pSquirrel[context]->pushnewstructinstance(sqvm, 21);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->state) : static_cast<int>(ModBrowserLoadState::Idle));
    g_pSquirrel[context]->sealstructslot(sqvm, 0);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? SquirrelInteger(snapshot->generation) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 1);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->id.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 2);
    g_pSquirrel[context]->pushstring(sqvm, details ? details->name.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 3);
    g_pSquirrel[context]->pushstring(sqvm, details ? details->author.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 4);
    g_pSquirrel[context]->pushstring(sqvm, details ? details->description.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 5);
    g_pSquirrel[context]->pushstring(sqvm, details ? details->version.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 6);
    g_pSquirrel[context]->pushstring(sqvm, details ? details->selectedFileId.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 7);
    g_pSquirrel[context]->pushstring(sqvm, details && details->selectedFileSize ? std::to_string(details->selectedFileSize).c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 8);
    g_pSquirrel[context]->pushstring(sqvm, details ? details->selectedFileUpdatedAt.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 9);
    g_pSquirrel[context]->pushinteger(sqvm, details ? SquirrelInteger(details->downloads) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 10);
    g_pSquirrel[context]->pushinteger(sqvm, details ? SquirrelInteger(details->likes) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 11);
    g_pSquirrel[context]->pushinteger(sqvm, details ? SquirrelInteger(details->views) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 12);
    g_pSquirrel[context]->pushbool(sqvm, details && details->installed);
    g_pSquirrel[context]->sealstructslot(sqvm, 13);
    g_pSquirrel[context]->pushinteger(sqvm, details ? details->updateState : static_cast<int>(ModUpdateState::LegacyUnknown));
    g_pSquirrel[context]->sealstructslot(sqvm, 14);
    g_pSquirrel[context]->pushbool(sqvm, details && details->hasDownload && details->approved && !details->suspended && !details->disableModManagers);
    g_pSquirrel[context]->sealstructslot(sqvm, 15);
    g_pSquirrel[context]->pushstring(sqvm, details ? details->pageUrl.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 16);
    g_pSquirrel[context]->newarray(sqvm, 0);
    if (details)
    {
        for (const std::string& dependency : details->dependencies)
        {
            g_pSquirrel[context]->pushstring(sqvm, dependency.c_str());
            g_pSquirrel[context]->arrayappend(sqvm, -2);
        }
    }
    g_pSquirrel[context]->sealstructslot(sqvm, 17);
    g_pSquirrel[context]->pushbool(sqvm, snapshot && snapshot->fromCache);
    g_pSquirrel[context]->sealstructslot(sqvm, 18);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->error.message.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 19);
    g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(details ? details->source : ModSource::Unknown));
    g_pSquirrel[context]->sealstructslot(sqvm, 20);
}

template <ScriptContext context> void CModBrowserSquirrel::PushOperationSnapshot(HSQUIRRELVM sqvm)
{
    const auto snapshot = CModInstallService::Get().GetSnapshot();
    g_pSquirrel[context]->pushnewstructinstance(sqvm, 11);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? SquirrelInteger(snapshot->generation) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 0);
    g_pSquirrel[context]->pushstring(sqvm, OperationId(snapshot.get()).c_str());
    g_pSquirrel[context]->sealstructslot(sqvm, 1);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->action) : static_cast<int>(ModInstallAction::Install));
    g_pSquirrel[context]->sealstructslot(sqvm, 2);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->state) : static_cast<int>(ModInstallOperationState::Idle));
    g_pSquirrel[context]->sealstructslot(sqvm, 3);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->name.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 4);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->version.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 5);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->message.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 6);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? SquirrelInteger(snapshot->progress) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 7);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? SquirrelInteger(snapshot->total) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 8);
    g_pSquirrel[context]->pushfloat(sqvm, snapshot ? snapshot->ratio : 0.0f);
    g_pSquirrel[context]->sealstructslot(sqvm, 9);
    g_pSquirrel[context]->pushbool(sqvm, snapshot && snapshot->cancellationDeferred);
    g_pSquirrel[context]->sealstructslot(sqvm, 10);
}

template <ScriptContext context> void CModBrowserSquirrel::PushInventorySnapshot(HSQUIRRELVM sqvm)
{
    const auto snapshot = CModBrowserService::Get().GetInventorySnapshot();
    g_pSquirrel[context]->pushnewstructinstance(sqvm, 6);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? SquirrelInteger(snapshot->generation) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 0);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? snapshot->updateCount : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 1);
    g_pSquirrel[context]->pushbool(sqvm, snapshot && snapshot->checking);
    g_pSquirrel[context]->sealstructslot(sqvm, 2);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->checkedAt.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 3);
    g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->error.c_str() : "");
    g_pSquirrel[context]->sealstructslot(sqvm, 4);
    g_pSquirrel[context]->pushinteger(sqvm, snapshot ? SquirrelInteger(snapshot->packages.size()) : 0);
    g_pSquirrel[context]->sealstructslot(sqvm, 5);
}

template <ScriptContext context> SQRESULT CModBrowserSquirrel::RequestOperation(HSQUIRRELVM sqvm, ModInstallAction action)
{
    EnsureCallbacks();
    const SQChar* id = g_pSquirrel[context]->getstring(sqvm, 1);
    g_pSquirrel[context]->pushbool(sqvm, CModBrowserService::Get().RequestOperation(action, id ? id : ""));
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", NSModBrowserRequestPage, "string search, string sort, int page, int filter, bool forceRefresh, int source",
           "Requests a cancellable mod catalog page.", ScriptContext::UI)
{
    CModBrowserSquirrel::EnsureCallbacks();
    const SQChar* search = g_pSquirrel[context]->getstring(sqvm, 1);
    const SQChar* sort = g_pSquirrel[context]->getstring(sqvm, 2);
    const int page = static_cast<int>(g_pSquirrel[context]->getinteger(sqvm, 3));
    const int filter = static_cast<int>(g_pSquirrel[context]->getinteger(sqvm, 4));
    const bool refresh = g_pSquirrel[context]->getbool(sqvm, 5);
    const auto source = static_cast<ModSource>(g_pSquirrel[context]->getinteger(sqvm, 6));
    const uint64_t generation = CModBrowserService::Get().RequestPage(search ? search : "", sort ? sort : "bumped_at", page, filter, source, refresh);
    g_pSquirrel[context]->pushinteger(sqvm, CModBrowserSquirrel::SquirrelInteger(generation));
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("ModBrowserPageSnapshot", NSModBrowserGetPage, "", "Returns the current catalog snapshot.", ScriptContext::UI)
{
    CModBrowserSquirrel::EnsureCallbacks();
    CModBrowserSquirrel::PushPageSnapshot<context>(sqvm);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSModBrowserCancelPage, "", "Cancels the active catalog request.", ScriptContext::UI)
{
    NOTE_UNUSED(sqvm);
    CModBrowserService::Get().CancelPageRequest();
    return SQRESULT_NULL;
}

ADD_SQFUNC("int", NSModBrowserRequestDetails, "string modId, bool forceRefresh", "Requests details for a source-qualified mod ID.", ScriptContext::UI)
{
    CModBrowserSquirrel::EnsureCallbacks();
    const SQChar* id = g_pSquirrel[context]->getstring(sqvm, 1);
    const uint64_t generation = CModBrowserService::Get().RequestDetails(id ? id : "", g_pSquirrel[context]->getbool(sqvm, 2));
    g_pSquirrel[context]->pushinteger(sqvm, CModBrowserSquirrel::SquirrelInteger(generation));
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("ModBrowserDetailsSnapshot", NSModBrowserGetDetails, "", "Returns the current details snapshot.", ScriptContext::UI)
{
    CModBrowserSquirrel::EnsureCallbacks();
    CModBrowserSquirrel::PushDetailsSnapshot<context>(sqvm);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSModBrowserCancelDetails, "", "Cancels the active details request.", ScriptContext::UI)
{
    NOTE_UNUSED(sqvm);
    CModBrowserService::Get().CancelDetailsRequest();
    return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSModBrowserInstall, "string modId", "Installs a catalog mod.", ScriptContext::UI)
{
    return CModBrowserSquirrel::RequestOperation<context>(sqvm, ModInstallAction::Install);
}

ADD_SQFUNC("bool", NSModBrowserUpdate, "string modId", "Updates a managed mod.", ScriptContext::UI)
{
    return CModBrowserSquirrel::RequestOperation<context>(sqvm, ModInstallAction::Update);
}

ADD_SQFUNC("bool", NSModBrowserRemove, "string modId", "Removes a managed mod.", ScriptContext::UI)
{
    return CModBrowserSquirrel::RequestOperation<context>(sqvm, ModInstallAction::Remove);
}

ADD_SQFUNC("ModBrowserOperationSnapshot", NSModBrowserGetOperationState, "", "Returns the synchronized install operation snapshot.",
           ScriptContext::UI)
{
    CModBrowserSquirrel::EnsureCallbacks();
    CModBrowserSquirrel::PushOperationSnapshot<context>(sqvm);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSModBrowserDecideMigration, "int generation, bool accept", "Accepts or declines replacement of an existing package.",
           ScriptContext::UI)
{
    const uint64_t generation = static_cast<uint64_t>(std::max<SQInteger>(0, g_pSquirrel[context]->getinteger(sqvm, 1)));
    g_pSquirrel[context]->pushbool(sqvm, CModInstallService::Get().DecideMigration(generation, g_pSquirrel[context]->getbool(sqvm, 2)));
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSModBrowserCancelOperation, "", "Cancels the active install operation.", ScriptContext::UI)
{
    NOTE_UNUSED(sqvm);
    CModInstallService::Get().Cancel();
    return SQRESULT_NULL;
}

ADD_SQFUNC("int", NSModBrowserRefreshTrackedMods, "bool checkRemote", "Refreshes local packages and optionally checks for updates.",
           ScriptContext::UI)
{
    CModBrowserSquirrel::EnsureCallbacks();
    const uint64_t generation = CModBrowserService::Get().RefreshTrackedMods(g_pSquirrel[context]->getbool(sqvm, 1));
    g_pSquirrel[context]->pushinteger(sqvm, CModBrowserSquirrel::SquirrelInteger(generation));
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("ModBrowserInventorySnapshot", NSModBrowserGetInventoryState, "", "Returns the managed-package inventory snapshot.", ScriptContext::UI)
{
    CModBrowserSquirrel::EnsureCallbacks();
    CModBrowserSquirrel::PushInventorySnapshot<context>(sqvm);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSModBrowserOpenPage, "string modId", "Opens a catalog mod's provider page.", ScriptContext::UI)
{
    const SQChar* id = g_pSquirrel[context]->getstring(sqvm, 1);
    const std::string url = CModBrowserService::BuildPageUrl(id ? id : "");
    const bool opened = !url.empty() && reinterpret_cast<intptr_t>(ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    g_pSquirrel[context]->pushbool(sqvm, opened);
    return SQRESULT_NOTNULL;
}

