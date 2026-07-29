# project_session

`ProjectSession` is Bess's project-scoped subsystem. It is also a subsystem
container, so ownership and shutdown order are explicit:

```text
GAppContext
└── ProjectSession
    ├── SceneDriver
    ├── SimulationEngine
    ├── CopyPaste::Context
    ├── SvcConnection
    └── ProjectDoc
```

`SceneDriver` and `SimulationEngine` are installed by `ProjectSession`.
Application-specific project services are added before the application
subsystem graph is initialized:

```cpp
auto session = app.addSubSystem<Bess::ProjectSession>();
session->addSubSystem<Bess::Svc::CopyPaste::Context>();
session->addSubSystem<Bess::Svc::SvcConnection>();
app.init();
```

Callers get project state through the session:

```cpp
auto session = app.getSubSystem<Bess::ProjectSession>();
auto &scenes = session->scenes();
auto &sim = session->sim();
auto &doc = session->doc();
```

## Edits

Single edits use the short domain API:

```cpp
auto added = session->addComp(def, mousePos, sceneId);
auto removed = session->rmComp(compId, sceneId);
auto moved = session->moveComp(compId, nextPos, sceneId);
auto slot = session->addSlot(newSlot, parentId, sceneId);
auto conn = session->addConn(newConn, sceneId);

auto undo = session->undo();
auto redo = session->redo();
```

Use one transaction when a UI action changes several things:

```cpp
auto result = session->run("Delete selection", [&](Bess::ProjectTx &tx) {
    auto status = tx.rmComp(firstId, sceneId);
    if (!status) {
        return status;
    }
    return tx.rmComp(secondId, sceneId);
});
```

`ProjectTx` latches its first staging error, so `commit()` cannot apply a
partially prepared transaction even if a caller forgets to inspect an
intermediate result. Commit applies steps in order and rolls completed steps
back in reverse order on failure. Undo uses reverse order; redo uses forward
order. A failed compensation faults the session and blocks further edits until
`recover()` is called.

`trackAdd`, `trackConn`, `trackMove`, and `trackParent` record mutations that a
Bess interaction must perform before its event reaches the session. Prefer the
non-`track` methods when the session can perform the mutation itself.

The history has entry and byte limits. Each committed state has a stable ID,
so undoing back to the saved state clears `dirty()` correctly. New edits after
undo discard the redo branch. Repeated moves, reparents, and project renames
merge into one undo entry when possible.

## Documents

`ProjectDoc` is owned by the session and directly serializes the Bess scene
driver and simulation engine:

```cpp
auto status = session->newProj("Counter");
status = session->saveAs("counter.bproj");
status = session->save();
status = session->load("counter.bproj");
```

All file work uses `std::filesystem`, `std::ifstream`, and `std::ofstream`.
Loads are size-bounded, parsed and schema-checked before changing live state,
and restore the previous project if applying the new project fails. Saves write
a temporary file beside the destination and then replace the destination. On
platforms where `std::filesystem::rename` cannot replace a file, the old file
is moved to a recovery backup until the new file is installed.

The UI owns file dialogs only. Project names, dirty state, save points, paths,
serialization, and load rollback remain inside `ProjectSession`.

## Build

The target is part of the Bess source graph:

```cmake
add_subdirectory(project_session)
target_link_libraries(your_target PRIVATE
    project_session::project_session)
```

The public API requires C++23.
