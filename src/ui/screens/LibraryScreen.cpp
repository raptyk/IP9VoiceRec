// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Patryk Strzelecki <patryk@strzelecki.info>
#include "LibraryScreen.h"

#include "app/App.h"
#include "audio/Mp3Info.h"
#include "ui/Painter.h"
#include "ui/Strings.h"
#include "ui/screens/MessageScreens.h"
#include "ui/screens/PlayerScreen.h"

namespace voicerec {

namespace {
String withoutExtension(const String& name) {
    const int dot = name.lastIndexOf('.');
    return dot > 0 ? name.substring(0, dot) : name;
}
}  // namespace

LibraryScreen::LibraryScreen(App& app) : ListScreen(app, str::kLibraryTitle) {}

void LibraryScreen::onEnter() {
    reload();
}

void LibraryScreen::reload() {
    files_ = SdStorage::instance().listRecordings();
    durations_.assign(files_.size(), -1);

    std::vector<ListItem> items;
    items.reserve(files_.size());
    for (size_t i = 0; i < files_.size(); ++i) {
        ListItem item;
        item.label = withoutExtension(files_[i].name);
        item.value = [this, i]() { return duration(i); };
        item.onSelect = [this, i]() {
            app_.push(std::make_unique<PlayerScreen>(app_, files_, static_cast<int>(i)));
        };
        char description[64];
        snprintf(description, sizeof(description), str::kLibraryDetail,
                 painter::formatBytes(files_[i].sizeBytes).c_str());
        item.description = description;
        items.push_back(std::move(item));
    }
    setItems(std::move(items));
}

String LibraryScreen::duration(size_t index) {
    if (index >= durations_.size()) {
        return String();
    }
    if (durations_[index] < 0) {
        File file = SdStorage::instance().openForRead(files_[index].path);
        durations_[index] = 0;
        if (file) {
            durations_[index] = static_cast<int32_t>(readMp3Info(file).durationMs);
            file.close();
        }
    }
    return painter::formatDuration(durations_[index]);
}

void LibraryScreen::onKey(const KeyEvent& event) {
    if (!event.repeat) {
        if (event.code == 'd' && !files_.empty()) {
            confirmDelete(selected_);
            return;
        }
        if (event.code == 'r') {
            app_.startRecording();
            return;
        }
    }
    ListScreen::onKey(event);
}

void LibraryScreen::confirmDelete(size_t index) {
    const RecordingFile file = files_[index];
    char question[64];
    snprintf(question, sizeof(question), str::kConfirmDelete, withoutExtension(file.name).c_str());
    // The list stays on the stack below the question, so `this` is still there afterwards.
    app_.push(std::make_unique<ConfirmScreen>(app_, question, str::kConfirmDeleteDetail, [this, file]() {
        if (SdStorage::instance().remove(file.path)) {
            app_.toast(str::kDeleted);
        } else {
            app_.warn(str::kDeleteFailed);
        }
        reload();
    }));
}

const char* LibraryScreen::footer() const {
    return files_.empty() ? str::kFooterLibraryEmpty : str::kFooterLibrary;
}

void LibraryScreen::drawEmpty(M5Canvas& canvas, int top, int height) {
    if (!SdStorage::instance().mounted()) {
        painter::drawCenteredLines(canvas, top, height, str::kInsertCard);
        return;
    }
    painter::drawCenteredLines(canvas, top, height, str::kLibraryEmptyTitle, str::kLibraryEmptyText);
}

}  // namespace voicerec
