#pragma once

// Stops material-bound shader watchers and releases their shader overrides.
// Must be called on the main thread before map materials are unloaded.
void StopFXCAndHotReloadWatchers();
