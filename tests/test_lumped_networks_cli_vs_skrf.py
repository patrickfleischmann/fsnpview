#!/usr/bin/env python3
"""Compare fsnpview lumped networks against scikit-rf implementations."""
from __future__ import annotations

import math
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import skrf as rf
from skrf.media import DefinedGammaZ0

C0 = 299_792_458.0
Z0_REFERENCE = 50.0
FMIN_HZ = 1e6
FMAX_HZ = 10e9
FREQ_POINTS = 501
MIN_MAGNITUDE = 1e-30
PLOT_S_PARAMETERS: Sequence[tuple[int, int]] = ((0, 0), (1, 0), (0, 1), (1, 1))


@dataclass(frozen=True)
class LumpedNetworkSpec:
    name: str
    cli_tokens: list[str]
    build_expected: Callable[[Callable[[float], DefinedGammaZ0]], rf.Network]


def rename_network(network: rf.Network, name: str) -> rf.Network:
    copy = network.copy()
    copy.name = name
    return copy


def build_series_rl(media: DefinedGammaZ0, inductance_h: float, resistance_ohm: float, base_name: str) -> rf.Network:
    resistor = rename_network(media.resistor(resistance_ohm), f"{base_name}_res")
    inductor = rename_network(media.inductor(inductance_h), f"{base_name}_ind")
    series = resistor ** inductor
    series = rename_network(series, base_name)
    return series


def build_shunt_rl(media: DefinedGammaZ0, inductance_h: float, resistance_ohm: float, base_name: str) -> rf.Network:
    series_branch = build_series_rl(media, inductance_h, resistance_ohm, f"{base_name}_series")
    short = rename_network(media.short(), f"{base_name}_short")
    terminated = series_branch ** short
    terminated = rename_network(terminated, f"{base_name}_terminated")
    shunted = media.shunt(terminated)
    shunted = rename_network(shunted, base_name)
    return shunted


def wrap_phase_deg(values: np.ndarray) -> np.ndarray:
    """Wrap phase values to [-180, 180) degrees."""
    return (values + 180.0) % 360.0 - 180.0


def amplitude_db(values: np.ndarray) -> np.ndarray:
    return 20.0 * np.log10(np.maximum(np.abs(values), MIN_MAGNITUDE))


def format_frequency_axis(freq_hz: np.ndarray) -> np.ndarray:
    return freq_hz / 1e9


def create_artifact_dir(root: Path) -> Path:
    artifact_dir = root / "tests" / "artifacts" / "lumped_networks"
    artifact_dir.mkdir(parents=True, exist_ok=True)
    for path in artifact_dir.glob("*"):
        if path.is_file():
            path.unlink()
    return artifact_dir


def run_fsnpview(
    binary: Path,
    cascade_tokens: Sequence[str],
    save_path: Path,
    *,
    env: dict[str, str],
    cwd: Path,
) -> None:
    command = [
        str(binary),
        "--nogui",
        "--freq",
        f"{FMIN_HZ}",
        f"{FMAX_HZ}",
        str(FREQ_POINTS),
        "--cascade",
        *cascade_tokens,
        "--save",
        str(save_path),
    ]
    subprocess.run(command, check=True, cwd=cwd, env=env)


def plot_comparison(
    network_name: str,
    freq_hz: np.ndarray,
    cli_network: rf.Network,
    expected_network: rf.Network,
    artifact_dir: Path,
) -> None:
    freq_ghz = format_frequency_axis(freq_hz)
    amplitude_fig, amplitude_axes = plt.subplots(len(PLOT_S_PARAMETERS), 2, figsize=(14, 12), sharex=True)
    phase_fig, phase_axes = plt.subplots(len(PLOT_S_PARAMETERS), 2, figsize=(14, 12), sharex=True)

    for row_index, (i, j) in enumerate(PLOT_S_PARAMETERS):
        cli_s = cli_network.s[:, i, j]
        expected_s = expected_network.s[:, i, j]

        cli_amp = amplitude_db(cli_s)
        expected_amp = amplitude_db(expected_s)
        amp_diff = cli_amp - expected_amp

        cli_phase = np.angle(cli_s, deg=True)
        expected_phase = np.angle(expected_s, deg=True)
        phase_diff = wrap_phase_deg(cli_phase - expected_phase)

        ax_amp = amplitude_axes[row_index, 0]
        ax_amp.plot(freq_ghz, cli_amp, label="fsnpview")
        ax_amp.plot(freq_ghz, expected_amp, label="scikit-rf", linestyle="--")
        ax_amp.set_ylabel(f"S{ i + 1 }{ j + 1 } (dB)")
        ax_amp.grid(True, which="both")
        if row_index == 0:
            ax_amp.set_title("Amplitude comparison")

        ax_amp_diff = amplitude_axes[row_index, 1]
        ax_amp_diff.plot(freq_ghz, amp_diff, color="tab:red")
        ax_amp_diff.set_ylabel(f"ΔS{ i + 1 }{ j + 1 } (dB)")
        ax_amp_diff.grid(True, which="both")
        if row_index == 0:
            ax_amp_diff.set_title("Amplitude difference")

        ax_phase = phase_axes[row_index, 0]
        ax_phase.plot(freq_ghz, cli_phase, label="fsnpview")
        ax_phase.plot(freq_ghz, expected_phase, label="scikit-rf", linestyle="--")
        ax_phase.set_ylabel(f"S{ i + 1 }{ j + 1 } (deg)")
        ax_phase.grid(True, which="both")
        if row_index == 0:
            ax_phase.set_title("Phase comparison")

        ax_phase_diff = phase_axes[row_index, 1]
        ax_phase_diff.plot(freq_ghz, phase_diff, color="tab:purple")
        ax_phase_diff.set_ylabel(f"ΔS{ i + 1 }{ j + 1 } (deg)")
        ax_phase_diff.grid(True, which="both")
        if row_index == 0:
            ax_phase_diff.set_title("Phase difference")

    for axes in (amplitude_axes, phase_axes):
        axes[-1, 0].set_xlabel("Frequency (GHz)")
        axes[-1, 1].set_xlabel("Frequency (GHz)")

    handles, labels = amplitude_axes[0, 0].get_legend_handles_labels()
    if handles:
        amplitude_fig.legend(handles, labels, loc="upper center", ncol=2)
    handles, labels = phase_axes[0, 0].get_legend_handles_labels()
    if handles:
        phase_fig.legend(handles, labels, loc="upper center", ncol=2)

    amplitude_fig.tight_layout(rect=(0, 0, 1, 0.96))
    phase_fig.tight_layout(rect=(0, 0, 1, 0.96))

    amplitude_fig.savefig(artifact_dir / f"{network_name}_amplitude.png", dpi=200)
    phase_fig.savefig(artifact_dir / f"{network_name}_phase.png", dpi=200)
    plt.close(amplitude_fig)
    plt.close(phase_fig)


def resolve_fsnpview_binary(repo_root: Path) -> Path | None:
    """Return the fsnpview executable for the current platform."""

    env_override = os.environ.get("FSNPVIEW_BINARY")
    candidates: list[Path] = []

    if env_override:
        override_path = Path(env_override)
        if not override_path.is_absolute():
            override_path = repo_root / override_path
        candidates.append(override_path)

    if sys.platform.startswith("win"):
        candidates.extend(
            (
                repo_root / "fsnpview.exe",
                repo_root / "build" / "fsnpview.exe",
                repo_root / "Release" / "fsnpview.exe",
            )
        )
    else:
        candidates.extend(
            (
                repo_root / "fsnpview",
                repo_root / "fsnpview.exe",
            )
        )

    seen: set[Path] = set()
    for candidate in candidates:
        if candidate in seen:
            continue
        seen.add(candidate)
        if not candidate.is_file():
            continue
        if sys.platform.startswith("win") and candidate.suffix.lower() not in {".exe", ".bat", ".cmd", ".com"}:
            # Avoid trying to execute non-Windows binaries such as the Linux build artifact.
            continue
        return candidate
    return None


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]

    binary = resolve_fsnpview_binary(repo_root)
    if binary is None:
        print(
            "fsnpview binary not found. Build the project and/or set FSNPVIEW_BINARY before running this test.",
            file=sys.stderr,
        )

        return 1

    artifact_dir = create_artifact_dir(repo_root)

    freq_values = np.linspace(FMIN_HZ, FMAX_HZ, FREQ_POINTS)
    frequency = rf.Frequency.from_f(freq_values, unit="hz")
    gamma = 1j * 2.0 * math.pi * freq_values / C0

    def make_media(z0: float = Z0_REFERENCE) -> DefinedGammaZ0:
        return DefinedGammaZ0(frequency=frequency, z0=z0, z0_port=Z0_REFERENCE, gamma=gamma)

    specs: list[LumpedNetworkSpec] = [
        LumpedNetworkSpec(
            name="R_series",
            cli_tokens=["R_series", "R", "75.0"],
            build_expected=lambda factory: rename_network(factory().resistor(75.0), "R_series_expected"),
        ),
        LumpedNetworkSpec(
            name="R_shunt",
            cli_tokens=["R_shunt", "R", "30.0"],
            build_expected=lambda factory: rename_network(factory().shunt_resistor(30.0), "R_shunt_expected"),
        ),
        LumpedNetworkSpec(
            name="C_series",
            cli_tokens=["C_series", "C", "2.2"],
            build_expected=lambda factory: rename_network(factory().capacitor(2.2e-12), "C_series_expected"),
        ),
        LumpedNetworkSpec(
            name="C_shunt",
            cli_tokens=["C_shunt", "C", "4.7"],
            build_expected=lambda factory: rename_network(factory().shunt_capacitor(4.7e-12), "C_shunt_expected"),
        ),
        LumpedNetworkSpec(
            name="L_series",
            cli_tokens=["L_series", "L", "8.2", "R_ser", "0.75"],
            build_expected=lambda factory: rename_network(
                build_series_rl(factory(), 8.2e-9, 0.75, "L_series_expected"),
                "L_series_expected",
            ),
        ),
        LumpedNetworkSpec(
            name="L_shunt",
            cli_tokens=["L_shunt", "L", "12.0", "R_ser", "0.25"],
            build_expected=lambda factory: rename_network(
                build_shunt_rl(factory(), 12e-9, 0.25, "L_shunt_expected"),
                "L_shunt_expected",
            ),
        ),
        LumpedNetworkSpec(
            name="TransmissionLine",
            cli_tokens=["TransmissionLine", "len", "0.015", "z0", "60.0"],
            build_expected=lambda factory: rename_network(
                factory(60.0).line(d=0.015, unit="m", z0=60.0),
                "TransmissionLine_expected",
            ),
        ),
    ]

    env = os.environ.copy()
    env.setdefault("QT_QPA_PLATFORM", "offscreen")

    success = True

    for spec in specs:
        s2p_path = artifact_dir / f"{spec.name}.s2p"
        run_fsnpview(binary, spec.cli_tokens, s2p_path, env=env, cwd=repo_root)
        cli_network = rf.Network(str(s2p_path))
        expected_network = spec.build_expected(make_media)

        if cli_network.frequency != expected_network.frequency:
            cli_network = cli_network.interpolate(expected_network.frequency, kind="linear")

        diff = cli_network.s - expected_network.s
        max_abs_error = float(np.max(np.abs(diff)))

        amp_diff = amplitude_db(cli_network.s) - amplitude_db(expected_network.s)
        max_amp_error = float(np.nanmax(np.abs(amp_diff)))

        phase_diff = wrap_phase_deg(np.angle(cli_network.s, deg=True) - np.angle(expected_network.s, deg=True))
        max_phase_error = float(np.nanmax(np.abs(phase_diff)))

        print(
            f"{spec.name}: max |ΔS| = {max_abs_error:.3e}, "
            f"max |Δmag| = {max_amp_error:.3e} dB, max |Δphase| = {max_phase_error:.3e}°"
        )

        tolerance = 1e-9
        if max_abs_error > tolerance:
            success = False
            print(f"  ERROR: Difference exceeds tolerance of {tolerance:g}.", file=sys.stderr)

        plot_comparison(spec.name, freq_values, cli_network, expected_network, artifact_dir)

    if not success:
        return 1

    print(f"Plots saved to {artifact_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
