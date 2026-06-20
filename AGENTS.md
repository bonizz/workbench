Workbench Agent Guidelines

Project Goals

Workbench is a learning-focused game engine built in C++ and Metal.

The primary goal is understanding engine architecture through implementation.

The code should favor readability, simplicity, and explicitness over abstraction and optimization.

Design Principles

* Prefer simple solutions first.
* Avoid introducing systems before they are needed.
* Refactor when patterns emerge.
* Keep dependencies minimal.
* Optimize only after measuring.

Current Architecture Preferences

* Traditional GameObject + Component model.
* No ECS unless a real performance problem justifies it.
* Clear ownership and lifetime management.
* Stable object IDs for serialization.
* ImGui for tooling and debugging.

When Implementing Features

Before adding a new system:

1. Explain why the system is needed.
2. Describe simpler alternatives considered.
3. Keep implementations minimal.
4. Avoid speculative extensibility.

Code Style

* Modern C++
* Prefer composition over inheritance
* Minimize macros
* Favor explicit code over clever code
* Small focused classes

Communication

When proposing architectural changes:

* Explain tradeoffs.
* Explain what future problem the design solves.
* Call out complexity costs.
* Prefer iterative improvements.
