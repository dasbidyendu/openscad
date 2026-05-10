# OpenSCAD AI Subsystem Architecture

This document describes the design and interaction of the Agentic AI features in OpenSCAD.

## Overview

The AI subsystem provides a conversational interface that can "see" the workspace, propose code changes, and interact with the OpenSCAD engine via an OpenAI-compatible REST API.

### Key Components

1.  **AIService Core (`src/ai/AIService.h/cc`)**:
    - **Qt-Free**: Pure C++ implementation using **Boost Asio**.
    - Manages the network protocol (HTTP/HTTPS) via a minimal `HttpClient`.
    - Handles tool definitions (Function Calling) and JSON processing.
    - Runs in its own worker thread for non-blocking I/O.

2.  **QtAIService Bridge (`src/gui/ai/QtAIService.h/cc`)**:
    - Wraps the core service for use in the Qt GUI.
    - Handles conversion between Qt types (QString, QJson) and standard C++ types.
    - Emits signals for UI updates.

2.  **AIDock (`src/gui/ai/AIDock.h/cc`)**:
    - The primary UI for user interaction.
    - Contains the `ChatModel` (data) and `ChatDelegate` (rendering).
    - Manages the "Proposal Panel" for code changes.

3.  **ChatModel & ChatDelegate**:
    - **ChatModel**: A `QAbstractListModel` storing message history and loading states.
    - **ChatDelegate**: A `QStyledItemDelegate` that renders messages as high-quality chat bubbles (Theme-aware).

4.  **DiffDialog (`src/gui/ai/DiffDialog.h/cc`)**:
    - A specialized viewer for reviewing surgical code changes.
    - Compares current editor content with AI proposals.

## Agentic Workflow (Tool Registry)

The AI is "agentic" because it can execute actions on behalf of the user through tool calls:

| Tool | Action | Description |
| :--- | :--- | :--- |
| `get_editor_code` | `MainWindow` | Returns the current SCAD script. |
| `set_editor_code` | `MainWindow` | Updates the editor buffer (Surgical fix). |
| `trigger_preview` | `MainWindow` | Executes F5 (Preview). |
| `get_console_output` | `Console` | Returns recent compilation logs for debugging. |

## Context Management

A robust context system is used to provide the LLM with relevant workspace information without exceeding token limits.

- **Sliding Window**: User history is capped to the last N messages.
- **Dynamic System Prompt**: Injected at every request, containing:
    - Current filename.
    - Recent console errors.
    - OpenSCAD syntax rules.

## Optionality

The feature is designed to be zero-dependency and non-intrusive:
- **Build-time**: No external LLM libraries are linked.
- **Runtime**: The AI features (Dock and Menu) are hidden unless an API endpoint is configured in **Preferences**.

## Future Considerations

- **Vision**: Sending viewport screenshots to the AI using the existing `offscreen` renderer.
- **RAG**: Local documentation indexing for better Scad syntax generation.
- **Persistence**: Saving chat history to a local SQLite or JSON file.
