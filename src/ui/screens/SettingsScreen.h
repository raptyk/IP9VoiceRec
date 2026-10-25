// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#pragma once

#include <memory>

#include "ui/Screen.h"

namespace voicerec {

// Settings list: , / change a value, enter toggles or opens; every row explains itself.
std::unique_ptr<ListScreen> makeSettingsScreen(App& app);

}  // namespace voicerec
