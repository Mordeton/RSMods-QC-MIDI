#!/usr/bin/env python3
"""Build and run host-independent upstream and QC tests with a Linux C++ compiler."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
framework = root / "DLL/Framework"
tests = {
    "ConflictResolverTests": [],
    "ResourceLedgerTests": ["ResourceLedger.cpp"],
    "AspectRatioTests": [],
    "CommandRouterTests": ["CommandRouter.cpp", "CommandCollisionDiagnostics.cpp"],
    "MainThreadInboxTests": ["MainThreadInbox.cpp"],
    "StateMachineTests": ["ModRegistry.cpp", "ResourceLedger.cpp", "CommandRouter.cpp",
                          "MainThreadInbox.cpp", "CommandCollisionDiagnostics.cpp"],
}
with tempfile.TemporaryDirectory(prefix="rsmods-tests-") as tmp:
    for name, sources in tests.items():
        exe = str(Path(tmp) / name)
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-pthread",
                        str(framework / "Tests" / (name + ".cpp")),
                        *[str(framework / s) for s in sources], "-o", exe], check=True)
        subprocess.run([exe], check=True)
    exe = str(Path(tmp) / "ToneSourceTimelineTests")
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20",
                    str(root / "scripts/tests/ToneSourceTimelineTests.cpp"),
                    str(root / "DLL/Mods/ToneSourceTimeline.cpp"), "-o", exe], check=True)
    subprocess.run([exe], check=True)
