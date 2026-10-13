#pragma once
#include "CTextureManager.h"

// Construct on the render owner, before starting decoder workers.
CTextureManager::Backend makeEngineTextureBackend();
