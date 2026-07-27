# Architecture

PluginAnalyzer follows an incremental Clean Architecture layout:

```text
Presentation (JUCE components)
          |
          v
Application (use cases and ports)
          |
          v
Domain (analysis models)
          ^
          |
Infrastructure (JUCE audio/plugin engine)
```

Dependencies point inward:

- `Source/Domain` contains analysis concepts and immutable result data. It has
  no JUCE dependency.
- `Source/Application` defines the UI-facing `AnalysisService` port and the
  `AnalysisSession` use case. Tab selection and control-visibility policy live
  here so they can be tested without a window or audio device.
- `AnalyzerEngine` is an infrastructure adapter. It implements the application
  port while owning JUCE plugin hosting, audio buffers, FFT processing, FIFOs,
  and the worker thread.
- JUCE components depend on `AnalysisService`, not on `AnalyzerEngine`.
  `MainComponent` is the composition root and retains direct access to the
  engine only for JUCE-specific audio and plugin-loading operations.

## Rules for future changes

1. Add framework-free analysis vocabulary to `Domain`.
2. Put user workflows and presentation policy in `Application`.
3. Define an application port before calling a filesystem, JUCE, or platform
   service from an inward layer.
4. Keep realtime audio constraints inside infrastructure adapters.
5. Test application policy through ports; reserve JUCE integration tests for
   adapter and application-lifecycle behaviour.
