#include "modsystem/modinstaller.h"
#include "modsystem/modworkshop_inventory.h"
#include "modsystem/modworkshop_service.h"
#include "modsystem/platform/modworkshop.h"
#include "rtech/rui/workshop_thumbnail_service.h"
#include "vscript/squirrel/squirrel.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <charconv>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class CModWorkshopSquirrel final
{
public:
	static int SquirrelInteger(uint64_t value);
	static std::string IdString(uint64_t value);
	static bool ParseModId(std::string_view text, uint64_t& modId);
	static const ModWorkshopTrackedPackage* FindTrackedPackage(const ModWorkshopInventorySnapshot* inventory, uint64_t modId);
	static void EnsureCallbacks();

	template <ScriptContext context>
	static void PushCatalogEntry(HSQUIRRELVM sqvm, const ModWorkshopCatalogEntry& entry, int atlasSlot, const ModWorkshopInventorySnapshot* inventory,
	                             const ModInstallOperationSnapshot* operation);
	template <ScriptContext context> static void PushPageSnapshot(HSQUIRRELVM sqvm);
	template <ScriptContext context> static void PushDetailsSnapshot(HSQUIRRELVM sqvm);
	template <ScriptContext context> static void PushOperationSnapshot(HSQUIRRELVM sqvm);
	template <ScriptContext context> static void PushInventorySnapshot(HSQUIRRELVM sqvm);
	template <ScriptContext context> static SQRESULT RequestOperation(HSQUIRRELVM sqvm, ModInstallAction action);

private:
	static void OnPageChanged(uint64_t generation);
	static void OnDetailsChanged(uint64_t modId);
	static void OnThumbnailReady(uint64_t generation, uint64_t modId, size_t slot);
	static void OnUpdatesChanged(uint64_t generation, int updateCount, ModWorkshopInventoryUpdateStage stage);
	static void OnOperationChanged();

	inline static std::mutex s_CallbackMutex;
	inline static bool s_CallbacksInitialized = false;
};

int CModWorkshopSquirrel::SquirrelInteger(uint64_t value)
{
	return static_cast<int>(std::min<uint64_t>(value, std::numeric_limits<int>::max()));
}

std::string CModWorkshopSquirrel::IdString(uint64_t value)
{
	return value == 0 ? std::string() : std::to_string(value);
}

bool CModWorkshopSquirrel::ParseModId(std::string_view text, uint64_t& modId)
{
	modId = 0;
	if (text.empty())
		return false;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), modId);
	return error == std::errc() && end == text.data() + text.size() && modId != 0;
}

const ModWorkshopTrackedPackage* CModWorkshopSquirrel::FindTrackedPackage(const ModWorkshopInventorySnapshot* inventory, uint64_t modId)
{
	if (!inventory)
		return nullptr;
	const auto found = std::ranges::find(inventory->packages, modId, &ModWorkshopTrackedPackage::modId);
	return found == inventory->packages.end() ? nullptr : &*found;
}

void CModWorkshopSquirrel::OnPageChanged(uint64_t generation)
{
	SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
	if (squirrel && squirrel->m_pSQVM)
		squirrel->AsyncCall("NSUICodeCallback_ModWorkshopPageChanged", CModWorkshopSquirrel::SquirrelInteger(generation));
}

void CModWorkshopSquirrel::OnDetailsChanged(uint64_t modId)
{
	SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
	if (squirrel && squirrel->m_pSQVM)
		squirrel->AsyncCall("NSUICodeCallback_ModWorkshopDetailsChanged", CModWorkshopSquirrel::IdString(modId));
}

void CModWorkshopSquirrel::OnThumbnailReady(uint64_t generation, uint64_t modId, size_t slot)
{
	SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
	if (squirrel && squirrel->m_pSQVM)
		squirrel->AsyncCall("NSUICodeCallback_ModWorkshopThumbnailReady", CModWorkshopSquirrel::SquirrelInteger(generation),
		                    CModWorkshopSquirrel::IdString(modId), static_cast<int>(slot));
}

void CModWorkshopSquirrel::OnUpdatesChanged(uint64_t generation, int updateCount, ModWorkshopInventoryUpdateStage stage)
{
	SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
	if (squirrel && squirrel->m_pSQVM)
		squirrel->AsyncCall("NSUICodeCallback_ModWorkshopUpdatesChanged", CModWorkshopSquirrel::SquirrelInteger(generation), updateCount,
		                    static_cast<int>(stage));
}

void CModWorkshopSquirrel::OnOperationChanged()
{
	SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
	if (squirrel && squirrel->m_pSQVM)
		squirrel->AsyncCall("NSUICodeCallback_ModWorkshopOperationChanged");
}

void CModWorkshopSquirrel::EnsureCallbacks()
{
	std::scoped_lock lock(s_CallbackMutex);
	if (s_CallbacksInitialized)
		return;

	CModWorkshopService::Get().SetPageChangedCallback(OnPageChanged);
	CModWorkshopService::Get().SetDetailsChangedCallback(OnDetailsChanged);
	CWorkshopThumbnailService::Get().SetReadyCallback(OnThumbnailReady);
	CModWorkshopService::Get().SetUpdatesChangedCallback(OnUpdatesChanged);
	CModInstallService::Get().SetOperationChangedCallback(OnOperationChanged);
	s_CallbacksInitialized = true;
}

template <ScriptContext context>
void CModWorkshopSquirrel::PushCatalogEntry(HSQUIRRELVM sqvm, const ModWorkshopCatalogEntry& entry, int atlasSlot,
                                            const ModWorkshopInventorySnapshot* inventory, const ModInstallOperationSnapshot* operation)
{
	const ModWorkshopTrackedPackage* package = CModWorkshopSquirrel::FindTrackedPackage(inventory, entry.id);
	const bool operationMatches = operation && operation->modId == entry.id;
	g_pSquirrel[context]->pushnewstructinstance(sqvm, 17);
	g_pSquirrel[context]->pushstring(sqvm, CModWorkshopSquirrel::IdString(entry.id).c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 0);
	g_pSquirrel[context]->pushstring(sqvm, entry.name.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 1);
	g_pSquirrel[context]->pushstring(sqvm, entry.author.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 2);
	const std::string& summary = entry.shortDescription.empty() ? entry.description : entry.shortDescription;
	g_pSquirrel[context]->pushstring(sqvm, summary.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 3);
	g_pSquirrel[context]->pushstring(sqvm, entry.version.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 4);
	g_pSquirrel[context]->pushinteger(sqvm, CModWorkshopSquirrel::SquirrelInteger(entry.downloads));
	g_pSquirrel[context]->sealstructslot(sqvm, 5);
	g_pSquirrel[context]->pushinteger(sqvm, CModWorkshopSquirrel::SquirrelInteger(entry.likes));
	g_pSquirrel[context]->sealstructslot(sqvm, 6);
	g_pSquirrel[context]->pushinteger(sqvm, CModWorkshopSquirrel::SquirrelInteger(entry.views));
	g_pSquirrel[context]->sealstructslot(sqvm, 7);
	g_pSquirrel[context]->pushinteger(sqvm, atlasSlot);
	g_pSquirrel[context]->sealstructslot(sqvm, 8);
	g_pSquirrel[context]->pushbool(sqvm, package != nullptr);
	g_pSquirrel[context]->sealstructslot(sqvm, 9);
	g_pSquirrel[context]->pushinteger(sqvm,
	                                  package ? static_cast<int>(package->updateState) : static_cast<int>(ModWorkshopUpdateState::LegacyUnknown));
	g_pSquirrel[context]->sealstructslot(sqvm, 10);
	g_pSquirrel[context]->pushinteger(sqvm, operationMatches ? static_cast<int>(operation->state) : static_cast<int>(ModInstallOperationState::Idle));
	g_pSquirrel[context]->sealstructslot(sqvm, 11);
	g_pSquirrel[context]->pushbool(sqvm, entry.approved);
	g_pSquirrel[context]->sealstructslot(sqvm, 12);
	g_pSquirrel[context]->pushbool(sqvm, entry.suspended);
	g_pSquirrel[context]->sealstructslot(sqvm, 13);
	const std::string pageUrl = CModWorkshopClient::BuildModPageUrl(entry.id);
	g_pSquirrel[context]->pushstring(sqvm, pageUrl.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 14);
	g_pSquirrel[context]->pushbool(sqvm, entry.hasDownload && entry.approved && !entry.suspended && !entry.disableModManagers);
	g_pSquirrel[context]->sealstructslot(sqvm, 15);
	g_pSquirrel[context]->pushstring(sqvm, entry.selectedFileId ? CModWorkshopSquirrel::IdString(*entry.selectedFileId).c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 16);
}

template <ScriptContext context> void CModWorkshopSquirrel::PushPageSnapshot(HSQUIRRELVM sqvm)
{
	const std::shared_ptr<const ModWorkshopPageSnapshot> snapshot = CModWorkshopService::Get().GetPageSnapshot();
	const std::shared_ptr<const ModWorkshopInventorySnapshot> inventory = CModWorkshopService::Get().GetInventorySnapshot();
	const std::shared_ptr<const ModInstallOperationSnapshot> operation = CModInstallService::Get().GetSnapshot();

	g_pSquirrel[context]->pushnewstructinstance(sqvm, 11);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->state) : static_cast<int>(ModWorkshopLoadState::Idle));
	g_pSquirrel[context]->sealstructslot(sqvm, 0);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? CModWorkshopSquirrel::SquirrelInteger(snapshot->generation) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 1);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->search.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 2);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->sort.c_str() : "", -1);
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
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->error.message.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 9);
	g_pSquirrel[context]->newarray(sqvm, 0);
	if (snapshot && snapshot->page)
	{
		for (size_t slot = 0; slot < snapshot->page->entries.size(); ++slot)
		{
			CModWorkshopSquirrel::PushCatalogEntry<context>(sqvm, snapshot->page->entries[slot], static_cast<int>(slot), inventory.get(),
			                                                operation.get());
			g_pSquirrel[context]->arrayappend(sqvm, -2);
		}
	}
	g_pSquirrel[context]->sealstructslot(sqvm, 10);
}

template <ScriptContext context> void CModWorkshopSquirrel::PushDetailsSnapshot(HSQUIRRELVM sqvm)
{
	const std::shared_ptr<const ModWorkshopDetailsSnapshot> snapshot = CModWorkshopService::Get().GetDetailsSnapshot();
	const ModWorkshopDetails* details = snapshot && snapshot->details ? snapshot->details.get() : nullptr;
	const std::shared_ptr<const ModWorkshopInventorySnapshot> inventory = CModWorkshopService::Get().GetInventorySnapshot();
	const ModWorkshopTrackedPackage* package = details ? CModWorkshopSquirrel::FindTrackedPackage(inventory.get(), details->id) : nullptr;

	g_pSquirrel[context]->pushnewstructinstance(sqvm, 20);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->state) : static_cast<int>(ModWorkshopLoadState::Idle));
	g_pSquirrel[context]->sealstructslot(sqvm, 0);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? CModWorkshopSquirrel::SquirrelInteger(snapshot->generation) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 1);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? CModWorkshopSquirrel::IdString(snapshot->modId).c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 2);
	g_pSquirrel[context]->pushstring(sqvm, details ? details->name.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 3);
	g_pSquirrel[context]->pushstring(sqvm, details ? details->author.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 4);
	g_pSquirrel[context]->pushstring(sqvm, details ? details->description.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 5);
	g_pSquirrel[context]->pushstring(sqvm, details ? details->version.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 6);
	g_pSquirrel[context]->pushstring(sqvm, details && details->selectedFile ? CModWorkshopSquirrel::IdString(details->selectedFile->id).c_str() : "",
	                                 -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 7);
	g_pSquirrel[context]->pushstring(sqvm, details && details->selectedFile ? std::to_string(details->selectedFile->size).c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 8);
	g_pSquirrel[context]->pushstring(sqvm, details && details->selectedFile ? details->selectedFile->updatedAt.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 9);
	g_pSquirrel[context]->pushinteger(sqvm, details ? CModWorkshopSquirrel::SquirrelInteger(details->downloads) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 10);
	g_pSquirrel[context]->pushinteger(sqvm, details ? CModWorkshopSquirrel::SquirrelInteger(details->likes) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 11);
	g_pSquirrel[context]->pushinteger(sqvm, details ? CModWorkshopSquirrel::SquirrelInteger(details->views) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 12);
	g_pSquirrel[context]->pushbool(sqvm, package != nullptr);
	g_pSquirrel[context]->sealstructslot(sqvm, 13);
	g_pSquirrel[context]->pushinteger(sqvm,
	                                  package ? static_cast<int>(package->updateState) : static_cast<int>(ModWorkshopUpdateState::LegacyUnknown));
	g_pSquirrel[context]->sealstructslot(sqvm, 14);
	g_pSquirrel[context]->pushbool(sqvm, details && details->hasDownload && details->approved && !details->suspended && !details->disableModManagers);
	g_pSquirrel[context]->sealstructslot(sqvm, 15);
	const std::string pageUrl = details ? CModWorkshopClient::BuildModPageUrl(details->id) : std::string();
	g_pSquirrel[context]->pushstring(sqvm, pageUrl.c_str(), -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 16);
	g_pSquirrel[context]->newarray(sqvm, 0);
	if (details)
	{
		for (const ModWorkshopDependency& dependency : details->dependencies)
		{
			const std::string label =
			    dependency.name.empty() ? (dependency.modId ? CModWorkshopSquirrel::IdString(*dependency.modId) : dependency.url) : dependency.name;
			g_pSquirrel[context]->pushstring(sqvm, label.c_str(), -1);
			g_pSquirrel[context]->arrayappend(sqvm, -2);
		}
	}
	g_pSquirrel[context]->sealstructslot(sqvm, 17);
	g_pSquirrel[context]->pushbool(sqvm, snapshot && snapshot->fromCache);
	g_pSquirrel[context]->sealstructslot(sqvm, 18);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->error.message.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 19);
}

template <ScriptContext context> void CModWorkshopSquirrel::PushOperationSnapshot(HSQUIRRELVM sqvm)
{
	const std::shared_ptr<const ModInstallOperationSnapshot> snapshot = CModInstallService::Get().GetSnapshot();
	g_pSquirrel[context]->pushnewstructinstance(sqvm, 11);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? CModWorkshopSquirrel::SquirrelInteger(snapshot->generation) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 0);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? CModWorkshopSquirrel::IdString(snapshot->modId).c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 1);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->action) : static_cast<int>(ModInstallAction::Install));
	g_pSquirrel[context]->sealstructslot(sqvm, 2);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->state) : static_cast<int>(ModInstallOperationState::Idle));
	g_pSquirrel[context]->sealstructslot(sqvm, 3);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->name.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 4);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->version.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 5);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->message.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 6);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? CModWorkshopSquirrel::SquirrelInteger(snapshot->progress) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 7);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? CModWorkshopSquirrel::SquirrelInteger(snapshot->total) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 8);
	g_pSquirrel[context]->pushfloat(sqvm, snapshot ? snapshot->ratio : 0.0f);
	g_pSquirrel[context]->sealstructslot(sqvm, 9);
	g_pSquirrel[context]->pushbool(sqvm, snapshot && snapshot->cancellationDeferred);
	g_pSquirrel[context]->sealstructslot(sqvm, 10);
}

template <ScriptContext context> void CModWorkshopSquirrel::PushInventorySnapshot(HSQUIRRELVM sqvm)
{
	const std::shared_ptr<const ModWorkshopInventorySnapshot> snapshot = CModWorkshopService::Get().GetInventorySnapshot();
	g_pSquirrel[context]->pushnewstructinstance(sqvm, 6);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? CModWorkshopSquirrel::SquirrelInteger(snapshot->generation) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 0);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? snapshot->updateCount : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 1);
	g_pSquirrel[context]->pushbool(sqvm, snapshot && snapshot->checking);
	g_pSquirrel[context]->sealstructslot(sqvm, 2);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->checkedAt.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 3);
	g_pSquirrel[context]->pushstring(sqvm, snapshot ? snapshot->error.c_str() : "", -1);
	g_pSquirrel[context]->sealstructslot(sqvm, 4);
	g_pSquirrel[context]->pushinteger(sqvm, snapshot ? static_cast<int>(snapshot->packages.size()) : 0);
	g_pSquirrel[context]->sealstructslot(sqvm, 5);
}

template <ScriptContext context> SQRESULT CModWorkshopSquirrel::RequestOperation(HSQUIRRELVM sqvm, ModInstallAction action)
{
	uint64_t modId = 0;
	const SQChar* id = g_pSquirrel[context]->getstring(sqvm, 1);
	const bool accepted = CModWorkshopSquirrel::ParseModId(id ? id : "", modId) && CModInstallService::Get().Request(action, modId);
	g_pSquirrel[context]->pushbool(sqvm, accepted);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", NSMWSRequestPage, "string search, string sort, int page, int filter, bool forceRefresh",
           "Requests a cancellable ModWorkshop catalog page.", ScriptContext::UI)
{
	CModWorkshopSquirrel::EnsureCallbacks();
	const SQChar* search = g_pSquirrel[context]->getstring(sqvm, 1);
	const SQChar* sort = g_pSquirrel[context]->getstring(sqvm, 2);
	const int page = std::max(1, static_cast<int>(g_pSquirrel[context]->getinteger(sqvm, 3)));
	const int filter = static_cast<int>(g_pSquirrel[context]->getinteger(sqvm, 4));
	const bool forceRefresh = g_pSquirrel[context]->getbool(sqvm, 5);
	std::optional<std::vector<uint64_t>> restrictedIds;
	std::optional<std::vector<uint64_t>> excludedIds;
	const std::shared_ptr<const ModWorkshopInventorySnapshot> inventory = CModWorkshopService::Get().GetInventorySnapshot();
	if (filter == 0)
	{
		if (inventory)
		{
			excludedIds.emplace();
			for (const ModWorkshopTrackedPackage& package : inventory->packages)
			{
				if (package.modId != 0)
					excludedIds->push_back(package.modId);
			}
			if (excludedIds->empty())
				excludedIds.reset();
		}
	}
	else if (filter == 1 || filter == 2)
	{
		restrictedIds.emplace();
		if (inventory)
		{
			for (const ModWorkshopTrackedPackage& package : inventory->packages)
			{
				if (package.modId != 0 && (filter == 1 || package.updateState == ModWorkshopUpdateState::UpdateAvailable))
					restrictedIds->push_back(package.modId);
			}
		}
	}
	const uint64_t generation = CModWorkshopService::Get().RequestPage(search ? search : "", sort ? sort : "bumped_at", page, forceRefresh,
	                                                                   std::move(restrictedIds), std::move(excludedIds));
	g_pSquirrel[context]->pushinteger(sqvm, CModWorkshopSquirrel::SquirrelInteger(generation));
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("MWSPageSnapshot", NSMWSGetPage, "", "Returns an immutable snapshot of the current catalog request.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CModWorkshopSquirrel::EnsureCallbacks();
	CModWorkshopSquirrel::PushPageSnapshot<context>(sqvm);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSMWSIsThumbnailReady, "int generation, string modId, int atlasSlot",
           "Returns whether a catalog thumbnail has been uploaded to its current atlas slot.", ScriptContext::UI)
{
	const int generation = static_cast<int>(g_pSquirrel[context]->getinteger(sqvm, 1));
	const SQChar* id = g_pSquirrel[context]->getstring(sqvm, 2);
	const int slot = static_cast<int>(g_pSquirrel[context]->getinteger(sqvm, 3));
	uint64_t modId = 0;
	const bool ready = generation > 0 && slot >= 0 && CModWorkshopSquirrel::ParseModId(id ? id : "", modId) &&
	                   CWorkshopThumbnailService::Get().IsReady(static_cast<uint64_t>(generation), modId, static_cast<size_t>(slot));
	g_pSquirrel[context]->pushbool(sqvm, ready);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSMWSCancelPage, "", "Cancels the active catalog request.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CModWorkshopService::Get().CancelPageRequest();
	return SQRESULT_NULL;
}

ADD_SQFUNC("int", NSMWSRequestDetails, "string modId, bool forceRefresh", "Requests complete details for one ModWorkshop mod.", ScriptContext::UI)
{
	CModWorkshopSquirrel::EnsureCallbacks();
	uint64_t modId = 0;
	const SQChar* id = g_pSquirrel[context]->getstring(sqvm, 1);
	const bool valid = CModWorkshopSquirrel::ParseModId(id ? id : "", modId);
	const uint64_t generation = valid ? CModWorkshopService::Get().RequestDetails(modId, g_pSquirrel[context]->getbool(sqvm, 2)) : 0;
	g_pSquirrel[context]->pushinteger(sqvm, CModWorkshopSquirrel::SquirrelInteger(generation));
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("MWSDetailsSnapshot", NSMWSGetDetails, "", "Returns an immutable snapshot of the current details request.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CModWorkshopSquirrel::EnsureCallbacks();
	CModWorkshopSquirrel::PushDetailsSnapshot<context>(sqvm);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSMWSCancelDetails, "", "Cancels the active details request.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CModWorkshopService::Get().CancelDetailsRequest();
	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSMWSInstall, "string modId", "Installs a ModWorkshop mod.", ScriptContext::UI)
{
	CModWorkshopSquirrel::EnsureCallbacks();
	return CModWorkshopSquirrel::RequestOperation<context>(sqvm, ModInstallAction::Install);
}

ADD_SQFUNC("bool", NSMWSUpdate, "string modId", "Updates a ModWorkshop mod.", ScriptContext::UI)
{
	CModWorkshopSquirrel::EnsureCallbacks();
	return CModWorkshopSquirrel::RequestOperation<context>(sqvm, ModInstallAction::Update);
}

ADD_SQFUNC("bool", NSMWSRemove, "string modId", "Removes a managed ModWorkshop mod.", ScriptContext::UI)
{
	CModWorkshopSquirrel::EnsureCallbacks();
	return CModWorkshopSquirrel::RequestOperation<context>(sqvm, ModInstallAction::Remove);
}

ADD_SQFUNC("MWSOperationSnapshot", NSMWSGetOperationState, "", "Returns the synchronized install operation snapshot.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CModWorkshopSquirrel::EnsureCallbacks();
	CModWorkshopSquirrel::PushOperationSnapshot<context>(sqvm);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSMWSDecideMigration, "int generation, bool accept",
           "Accepts or declines replacement of an unmanaged package during installation.", ScriptContext::UI)
{
	const uint64_t generation = static_cast<uint64_t>(std::max<SQInteger>(0, g_pSquirrel[context]->getinteger(sqvm, 1)));
	const bool accepted = CModInstallService::Get().DecideMigration(generation, g_pSquirrel[context]->getbool(sqvm, 2));
	g_pSquirrel[context]->pushbool(sqvm, accepted);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSMWSCancelOperation, "", "Cancels the active install operation.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CModInstallService::Get().Cancel();
	return SQRESULT_NULL;
}

ADD_SQFUNC("int", NSMWSRefreshTrackedMods, "bool checkRemote", "Refreshes local tracking and optionally checks selected file IDs remotely.",
           ScriptContext::UI)
{
	CModWorkshopSquirrel::EnsureCallbacks();
	const uint64_t generation = CModWorkshopService::Get().RefreshTrackedMods(g_pSquirrel[context]->getbool(sqvm, 1));
	g_pSquirrel[context]->pushinteger(sqvm, CModWorkshopSquirrel::SquirrelInteger(generation));
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("MWSInventorySnapshot", NSMWSGetInventoryState, "", "Returns the synchronized managed-package inventory snapshot.", ScriptContext::UI)
{
	NOTE_UNUSED(sqvm);
	CModWorkshopSquirrel::EnsureCallbacks();
	CModWorkshopSquirrel::PushInventorySnapshot<context>(sqvm);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSMWSOpenPage, "string modId", "Opens the ModWorkshop web page.", ScriptContext::UI)
{
	uint64_t modId = 0;
	const SQChar* id = g_pSquirrel[context]->getstring(sqvm, 1);
	bool opened = false;
	if (CModWorkshopSquirrel::ParseModId(id ? id : "", modId))
	{
		const std::string url = CModWorkshopClient::BuildModPageUrl(modId);
		opened = reinterpret_cast<intptr_t>(ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
	}
	g_pSquirrel[context]->pushbool(sqvm, opened);
	return SQRESULT_NOTNULL;
}
