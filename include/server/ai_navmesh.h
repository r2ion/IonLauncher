#pragma once

class dtNavMesh;

// Borrow the current local-server graph: 1 small, 2 med_short, 3 medium, 4 large.
// Invalid hulls and unloaded maps return nullptr. Do not retain across frames.
const dtNavMesh* GetNavMeshForHull(int nHull);
