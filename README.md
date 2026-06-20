Workbench

Workbench is a personal game engine and experimentation platform built from scratch in C++ and Metal.

Goals:

* Learn how modern engines are structured
* Build systems incrementally through real prototypes
* Understand all code at an engineering-manager level or deeper
* Favor simplicity and clarity over premature optimization

Non-goals:

* Compete with Unity, Unreal, or Godot
* Build an ECS before it is needed
* Optimize before performance becomes a problem

Initial milestones:

* Window + application loop
* Metal renderer
* Camera
* Scene hierarchy
* GameObjects and Components
* ImGui editor
* Agent observability
* Scene serialization

## Agent workflow

Workbench can dump its full state to text for AI-assisted debugging.

In the editor, use **Copy Debug State** to paste the current engine state into chat,
or **Write Debug State** to save `logs/latest_state.txt`.
See `docs/observability.md` for details.

Philosophy:

Build features because prototypes need them, not because engines are supposed to have them.
