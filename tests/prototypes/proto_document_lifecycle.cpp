// Document lifecycle regression tests.
//
// Guards the file-open document swap: listeners holding a pointer to the old
// Document (AutosaveManager) must survive the old object's destruction without
// touching freed memory. Run under ASan to make violations hard failures.

#include "app/AutosaveManager.h"
#include "app/document/Document.h"

#include <QCoreApplication>
#include <QMetaObject>

#include <cstdio>
#include <memory>

using onecad::app::AutosaveManager;
using onecad::app::Document;

namespace {

bool check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

// The pre-fix crash: open a document (setDocument A), open another file — the
// owner destroys A, then calls setDocument(B). The old sender-form disconnect
// walked A's freed connection list.
bool testSetDocumentAfterDocumentDestroyed() {
    std::printf("Test: setDocument after old document destroyed...");

    AutosaveManager autosave;
    autosave.setSaveFunction([](const QString&, const Document*) { return true; });

    auto docA = std::make_unique<Document>();
    autosave.setDocument(docA.get());
    docA->setModified(true);

    docA.reset();  // owner replaces the document; listener not yet updated

    auto docB = std::make_unique<Document>();
    autosave.setDocument(docB.get());  // must not touch freed docA
    docB->setModified(true);

    // Force an autosave pass against the new document.
    QMetaObject::invokeMethod(&autosave, "performAutosave", Qt::DirectConnection);

    std::printf(" PASS\n");
    return true;
}

// Autosave firing while the document pointer is dead must be a no-op.
bool testAutosaveWithDestroyedDocumentIsNoop() {
    std::printf("Test: autosave tick with destroyed document is a no-op...");

    bool saveCalled = false;
    AutosaveManager autosave;
    autosave.setSaveFunction([&saveCalled](const QString&, const Document*) {
        saveCalled = true;
        return true;
    });

    auto doc = std::make_unique<Document>();
    autosave.setDocument(doc.get());
    doc->setModified(true);
    doc.reset();  // QPointer auto-nulls

    QMetaObject::invokeMethod(&autosave, "performAutosave", Qt::DirectConnection);

    if (!check(!saveCalled, "autosave ran against a destroyed document")) {
        return false;
    }

    std::printf(" PASS\n");
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::printf("\n=== Document lifecycle tests ===\n\n");

    bool ok = true;
    ok = testSetDocumentAfterDocumentDestroyed() && ok;
    ok = testAutosaveWithDestroyedDocumentIsNoop() && ok;

    if (!ok) {
        return 1;
    }
    std::printf("\n=== All tests passed! ===\n\n");
    return 0;
}
