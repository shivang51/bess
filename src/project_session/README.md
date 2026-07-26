# Project session architecture

`ProjectSession` is the application boundary for an open project. UI code
should read project state through `ProjectSession::read()` and submit all
mutations as transactions. It should not call the simulation engine or mutate
an EnTT registry directly.

## Ownership

```text
ProjectSession (UI-facing façade)
├── ProjectDocument (persistent project aggregate)
│   └── SceneDocument[] (one EnTT registry per scene)
├── ISimulationGateway (simulation-engine adapter)
├── transaction history (atomic undo/redo)
└── session observers (coarse invalidation events)
```

- `ProjectDocument` owns scene order, root/active scene identity, and the
  module-to-scene index.
- Each `SceneDocument` owns an independent EnTT registry. Stable `UUID`s cross
  API and persistence boundaries; `entt::entity` values never do.
- ECS components contain project data only. Renderer handles, widgets,
  callbacks, subscriptions, and other process-local resources belong in UI or
  runtime systems.
- `ISimulationGateway` is the only project-layer dependency on simulation
  behavior. This makes headless tools and deterministic unit tests possible.
- A transaction compensates already-applied operations on failure. Undo and
  redo also compensate partial failures, so history is not silently lost.

## UI integration rules

1. Hold a `ProjectReadAccess` for the duration of a render/query pass.
2. Cache UUIDs, never EnTT entity handles or pointers into component storage.
3. Use `ProjectSession::transact()` for edits, including hierarchy and scene
   changes.
4. Subscribe to `SessionChangeSet` for invalidation. Observers run after the
   project write lock is released and may safely read the session.
5. Treat simulation IDs as runtime details. Scene UUIDs remain stable when a
   simulation component is undone, redone, or rehydrated from disk.

## Extension policy

Built-in project concepts use typed ECS components. Plugin-defined persistent
data uses `ExtensionComponents`, keyed by a stable namespaced type name. A
plugin may materialize additional runtime-only EnTT components, but persisted
state must remain readable when that plugin is absent.

The store interface is deliberately separate from the session.
`JsonProjectStore` writes the versioned `bess.project` schema atomically.
Pre-session `SceneState` files are deliberately rejected until a dedicated
legacy importer can translate and validate them; legacy parsing and migration
policy do not belong in the core aggregate.
