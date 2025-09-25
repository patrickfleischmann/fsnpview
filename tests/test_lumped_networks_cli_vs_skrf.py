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
DEFAULT_FMIN_HZ = 1e6
DEFAULT_FMAX_HZ = 10e9
DEFAULT_FREQ_POINTS = 501
MIN_MAGNITUDE = 1e-30
PLOT_S_PARAMETERS: Sequence[tuple[int, int]] = ((0, 0), (1, 0), (0, 1), (1, 1))


def abcd_to_s(abcd: np.ndarray, z0: float) -> np.ndarray:
    """Convert ABCD matrices to S-parameters for the provided reference impedance."""

    if abcd.ndim != 3 or abcd.shape[1:] != (2, 2):
        raise ValueError("ABCD array must have shape (N, 2, 2)")

    a = abcd[:, 0, 0]
    b = abcd[:, 0, 1]
    c = abcd[:, 1, 0]
    d = abcd[:, 1, 1]

    denominator = a + b / z0 + c * z0 + d

    with np.errstate(divide="ignore", invalid="ignore"):
        s11 = (a + b / z0 - c * z0 - d) / denominator
        s21 = 2.0 / denominator
        s12 = 2.0 * (a * d - b * c) / denominator
        s22 = (-a + b / z0 - c * z0 + d) / denominator

    s = np.empty_like(abcd, dtype=np.complex128)
    s[:, 0, 0] = s11
    s[:, 0, 1] = s12
    s[:, 1, 0] = s21
    s[:, 1, 1] = s22
    return s


@dataclass(frozen=True)
class LumpedNetworkSpec:
    name: str
    cli_tokens: list[str]
    build_expected: Callable[[Callable[[float], DefinedGammaZ0]], rf.Network]
    fmin_hz: float = DEFAULT_FMIN_HZ
    fmax_hz: float = DEFAULT_FMAX_HZ
    npoints: int = DEFAULT_FREQ_POINTS


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


def cascade_networks(networks: Sequence[rf.Network], base_name: str) -> rf.Network:
    if not networks:
        raise ValueError("At least one network is required to form a cascade.")
    result = networks[0]
    for network in networks[1:]:
        result = result ** network
    return rename_network(result, base_name)


def create_r_series(media: DefinedGammaZ0, resistance_ohm: float, base_name: str) -> rf.Network:
    return rename_network(media.resistor(resistance_ohm), base_name)


def create_r_shunt(media: DefinedGammaZ0, resistance_ohm: float, base_name: str) -> rf.Network:
    return rename_network(media.shunt_resistor(resistance_ohm), base_name)


def create_c_series(media: DefinedGammaZ0, capacitance_pf: float, base_name: str) -> rf.Network:
    return rename_network(media.capacitor(capacitance_pf * 1e-12), base_name)


def create_c_shunt(media: DefinedGammaZ0, capacitance_pf: float, base_name: str) -> rf.Network:
    return rename_network(media.shunt_capacitor(capacitance_pf * 1e-12), base_name)


def create_l_series(media: DefinedGammaZ0, inductance_nh: float, resistance_ohm: float, base_name: str) -> rf.Network:
    return rename_network(build_series_rl(media, inductance_nh * 1e-9, resistance_ohm, base_name), base_name)


def create_l_shunt(media: DefinedGammaZ0, inductance_nh: float, resistance_ohm: float, base_name: str) -> rf.Network:
    return rename_network(build_shunt_rl(media, inductance_nh * 1e-9, resistance_ohm, base_name), base_name)


def create_transmission_line(
    factory: Callable[[float], DefinedGammaZ0],
    length_m: float,
    z0_ohm: float,
    base_name: str,
    *,
    er_eff: float = 1.0,
    a_db_per_m: float = 0.0,
    a_d_db_per_m: float = 0.0,
    fa_hz: float = 1e9,
) -> rf.Network:
    base_media = factory(z0_ohm)
    frequency = base_media.frequency
    freq_hz = frequency.f
    sqrt_er_eff = math.sqrt(max(er_eff, 0.0))
    beta = 2.0 * math.pi * freq_hz * sqrt_er_eff / C0

    alpha_db = np.zeros_like(freq_hz)
    if a_db_per_m != 0.0 or a_d_db_per_m != 0.0:
        if fa_hz > 0.0:
            ratio = np.maximum(freq_hz / fa_hz, 0.0)
            alpha_db = a_db_per_m * np.sqrt(ratio) + a_d_db_per_m * ratio
        else:
            alpha_db = (a_db_per_m + a_d_db_per_m) * np.ones_like(freq_hz)
    alpha_neper = alpha_db * (math.log(10.0) / 20.0)
    gamma = alpha_neper + 1j * beta

    custom_media = DefinedGammaZ0(
        frequency=frequency,
        z0=z0_ohm,
        z0_port=Z0_REFERENCE,
        gamma=gamma,
    )
    return rename_network(custom_media.line(d=length_m, unit="m", z0=z0_ohm), base_name)


def expected_r_series(factory: Callable[[float], DefinedGammaZ0], resistance_ohm: float, base_name: str) -> rf.Network:
    return create_r_series(factory(), resistance_ohm, base_name)


def expected_r_shunt(factory: Callable[[float], DefinedGammaZ0], resistance_ohm: float, base_name: str) -> rf.Network:
    return create_r_shunt(factory(), resistance_ohm, base_name)


def expected_c_series(factory: Callable[[float], DefinedGammaZ0], capacitance_pf: float, base_name: str) -> rf.Network:
    return create_c_series(factory(), capacitance_pf, base_name)


def expected_c_shunt(factory: Callable[[float], DefinedGammaZ0], capacitance_pf: float, base_name: str) -> rf.Network:
    return create_c_shunt(factory(), capacitance_pf, base_name)


def expected_l_series(
    factory: Callable[[float], DefinedGammaZ0], inductance_nh: float, resistance_ohm: float, base_name: str
) -> rf.Network:
    return create_l_series(factory(), inductance_nh, resistance_ohm, base_name)


def expected_l_shunt(
    factory: Callable[[float], DefinedGammaZ0], inductance_nh: float, resistance_ohm: float, base_name: str
) -> rf.Network:
    return create_l_shunt(factory(), inductance_nh, resistance_ohm, base_name)


def expected_transmission_line(
    factory: Callable[[float], DefinedGammaZ0],
    length_m: float,
    z0_ohm: float,
    base_name: str,
    *,
    er_eff: float = 1.0,
) -> rf.Network:
    return create_transmission_line(factory, length_m, z0_ohm, base_name, er_eff=er_eff)


def expected_tl_lossy(
    factory: Callable[[float], DefinedGammaZ0],
    length_m: float,
    z0_ohm: float,
    base_name: str,
    *,
    er_eff: float,
    a_db_per_m: float,
    a_d_db_per_m: float,
    fa_hz: float,
) -> rf.Network:
    return create_transmission_line(
        factory,
        length_m,
        z0_ohm,
        base_name,
        er_eff=er_eff,
        a_db_per_m=a_db_per_m,
        a_d_db_per_m=a_d_db_per_m,
        fa_hz=fa_hz,
    )


def format_value(value: float) -> str:
    return f"{value:.12g}"


def tokens_r_series(resistance_ohm: float) -> list[str]:
    return ["R_series", "R", format_value(resistance_ohm)]


def tokens_r_shunt(resistance_ohm: float) -> list[str]:
    return ["R_shunt", "R", format_value(resistance_ohm)]


def tokens_c_series(capacitance_pf: float) -> list[str]:
    return ["C_series", "C", format_value(capacitance_pf)]


def tokens_c_shunt(capacitance_pf: float) -> list[str]:
    return ["C_shunt", "C", format_value(capacitance_pf)]


def tokens_l_series(inductance_nh: float, resistance_ohm: float) -> list[str]:
    return ["L_series", "L", format_value(inductance_nh), "R_ser", format_value(resistance_ohm)]


def tokens_l_shunt(inductance_nh: float, resistance_ohm: float) -> list[str]:
    return ["L_shunt", "L", format_value(inductance_nh), "R_ser", format_value(resistance_ohm)]


def tokens_transmission_line(length_m: float, z0_ohm: float) -> list[str]:
    length_mm = length_m * 1e3
    return [
        "TransmissionLine",
        "len",
        format_value(length_mm),
        "z0",
        format_value(z0_ohm),
    ]


def tokens_transmission_line_er(length_m: float, z0_ohm: float, er_eff: float) -> list[str]:
    tokens = tokens_transmission_line(length_m, z0_ohm)
    tokens += ["er_eff", format_value(er_eff)]
    return tokens


def tokens_tl_lossy(
    length_m: float,
    z0_ohm: float,
    er_eff: float,
    a_db_per_m: float,
    a_d_db_per_m: float,
    fa_hz: float,
) -> list[str]:
    length_mm = length_m * 1e3
    return [
        "TL_lossy",
        "len",
        format_value(length_mm),
        "z0",
        format_value(z0_ohm),
        "er_eff",
        format_value(er_eff),
        "a",
        format_value(a_db_per_m),
        "a_d",
        format_value(a_d_db_per_m),
        "fa",
        format_value(fa_hz),
    ]


def tokens_lrc_series_shunt(inductance_nh: float, resistance_ohm: float, capacitance_pf: float) -> list[str]:
    return [
        "LRC_ser_shunt",
        "l",
        format_value(inductance_nh),
        "r",
        format_value(resistance_ohm),
        "c",
        format_value(capacitance_pf),
    ]


def tokens_lrc_parallel_series(inductance_nh: float, resistance_ohm: float, capacitance_pf: float) -> list[str]:
    return [
        "LRC_par_ser",
        "l",
        format_value(inductance_nh),
        "r",
        format_value(resistance_ohm),
        "c",
        format_value(capacitance_pf),
    ]


def network_from_abcd(frequency: rf.Frequency, abcd: np.ndarray, base_name: str) -> rf.Network:
    s_params = abcd_to_s(abcd, Z0_REFERENCE)
    network = rf.Network(frequency=frequency, s=s_params, z0=Z0_REFERENCE)
    return rename_network(network, base_name)


def create_lrc_series_shunt(
    factory: Callable[[float], DefinedGammaZ0],
    inductance_nh: float,
    resistance_ohm: float,
    capacitance_pf: float,
    base_name: str,
) -> rf.Network:
    base_media = factory()
    frequency = base_media.frequency
    freq_hz = frequency.f
    omega = 2.0 * math.pi * freq_hz
    inductance_h = inductance_nh * 1e-9
    capacitance_f = capacitance_pf * 1e-12

    impedance = np.full(freq_hz.shape, resistance_ohm, dtype=np.complex128)
    impedance += 1j * omega * inductance_h
    if capacitance_f > 0.0:
        cap_impedance = np.full(freq_hz.shape, np.inf, dtype=np.complex128)
        nonzero = omega != 0.0
        cap_impedance[nonzero] = 1.0 / (1j * omega[nonzero] * capacitance_f)
        impedance = impedance + cap_impedance

    admittance = np.zeros(freq_hz.shape, dtype=np.complex128)
    finite_impedance = np.isfinite(impedance) & (np.abs(impedance) > 0.0)
    admittance[finite_impedance] = 1.0 / impedance[finite_impedance]

    abcd = np.zeros((freq_hz.size, 2, 2), dtype=np.complex128)
    abcd[:, 0, 0] = 1.0
    abcd[:, 1, 1] = 1.0
    abcd[:, 1, 0] = admittance
    return network_from_abcd(frequency, abcd, base_name)


def create_lrc_parallel_series(
    factory: Callable[[float], DefinedGammaZ0],
    inductance_nh: float,
    resistance_ohm: float,
    capacitance_pf: float,
    base_name: str,
) -> rf.Network:
    base_media = factory()
    frequency = base_media.frequency
    freq_hz = frequency.f
    omega = 2.0 * math.pi * freq_hz
    inductance_h = inductance_nh * 1e-9
    capacitance_f = capacitance_pf * 1e-12

    if resistance_ohm != 0.0:
        y_r = np.full(freq_hz.shape, 1.0 / resistance_ohm, dtype=np.complex128)
    else:
        y_r = np.full(freq_hz.shape, np.inf, dtype=np.complex128)

    y_l = np.full(freq_hz.shape, np.inf, dtype=np.complex128)
    if inductance_h != 0.0:
        nonzero = omega != 0.0
        y_l[nonzero] = 1.0 / (1j * omega[nonzero] * inductance_h)
    y_c = 1j * omega * capacitance_f

    admittance = y_r + y_l + y_c
    impedance = np.full(freq_hz.shape, 0.0, dtype=np.complex128)
    finite_mask = np.isfinite(admittance)
    zero_mask = finite_mask & np.isclose(admittance, 0.0)
    nonzero_mask = finite_mask & (~zero_mask)
    impedance[nonzero_mask] = 1.0 / admittance[nonzero_mask]
    impedance[zero_mask] = np.inf

    abcd = np.zeros((freq_hz.size, 2, 2), dtype=np.complex128)
    abcd[:, 0, 0] = 1.0
    abcd[:, 1, 1] = 1.0
    abcd[:, 0, 1] = impedance
    return network_from_abcd(frequency, abcd, base_name)


def expected_lrc_series_shunt(
    factory: Callable[[float], DefinedGammaZ0],
    inductance_nh: float,
    resistance_ohm: float,
    capacitance_pf: float,
    base_name: str,
) -> rf.Network:
    return create_lrc_series_shunt(factory, inductance_nh, resistance_ohm, capacitance_pf, base_name)


def expected_lrc_parallel_series(
    factory: Callable[[float], DefinedGammaZ0],
    inductance_nh: float,
    resistance_ohm: float,
    capacitance_pf: float,
    base_name: str,
) -> rf.Network:
    return create_lrc_parallel_series(factory, inductance_nh, resistance_ohm, capacitance_pf, base_name)


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
    fmin_hz: float,
    fmax_hz: float,
    npoints: int,
) -> None:
    command = [
        str(binary),
        "--nogui",
        "--freq",
        f"{fmin_hz}",
        f"{fmax_hz}",
        str(npoints),
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
                repo_root / "bin" / "fsnpview.exe"
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

    specs: list[LumpedNetworkSpec] = [
        LumpedNetworkSpec(
            name="R_series",
            cli_tokens=tokens_r_series(75.0),
            build_expected=lambda factory: expected_r_series(factory, 75.0, "R_series_expected"),
            fmin_hz=1e2,
            fmax_hz=1e5,
            npoints=33,
        ),
        LumpedNetworkSpec(
            name="R_shunt",
            cli_tokens=tokens_r_shunt(30.0),
            build_expected=lambda factory: expected_r_shunt(factory, 30.0, "R_shunt_expected"),
            fmin_hz=1e-1,
            fmax_hz=1e1,
            npoints=21,
        ),
        LumpedNetworkSpec(
            name="C_series",
            cli_tokens=tokens_c_series(2.2),
            build_expected=lambda factory: expected_c_series(factory, 2.2, "C_series_expected"),
            fmin_hz=5e3,
            fmax_hz=5e6,
            npoints=51,
        ),
        LumpedNetworkSpec(
            name="C_shunt",
            cli_tokens=tokens_c_shunt(4.7),
            build_expected=lambda factory: expected_c_shunt(factory, 4.7, "C_shunt_expected"),
            fmin_hz=1e4,
            fmax_hz=1e7,
            npoints=75,
        ),
        LumpedNetworkSpec(
            name="L_series",
            cli_tokens=tokens_l_series(8.2, 0.75),
            build_expected=lambda factory: expected_l_series(factory, 8.2, 0.75, "L_series_expected"),
            fmin_hz=2e5,
            fmax_hz=2e8,
            npoints=101,
        ),
        LumpedNetworkSpec(
            name="L_shunt",
            cli_tokens=tokens_l_shunt(12.0, 0.25),
            build_expected=lambda factory: expected_l_shunt(factory, 12.0, 0.25, "L_shunt_expected"),
            fmin_hz=5e5,
            fmax_hz=5e8,
            npoints=111,
        ),
        LumpedNetworkSpec(
            name="TransmissionLine",
            cli_tokens=tokens_transmission_line(0.015, 60.0),
            build_expected=lambda factory: expected_transmission_line(
                factory, 0.015, 60.0, "TransmissionLine_expected"
            ),
            fmin_hz=1e6,
            fmax_hz=2e9,
            npoints=151,
        ),
        LumpedNetworkSpec(
            name="TransmissionLine_er_eff",
            cli_tokens=tokens_transmission_line_er(0.011, 55.0, 2.4),
            build_expected=lambda factory: expected_transmission_line(
                factory, 0.011, 55.0, "TransmissionLine_er_eff_expected", er_eff=2.4
            ),
            fmin_hz=2e6,
            fmax_hz=3e9,
            npoints=121,
        ),
        LumpedNetworkSpec(
            name="TL_lossy",
            cli_tokens=tokens_tl_lossy(0.008, 42.0, 3.1, 8.0, 1.2, 5e9),
            build_expected=lambda factory: expected_tl_lossy(
                factory,
                0.008,
                42.0,
                "TL_lossy_expected",
                er_eff=3.1,
                a_db_per_m=8.0,
                a_d_db_per_m=1.2,
                fa_hz=5e9,
            ),
            fmin_hz=5e6,
            fmax_hz=8e9,
            npoints=161,
        ),
        LumpedNetworkSpec(
            name="LRC_ser_shunt",
            cli_tokens=tokens_lrc_series_shunt(2.5, 0.002, 1.8),
            build_expected=lambda factory: expected_lrc_series_shunt(
                factory, 2.5, 0.002, 1.8, "LRC_ser_shunt_expected"
            ),
            fmin_hz=1e4,
            fmax_hz=1e8,
            npoints=91,
        ),
        LumpedNetworkSpec(
            name="LRC_par_ser",
            cli_tokens=tokens_lrc_parallel_series(3.7, 750000.0, 2.6),
            build_expected=lambda factory: expected_lrc_parallel_series(
                factory, 3.7, 750000.0, 2.6, "LRC_par_ser_expected"
            ),
            fmin_hz=2e3,
            fmax_hz=2e7,
            npoints=83,
        ),
    ]

    def build_r_series_double(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        first = create_r_series(media, 25.0, "R_series_double_stage1")
        second = create_r_series(media, 80.0, "R_series_double_stage2")
        return cascade_networks([first, second], "R_series_double_expected")

    def build_r_shunt_double(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        first = create_r_shunt(media, 40.0, "R_shunt_double_stage1")
        second = create_r_shunt(media, 65.0, "R_shunt_double_stage2")
        return cascade_networks([first, second], "R_shunt_double_expected")

    def build_c_series_double(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        first = create_c_series(media, 1.5, "C_series_double_stage1")
        second = create_c_series(media, 5.5, "C_series_double_stage2")
        return cascade_networks([first, second], "C_series_double_expected")

    def build_c_shunt_double(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        first = create_c_shunt(media, 2.5, "C_shunt_double_stage1")
        second = create_c_shunt(media, 6.5, "C_shunt_double_stage2")
        return cascade_networks([first, second], "C_shunt_double_expected")

    def build_l_series_double(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        first = create_l_series(media, 5.6, 0.2, "L_series_double_stage1")
        second = create_l_series(media, 15.0, 0.7, "L_series_double_stage2")
        return cascade_networks([first, second], "L_series_double_expected")

    def build_l_shunt_double(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        first = create_l_shunt(media, 7.5, 0.15, "L_shunt_double_stage1")
        second = create_l_shunt(media, 20.0, 0.45, "L_shunt_double_stage2")
        return cascade_networks([first, second], "L_shunt_double_expected")

    def build_transmission_line_double(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        first = create_transmission_line(factory, 0.004, 45.0, "TransmissionLine_double_section1")
        second = create_transmission_line(factory, 0.007, 65.0, "TransmissionLine_double_section2")
        return cascade_networks([first, second], "TransmissionLine_double_expected")

    specs.extend(
        [
            LumpedNetworkSpec(
                name="R_series_double",
                cli_tokens=tokens_r_series(25.0) + tokens_r_series(80.0),
                build_expected=build_r_series_double,
                fmin_hz=1e3,
                fmax_hz=1e6,
                npoints=41,
            ),
            LumpedNetworkSpec(
                name="R_shunt_double",
                cli_tokens=tokens_r_shunt(40.0) + tokens_r_shunt(65.0),
                build_expected=build_r_shunt_double,
                fmin_hz=0.5,
                fmax_hz=5e1,
                npoints=27,
            ),
            LumpedNetworkSpec(
                name="C_series_double",
                cli_tokens=tokens_c_series(1.5) + tokens_c_series(5.5),
                build_expected=build_c_series_double,
                fmin_hz=3e4,
                fmax_hz=3e7,
                npoints=81,
            ),
            LumpedNetworkSpec(
                name="C_shunt_double",
                cli_tokens=tokens_c_shunt(2.5) + tokens_c_shunt(6.5),
                build_expected=build_c_shunt_double,
                fmin_hz=4e5,
                fmax_hz=4e8,
                npoints=123,
            ),
            LumpedNetworkSpec(
                name="L_series_double",
                cli_tokens=tokens_l_series(5.6, 0.2) + tokens_l_series(15.0, 0.7),
                build_expected=build_l_series_double,
                fmin_hz=6e5,
                fmax_hz=6e8,
                npoints=135,
            ),
            LumpedNetworkSpec(
                name="L_shunt_double",
                cli_tokens=tokens_l_shunt(7.5, 0.15) + tokens_l_shunt(20.0, 0.45),
                build_expected=build_l_shunt_double,
                fmin_hz=9e5,
                fmax_hz=9e8,
                npoints=147,
            ),
            LumpedNetworkSpec(
                name="TransmissionLine_double",
                cli_tokens=tokens_transmission_line(0.004, 45.0)
                + tokens_transmission_line(0.007, 65.0),
                build_expected=build_transmission_line_double,
                fmin_hz=1.5e6,
                fmax_hz=4e9,
                npoints=165,
            ),
        ]
    )

    def build_r_series_then_c_series(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        resistor = create_r_series(media, 33.0, "R_series_then_C_series_res")
        capacitor = create_c_series(media, 3.3, "R_series_then_C_series_cap")
        return cascade_networks([resistor, capacitor], "R_series_then_C_series_expected")

    def build_r_shunt_then_l_series(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        shunt = create_r_shunt(media, 85.0, "R_shunt_then_L_series_shunt")
        inductive = create_l_series(media, 9.1, 0.5, "R_shunt_then_L_series_ind")
        return cascade_networks([shunt, inductive], "R_shunt_then_L_series_expected")

    def build_c_series_then_transmission(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        capacitor = create_c_series(media, 4.4, "C_series_then_TransmissionLine_cap")
        line = create_transmission_line(factory, 0.006, 55.0, "C_series_then_TransmissionLine_line")
        return cascade_networks([capacitor, line], "C_series_then_TransmissionLine_expected")

    def build_c_shunt_then_r_shunt(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        cap = create_c_shunt(media, 5.2, "C_shunt_then_R_shunt_cap")
        resistor = create_r_shunt(media, 120.0, "C_shunt_then_R_shunt_res")
        return cascade_networks([cap, resistor], "C_shunt_then_R_shunt_expected")

    def build_l_series_then_c_shunt(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        inductive = create_l_series(media, 11.0, 0.35, "L_series_then_C_shunt_ind")
        capacitor = create_c_shunt(media, 3.8, "L_series_then_C_shunt_cap")
        return cascade_networks([inductive, capacitor], "L_series_then_C_shunt_expected")

    def build_l_shunt_then_transmission(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        inductive = create_l_shunt(media, 13.0, 0.28, "L_shunt_then_TransmissionLine_ind")
        line = create_transmission_line(factory, 0.009, 52.0, "L_shunt_then_TransmissionLine_line")
        return cascade_networks([inductive, line], "L_shunt_then_TransmissionLine_expected")

    def build_transmission_then_r_series(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        line = create_transmission_line(factory, 0.008, 48.0, "TransmissionLine_then_R_series_line")
        media = factory()
        resistor = create_r_series(media, 68.0, "TransmissionLine_then_R_series_res")
        return cascade_networks([line, resistor], "TransmissionLine_then_R_series_expected")

    specs.extend(
        [
            LumpedNetworkSpec(
                name="R_series_then_C_series",
                cli_tokens=tokens_r_series(33.0) + tokens_c_series(3.3),
                build_expected=build_r_series_then_c_series,
                fmin_hz=2e2,
                fmax_hz=2e5,
                npoints=55,
            ),
            LumpedNetworkSpec(
                name="R_shunt_then_L_series",
                cli_tokens=tokens_r_shunt(85.0) + tokens_l_series(9.1, 0.5),
                build_expected=build_r_shunt_then_l_series,
                fmin_hz=1e1,
                fmax_hz=1e4,
                npoints=45,
            ),
            LumpedNetworkSpec(
                name="C_series_then_TransmissionLine",
                cli_tokens=tokens_c_series(4.4) + tokens_transmission_line(0.006, 55.0),
                build_expected=build_c_series_then_transmission,
                fmin_hz=8e4,
                fmax_hz=8e7,
                npoints=95,
            ),
            LumpedNetworkSpec(
                name="C_shunt_then_R_shunt",
                cli_tokens=tokens_c_shunt(5.2) + tokens_r_shunt(120.0),
                build_expected=build_c_shunt_then_r_shunt,
                fmin_hz=4e2,
                fmax_hz=4e5,
                npoints=63,
            ),
            LumpedNetworkSpec(
                name="L_series_then_C_shunt",
                cli_tokens=tokens_l_series(11.0, 0.35) + tokens_c_shunt(3.8),
                build_expected=build_l_series_then_c_shunt,
                fmin_hz=7e5,
                fmax_hz=7e8,
                npoints=141,
            ),
            LumpedNetworkSpec(
                name="L_shunt_then_TransmissionLine",
                cli_tokens=tokens_l_shunt(13.0, 0.28) + tokens_transmission_line(0.009, 52.0),
                build_expected=build_l_shunt_then_transmission,
                fmin_hz=2e6,
                fmax_hz=6e9,
                npoints=175,
            ),
            LumpedNetworkSpec(
                name="TransmissionLine_then_R_series",
                cli_tokens=tokens_transmission_line(0.008, 48.0) + tokens_r_series(68.0),
                build_expected=build_transmission_then_r_series,
                fmin_hz=5e6,
                fmax_hz=9e9,
                npoints=2010,
            ),
        ]
    )

    def build_all_types_cascade(factory: Callable[[float], DefinedGammaZ0]) -> rf.Network:
        media = factory()
        components = [
            create_r_series(media, 47.0, "All_types_R_series"),
            create_r_shunt(media, 90.0, "All_types_R_shunt"),
            create_c_series(media, 3.9, "All_types_C_series"),
            create_c_shunt(media, 6.8, "All_types_C_shunt"),
            create_l_series(media, 10.5, 0.42, "All_types_L_series"),
            create_l_shunt(media, 18.0, 0.33, "All_types_L_shunt"),
            create_transmission_line(
                factory,
                0.012,
                58.0,
                "All_types_TransmissionLine",
                er_eff=1.7,
            ),
            create_transmission_line(
                factory,
                0.007,
                53.0,
                "All_types_TL_lossy",
                er_eff=2.5,
                a_db_per_m=6.5,
                a_d_db_per_m=0.9,
                fa_hz=4.5e9,
            ),
            create_lrc_series_shunt(factory, 2.1, 0.0015, 2.2, "All_types_LRC_ser_shunt"),
            create_lrc_parallel_series(factory, 2.8, 680000.0, 1.7, "All_types_LRC_par_ser"),
        ]
        return cascade_networks(components, "All_types_cascade_expected")

    specs.append(
        LumpedNetworkSpec(
            name="All_types_cascade",
            cli_tokens=(
                tokens_r_series(47.0)
                + tokens_r_shunt(90.0)
                + tokens_c_series(3.9)
                + tokens_c_shunt(6.8)
                + tokens_l_series(10.5, 0.42)
                + tokens_l_shunt(18.0, 0.33)
                + tokens_transmission_line_er(0.012, 58.0, 1.7)
                + tokens_tl_lossy(0.007, 53.0, 2.5, 6.5, 0.9, 4.5e9)
                + tokens_lrc_series_shunt(2.1, 0.0015, 2.2)
                + tokens_lrc_parallel_series(2.8, 680000.0, 1.7)
            ),
            build_expected=build_all_types_cascade,
            fmin_hz=1e7,
            fmax_hz=1e11,
            npoints=10000,
        )
    )

    env = os.environ.copy()
    env.setdefault("QT_QPA_PLATFORM", "offscreen")

    success = True

    for spec in specs:
        s2p_path = artifact_dir / f"{spec.name}.s2p"
        freq_values = np.linspace(spec.fmin_hz, spec.fmax_hz, spec.npoints)
        frequency = rf.Frequency.from_f(freq_values, unit="hz")
        gamma = 1j * 2.0 * math.pi * freq_values / C0

        def make_media(z0: float = Z0_REFERENCE) -> DefinedGammaZ0:
            return DefinedGammaZ0(frequency=frequency, z0=z0, z0_port=Z0_REFERENCE, gamma=gamma)

        run_fsnpview(
            binary,
            spec.cli_tokens,
            s2p_path,
            env=env,
            cwd=repo_root,
            fmin_hz=spec.fmin_hz,
            fmax_hz=spec.fmax_hz,
            npoints=spec.npoints,
        )
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
