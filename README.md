# Safe Start Condition Generation – Code Overview

This repo contains the full thesis [paper](SSG_daniel_sherbakov.pdf) as well as the implementation of the **safe start condition generation pipeline** used in the thesis, excluding the PlaJA framework.
The pipeline iteratively refines a start condition to exclude unsafe executions of a neural action policy, using a combination of **verification**, **testing**, and **approximation** techniques.

The implementation follows a **modular and highly configurable design**, allowing individual components of the pipeline to be easily extended or replaced, and enabling flexible switching between different verification, testing, and strengthening strategies.

## Core Components

### Top-Level
- **`safe_start_generator.{h,cpp}`**  Implements the main search loop of the pipeline. It coordinates testing, verification, condition refinement, and termination checks.
- **`start_generation_statistics.{h,cpp}`** Collection and export of global and per-iteration statistics.

### `verification_methods/`
Formal verification techniques used to identify unsafe states:
- **Invariant Strengthening** - derives the start condition from unsafety and uses z3 SMT solver for verification.
- **Start Condition Strengthening** - uses a model-defined start condition and uses Predicate Abstraction / Marabou for verification.
- Common interfaces and factory classes for method selection.

### `testing/`
Simulation-based testing of neural policies:
- Detection of unsafe execution paths via policy execution 
- or (Optional) policy run sampling.

### `approximation_methods/`
approximation techniques used to scale verification and testing:
- Bounding boxes and bounded boxes.
- Integrated optionally into refinement steps.

### `strengthening_strategy/`
Strategy layer that updates the start and unsafety conditions based on
unsafe states returned by testing or verification.

## Pipeline Overview

The pipeline alternates between **testing** and **verification** modes:

1. **Initialization**
   - The unsafety condition is derived from the reachability property.
   - The initial start condition is either the original model start or the negation of the unsafety condition (for invariant strengthening).

2. **Testing Phase (optional)**
   - Executes the neural policy in simulation.
   - Identifies unsafe execution paths.
   - Refines the start and unsafety conditions if unsafe states are found.
   - If approximation is enabled, it will attempt to approximate the set of unsafe states found.

3. **Verification Phase**
   - Applies the selected formal verification method.
   - If unsafe states are found, conditions are strengthened accordingly.
   - If approximation is enabled, it will attempt to approximate the set of unsafe states found.

4. **Termination Check**
   - The refined start condition is sampled.
   - The process terminates successfully if a non-empty safe start condition is found.

The loop continues until the start condition is proven safe or shown to be empty.

## Build Integration

Each submodule provides a `PlaJAFiles.cmake` file for integration into the overall PlaJA build system.
