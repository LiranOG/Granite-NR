# 💙 To the Community — A Personal Note

I want to speak directly to the physicists, engineers, students, developers, HPC practitioners, visualization people, and anyone who cares seriously about computational astrophysics.

GRANITE began as a personal project. It did not come out of an institution, a funded collaboration, or an established numerical relativity group. It started from a simple motivation: I wanted to understand extreme gravitational systems more deeply, and I wanted to build an open-source tool that could make serious NR/GRMHD experimentation more approachable over time.

That motivation is still personal. I care about this work. I care about the mathematics, the software architecture, the physics, and the possibility that more people could participate in this field if the tools were easier to inspect, configure, and reproduce.

At the same time, I want to be clear about where the project stands.

GRANITE is currently an active research project, not a finished production platform. The current foundation has meaningful implemented components, but the road to a credible v1.0 release is still very long. Some goals described in the Technical Vision — including broader YAML-defined scenarios, runtime browser telemetry, GPU execution, community challenge workflows, improved HPC deployment, and future hardware-abstraction work — are roadmap targets that require further implementation, validation, and review.

## Why I Started Building GRANITE

Numerical relativity and GRMHD are difficult fields to enter. The physics is already hard: Einstein’s equations, gauge choices, constraint control, relativistic fluids, magnetic fields, adaptive mesh refinement, boundary conditions, and stability issues all interact in ways that require care.

On top of that, the software can be difficult to approach. Many existing frameworks are powerful and historically important, but using them well often requires a strong background in C++, HPC systems, build environments, and framework-specific design. For a student, independent researcher, or theorist who wants to test a scenario, the software barrier can become almost as significant as the physics barrier.

The goal of GRANITE is to reduce that barrier where it can be reduced responsibly.

That does not mean hiding the complexity or pretending that simulations become simple. They do not. A readable configuration file does not replace convergence testing. A visualization does not prove physical correctness. A benchmark run does not validate every regime. But better interfaces, clearer configuration, documented scenarios, and reproducible workflows can help more people engage with the science in a serious way.

## What I Am Trying to Build

My long-term goal is for GRANITE to separate the numerical engine from the user-defined simulation scenario as much as possible.

The high-performance C++ core should handle the difficult computational work: evolution equations, AMR, MPI communication, GRMHD kernels, checkpointing, diagnostics, and performance-critical memory layout. The user-facing layer should allow scenarios to be described through structured YAML files, so that changing masses, spins, refinement settings, diagnostics, or output options does not require editing the solver itself.

This is an engineering goal, not a completed claim.

To make this reliable, the project needs schema validation, clear defaults, useful error messages, benchmark scenarios, documentation, and tests that catch invalid or physically questionable configurations. The purpose is not to make numerical relativity look easy. The purpose is to make the workflow more transparent, reproducible, and accessible without weakening the numerical standards.

I believe that is a worthwhile direction.

## Where the Project Stands

GRANITE started as the work of one person. That matters because it explains both the focus of the project and its current limitations.

A single developer can build a foundation, explore architecture, implement kernels, write documentation, and push the project forward. But a credible scientific code cannot mature in isolation. It needs review. It needs people to run it on different machines. It needs physicists to question assumptions. It needs HPC engineers to profile real workloads. It needs developers to find unsafe abstractions. It needs users to report where the documentation fails.

The project’s potential depends on that transition: from a personal effort into a shared technical and scientific effort.

## How People Can Help

You do not need to rewrite the core solvers to contribute. There are many useful ways to help, and not all of them require the same background.

A contribution can be:

- fixing a typo or unclear sentence in the documentation;
- testing the build instructions on a new machine;
- reporting a compiler, MPI, or platform issue;
- running a small benchmark and sharing the exact configuration;
- checking whether an example YAML file is understandable;
- improving error messages or configuration validation;
- reviewing a numerical method against known literature;
- adding unit tests around a fragile subsystem;
- profiling a performance bottleneck;
- helping prepare GPU-porting work;
- improving visualization and runtime telemetry;
- designing a realistic benchmark scenario;
- helping document deployment on an HPC cluster.

Small contributions matter because they reduce friction for the next person. A corrected command, a clearer explanation, a reproducible bug report, or a failed benchmark with good logs can save someone else hours of work.

Larger contributions are also welcome, especially from people with experience in numerical relativity, GRMHD, scientific computing, C++ performance engineering, GPU programming, MPI, HPC operations, or scientific visualization. GRANITE needs that kind of expertise if it is going to move from an ambitious prototype toward a tool that others can evaluate seriously.

## What I Can Promise

I cannot promise that every planned feature will arrive quickly.

I cannot promise that every scenario will work, or that every numerical issue has already been solved.

I can promise that I will try to keep the project honest: clear about what is implemented, clear about what is experimental, clear about what is only a roadmap item, and clear about what still needs validation.

The code is released as open source under the GPL-3.0 license. My intention is for GRANITE to remain inspectable, modifiable, and available to people who want to learn from it, test it, improve it, or build on it.

If you are reading this as a researcher, engineer, student, or independent developer: you are welcome here. Bring criticism. Bring bug reports. Bring benchmarks. Bring questions. Bring careful skepticism. That is how a project like this becomes stronger.

GRANITE began as one person’s attempt to build a serious open tool for extreme astrophysical simulation. It will only become something more useful if other people help test, challenge, and improve it.

Thank you for reading, and thank you to anyone who chooses to contribute.

— **Liran M. Schwartz (LiranOG),**
Founder and Lead Developer  

---

For the technical roadmap, current limitations, and planned development path, see the **[GRANITE Technical Vision](VISION.md)**.
