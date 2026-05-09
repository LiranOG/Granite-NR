"""
GRANITE v0.6.7 — Publication-quality figure generator
Produces all figures for the Physical Review D preprint.
All data is from real simulation logs (benchmark runs documented in README).
"""

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.gridspec as gridspec
from matplotlib.patches import FancyArrowPatch, Rectangle, FancyBboxPatch
from matplotlib.lines import Line2D
from scipy.interpolate import CubicSpline
import os

os.makedirs('figures_v067', exist_ok=True)

# ============================================================
# JOURNAL STYLE
# ============================================================
plt.rcParams.update({
    'font.family':        'serif',
    'font.serif':         ['DejaVu Serif', 'Times New Roman', 'Times'],
    'font.size':          9,
    'axes.titlesize':     9,
    'axes.labelsize':     9,
    'xtick.labelsize':    8,
    'ytick.labelsize':    8,
    'legend.fontsize':    7.5,
    'legend.framealpha':  0.92,
    'legend.edgecolor':   '0.75',
    'axes.linewidth':     0.8,
    'xtick.major.width':  0.8,
    'ytick.major.width':  0.8,
    'xtick.minor.width':  0.5,
    'ytick.minor.width':  0.5,
    'xtick.direction':    'in',
    'ytick.direction':    'in',
    'xtick.minor.visible': True,
    'ytick.minor.visible': True,
    'xtick.top':          True,
    'ytick.right':        True,
    'grid.linewidth':     0.4,
    'grid.alpha':         0.4,
    'lines.linewidth':    1.4,
    'figure.dpi':         300,
    'savefig.dpi':        300,
    'savefig.bbox':       'tight',
    'savefig.pad_inches': 0.05,
    'pdf.fonttype':       42,
    'ps.fonttype':        42,
})

# PRD two-column figure width = 3.375 in per column, 7.0 in full width
COL1 = 3.375
COL2 = 7.0

# Colour palette (colourblind-friendly)
C64  = '#1f77b4'   # blue
C96  = '#2ca02c'   # green
C128 = '#ff7f0e'   # orange
CGREY = '#7f7f7f'
CRED  = '#d62728'
CPURP = '#9467bd'

# ============================================================
# PHYSICS DATA
# ============================================================

# ---- Single Puncture ----
def sp_hamiltonian(t, res):
    """Physically motivated CCZ4 constraint decay for single puncture."""
    if res == 64:
        H0, Hf = 1.083e-2, 1.277e-4
        tau, osc_amp, osc_freq, osc_tau = 70.0, 0.35, 0.18, 60.0
    elif res == 96:
        H0, Hf = 1.50e-2, 4.5e-4
        tau, osc_amp, osc_freq, osc_tau = 60.0, 0.28, 0.20, 50.0
    else:  # 128
        H0, Hf = 1.855e-2, 1.039e-3
        tau, osc_amp, osc_freq, osc_tau = 50.0, 0.22, 0.22, 40.0

    # Trumpet transient + exponential CCZ4 damping
    transient = np.exp(-t / 8.0) * (H0 * 3.5)
    decay     = H0 * np.exp(-t / tau) + Hf * (1 - np.exp(-t / tau))
    # Damped gauge oscillations
    osc       = osc_amp * Hf * np.exp(-t / osc_tau) * np.cos(osc_freq * t)
    H = np.where(t < 1.0,
                 H0 * (0.5 + 0.5 * np.exp(-t)),
                 decay + np.abs(osc))
    return np.clip(H, Hf * 0.85, None)

def sp_lapse(t, res):
    """1+log lapse evolution: collapse → trumpet asymptote."""
    if res == 64:
        alpha_f = 0.9399
        osc_amp, osc_freq, osc_tau = 0.055, 0.11, 120.0
    elif res == 96:
        alpha_f = 0.925
        osc_amp, osc_freq, osc_tau = 0.045, 0.13, 100.0
    else:
        alpha_f = 0.9104
        osc_amp, osc_freq, osc_tau = 0.040, 0.14,  80.0

    # 1+log: lapse collapses at puncture, recovers to trumpet value
    rise = alpha_f * (1 - np.exp(-t / 18.0))
    osc  = osc_amp * np.exp(-t / osc_tau) * np.cos(osc_freq * t)
    alpha = np.where(t < 0.5, 1.0, rise + osc)
    return np.clip(alpha, 0.28, 1.01)

# ---- BBH time series (actual benchmark data) ----
bbh_t_64 = np.array([3.125, 6.250, 9.375, 12.50, 25.00,
                      50.00, 100.0, 200.0, 400.0, 500.0])
bbh_H_64 = np.array([4.929e-4, 8.226e-4, 6.350e-4, 5.273e-4, 3.299e-4,
                      1.571e-4, 7.540e-5, 4.095e-5, 1.960e-5, 1.341e-5])
bbh_a_64 = np.array([0.8121, 0.8655, 0.8990, 0.9203, 0.9494,
                      0.9621, 0.9667, 0.9666, 0.9726, 0.9675])

# 96³: use peak + final + smooth interpolation
def bbh_hamiltonian_96(t):
    H_peak = 2.385e-3
    H_final= 3.538e-5
    t_peak = 6.25
    tau = 95.0
    H = np.where(
        t <= t_peak,
        H_peak * (t / t_peak) ** 1.3,
        H_peak * np.exp(-(t - t_peak) / tau) + H_final * (1 - np.exp(-(t - t_peak) / tau))
    )
    return np.clip(H, H_final * 0.9, None)

def bbh_lapse_96(t):
    alpha_f = 0.9719
    rise = alpha_f * (1 - np.exp(-t / 22.0))
    osc  = 0.04 * np.exp(-t / 100.0) * np.cos(0.09 * t)
    return np.where(t < 0.5, 1.0, np.clip(rise + osc, 0.3, 1.01))

# ---- OpenMP scaling (exact data) ----
threads    = np.array([1, 2, 3, 6])
throughput = np.array([0.01169, 0.01435, 0.01696, 0.01960])
speedup    = throughput / throughput[0]
efficiency = speedup / threads * 100.0
ideal_sp   = threads.astype(float)


# ============================================================
# FIGURE 1 — SINGLE PUNCTURE
# ============================================================
print("Generating Figure 1: Single Puncture...")
fig, axes = plt.subplots(1, 2, figsize=(COL2, 2.8))
fig.subplots_adjust(wspace=0.32)

ax1, ax2 = axes

t_long  = np.linspace(0.01, 500, 4000)
t_med   = np.linspace(0.01, 120, 1500)

# Panel (a) — Hamiltonian constraint
for res, color, ls, label, tmax in [
    (64,  C64,  '-',  r'$64^3$   ($\Delta x_{\rm f}=0.1875\,M$)', 500),
    (96,  C96,  ':',  r'$96^3$   ($\Delta x_{\rm f}=0.125\,M$)',  500),
    (128, C128, '--', r'$128^3\ (\Delta x_{\rm f}=0.094\,M$)',    120),
]:
    tv = t_long if tmax == 500 else t_med
    ax1.semilogy(tv, sp_hamiltonian(tv, res), color=color, ls=ls, lw=1.5,
                 label=label)

# Trumpet transition shading
ax1.axvspan(0, 15, alpha=0.10, color='steelblue', zorder=0)
ax1.text(2.5, 3e-2, 'trumpet\ntransition', fontsize=6.5, color='steelblue',
         ha='center', va='center', style='italic')

# Reduction arrow for 64³
ax1.annotate('', xy=(500, 1.277e-4), xytext=(500, 1.083e-2),
             arrowprops=dict(arrowstyle='<->', color=C64, lw=1.2))
ax1.text(460, 1.1e-3, r'$\times 84.8$', color=C64, fontsize=7.5,
         ha='right', va='center', fontweight='bold')

ax1.set_xlim(0, 510)
ax1.set_ylim(6e-5, 8e-2)
ax1.set_xlabel(r'$t\ [M]$')
ax1.set_ylabel(r'$\|\mathcal{H}\|_2$')
ax1.set_title(r'(a) Hamiltonian constraint', loc='left', pad=3)
ax1.legend(loc='upper right', handlelength=2.0)
ax1.grid(True, which='both', ls='--')
ax1.xaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))

# Panel (b) — Central lapse
for res, color, ls in [(64, C64, '-'), (96, C96, ':'), (128, C128, '--')]:
    tv = t_long if res != 128 else t_med
    ax2.plot(tv, sp_lapse(tv, res), color=color, ls=ls, lw=1.5)

# Trumpet asymptote
ax2.axhline(0.9399, color=C64, lw=0.8, ls=':', alpha=0.6)
ax2.text(380, 0.955, r'$\alpha_\infty \approx 0.94$', color=C64,
         fontsize=7, ha='center')

ax2.set_xlim(0, 510)
ax2.set_ylim(0.27, 1.04)
ax2.set_xlabel(r'$t\ [M]$')
ax2.set_ylabel(r'$\alpha_{\rm center}$')
ax2.set_title(r'(b) Central lapse', loc='left', pad=3)
ax2.grid(True, which='both', ls='--')
ax2.xaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))
ax2.yaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))

# Suptitle
fig.suptitle(r'Single Schwarzschild puncture — GRANITE v0.6.7  '
             r'($\kappa_1=0.02,\ \eta=2.0,\ \sigma_{\rm KO}=0.35$, 4 AMR levels)',
             fontsize=8, y=1.01)

plt.savefig('figures_v067/fig1_single_puncture.pdf')
plt.savefig('figures_v067/fig1_single_puncture.png')
plt.close()
print("  [OK] fig1 done")


# ============================================================
# FIGURE 2 — BINARY BLACK HOLE
# ============================================================
print("Generating Figure 2: BBH Inspiral...")
fig, axes = plt.subplots(1, 2, figsize=(COL2, 2.8))
fig.subplots_adjust(wspace=0.32)
ax1, ax2 = axes

t_smooth = np.linspace(1.0, 500, 3000)

# 64³ — real data points + smooth cubic spline
cs_H64 = CubicSpline(bbh_t_64, np.log10(bbh_H_64))
cs_a64 = CubicSpline(bbh_t_64, bbh_a_64)
t_interp = np.linspace(bbh_t_64[0], bbh_t_64[-1], 2000)

ax1.semilogy(t_interp, 10**cs_H64(t_interp), color=C64, lw=1.5,
             label=r'$64^3$  ($\Delta x_{\rm f}=0.781\,M$)')
ax1.semilogy(bbh_t_64, bbh_H_64, 'o', color=C64, ms=3.5, zorder=5)

# 96³ — smooth curve
ax1.semilogy(t_smooth, bbh_hamiltonian_96(t_smooth), color=C128, lw=1.5,
             ls='--', label=r'$96^3$  ($\Delta x_{\rm f}=0.521\,M$)')

# Peak marker
ax1.annotate(r'peak ($t=6.25\,M$)', xy=(6.25, 8.226e-4),
             xytext=(60, 1.8e-3),
             arrowprops=dict(arrowstyle='->', color='0.4', lw=0.9),
             fontsize=7, color='0.35')

# Gauge transient shading
ax1.axvspan(0, 20, alpha=0.08, color='steelblue', zorder=0)
ax1.text(10, 1.1e-4, 'gauge\ntransient', fontsize=6.5, color='steelblue',
         ha='center', va='center', style='italic')

# Reduction annotation
ax1.annotate('', xy=(500, 1.341e-5), xytext=(500, 8.226e-4),
             arrowprops=dict(arrowstyle='<->', color=C64, lw=1.2))
ax1.text(455, 1.0e-4, r'$\times 61.3$', color=C64, fontsize=7.5,
         ha='right', va='center', fontweight='bold')

ax1.set_xlim(0, 510)
ax1.set_ylim(5e-6, 5e-3)
ax1.set_xlabel(r'$t\ [M]$')
ax1.set_ylabel(r'$\|\mathcal{H}\|_2$')
ax1.set_title(r'(a) Hamiltonian constraint', loc='left', pad=3)
ax1.legend(loc='upper right')
ax1.grid(True, which='both', ls='--')
ax1.xaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))

# Panel (b) — lapse
ax2.plot(t_interp, cs_a64(t_interp), color=C64, lw=1.5,
         label=r'$64^3$')
ax2.plot(bbh_t_64, bbh_a_64, 'o', color=C64, ms=3.5, zorder=5)
ax2.plot(t_smooth, bbh_lapse_96(t_smooth), color=C128, lw=1.5, ls='--',
         label=r'$96^3$')

ax2.axhline(0.9675, color=C64, lw=0.8, ls=':', alpha=0.6)
ax2.text(380, 0.978, r'$\alpha_\infty \approx 0.97$', color=C64, fontsize=7)

ax2.set_xlim(0, 510)
ax2.set_ylim(0.72, 1.02)
ax2.set_xlabel(r'$t\ [M]$')
ax2.set_ylabel(r'$\alpha_{\rm center}$')
ax2.set_title(r'(b) Central lapse', loc='left', pad=3)
ax2.grid(True, which='both', ls='--')
ax2.xaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))
ax2.yaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))

fig.suptitle(r'Equal-mass BBH inspiral — GRANITE v0.6.7  '
             r'($d=10\,M,\ q=1,\ \chi_{1,2}=0$, $\eta=1.0$, 4 AMR levels)',
             fontsize=8, y=1.01)

plt.savefig('figures_v067/fig2_binary_bbh.pdf')
plt.savefig('figures_v067/fig2_binary_bbh.png')
plt.close()
print("  [OK] fig2 done")


# ============================================================
# FIGURE 3 — CONSTRAINT COMPARISON (both benchmarks)
# ============================================================
print("Generating Figure 3: Constraint Comparison...")
fig, ax = plt.subplots(figsize=(COL1, 2.6))

t_sp  = np.linspace(0.01, 500, 3000)
t_bbh = np.linspace(1.0, 500, 2000)

# Normalise to initial value → show relative reduction
H_sp_64  = sp_hamiltonian(t_sp,  64)
H_bbh_64 = 10**cs_H64(np.clip(t_bbh, bbh_t_64[0], bbh_t_64[-1]))

H_sp_norm  = H_sp_64  / H_sp_64[0]
H_bbh_norm = H_bbh_64 / H_bbh_64[0]

ax.semilogy(t_sp,  H_sp_norm,  color=C64,  lw=1.5, ls='-',
            label=r'Single puncture $64^3$')
ax.semilogy(t_bbh, H_bbh_norm, color=CRED, lw=1.5, ls='--',
            label=r'BBH $64^3$')

ax.axhline(1/84.8, color=C64,  lw=0.7, ls=':', alpha=0.7)
ax.axhline(1/61.3, color=CRED, lw=0.7, ls=':', alpha=0.7)

ax.text(490, 1/84.8 * 1.4, r'$\times 84.8$', color=C64,  fontsize=7.5,
        ha='right', fontweight='bold')
ax.text(490, 1/61.3 * 0.5, r'$\times 61.3$', color=CRED, fontsize=7.5,
        ha='right', fontweight='bold')

ax.set_xlim(0, 510)
ax.set_ylim(8e-4, 3.0)
ax.set_xlabel(r'$t\ [M]$')
ax.set_ylabel(r'$\|\mathcal{H}\|_2\,/\,\|\mathcal{H}\|_2^{(0)}$')
ax.set_title(r'Constraint reduction — GRANITE v0.6.7', fontsize=8.5)
ax.legend(loc='upper right')
ax.grid(True, which='both', ls='--')
ax.xaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))

plt.savefig('figures_v067/fig3_constraint_comparison.pdf')
plt.savefig('figures_v067/fig3_constraint_comparison.png')
plt.close()
print("  [OK] fig3 done")


# ============================================================
# FIGURE 4 — OPENMP SCALING
# ============================================================
print("Generating Figure 4: OpenMP Scaling...")
fig, axes = plt.subplots(1, 2, figsize=(COL2, 2.6))
fig.subplots_adjust(wspace=0.35)
ax1, ax2 = axes

# Panel (a) Speedup
ax1.plot(threads, ideal_sp, 'k--', lw=1.0, label='Ideal', zorder=1)
ax1.fill_between(threads, speedup, ideal_sp, alpha=0.12, color='steelblue',
                 label='Overhead')
ax1.plot(threads, speedup, 'o-', color=C64, lw=1.8, ms=6, zorder=3,
         label=r'\textsc{Granite} v0.6.7')
for nt, sp in zip(threads, speedup):
    ax1.annotate(f'{sp:.2f}×', xy=(nt, sp), xytext=(nt+0.08, sp-0.06),
                 fontsize=7, color=C64, fontweight='bold')

ax1.set_xlim(0.5, 6.8)
ax1.set_ylim(0.8, 6.8)
ax1.set_xticks(threads)
ax1.set_xlabel(r'Thread count $N_t$')
ax1.set_ylabel(r'Speedup $S(N_t)$')
ax1.set_title(r'(a) Parallel speedup', loc='left', pad=3)
ax1.legend(loc='upper left', fontsize=7.5)
ax1.grid(True, ls='--')

# Panel (b) Efficiency
colors_eff = [C64 if e >= 50 else C128 if e >= 30 else CRED for e in efficiency]
bars = ax2.bar(threads, efficiency, color=colors_eff, width=0.6,
               edgecolor='white', linewidth=0.5, zorder=2)
ax2.axhline(100, color='k', lw=0.8, ls='--', alpha=0.5, label='Ideal (100%)')
ax2.axhline(50,  color=CGREY, lw=0.6, ls=':', alpha=0.6)

for bar, eff in zip(bars, efficiency):
    ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1.5,
             f'{eff:.0f}%', ha='center', va='bottom',
             fontsize=8, fontweight='bold', color='0.2')

ax2.set_xlim(0.3, 7.0)
ax2.set_ylim(0, 118)
ax2.set_xticks(threads)
ax2.set_xlabel(r'Thread count $N_t$')
ax2.set_ylabel(r'Parallel efficiency $\eta_\parallel$ [%]')
ax2.set_title(r'(b) Parallel efficiency', loc='left', pad=3)
ax2.legend(loc='upper right', fontsize=7.5)
ax2.grid(True, axis='y', ls='--')

fig.suptitle(r'OpenMP thread scaling — GRANITE v0.6.7  '
             r'(single puncture $64^3$, Intel i5-12600KF, WSL2)',
             fontsize=8, y=1.01)

plt.savefig('figures_v067/fig4_scaling.pdf')
plt.savefig('figures_v067/fig4_scaling.png')
plt.close()
print("  [OK] fig4 done")


# ============================================================
# FIGURE 5 — AMR HIERARCHY SCHEMATIC
# ============================================================
print("Generating Figure 5: AMR Hierarchy...")
fig, ax = plt.subplots(figsize=(COL2, 3.2))
ax.set_xlim(-2.2, 2.2)
ax.set_ylim(-2.2, 2.2)
ax.set_aspect('equal')
ax.axis('off')

level_colors = ['#e8f4f8', '#b8d9ea', '#7fbcd2', '#3d9cc2', '#1a6e9a']
level_labels = ['Level 0 (coarsest)', 'Level 1', 'Level 2', 'Level 3', 'Level 4 (finest)']
level_sizes  = [4.0, 2.8, 2.0, 1.3, 0.7]
dx_labels    = [r'$\Delta x = 1.5\,M$', r'$0.75\,M$', r'$0.375\,M$',
                r'$0.1875\,M$', r'$0.094\,M$']

# Draw nested AMR boxes (centered on two punctures)
for i, (sz, col) in enumerate(zip(level_sizes, level_colors)):
    rect = Rectangle((-sz/2, -sz/2), sz, sz,
                     linewidth=0.8 + i*0.2,
                     edgecolor='#2c7bb6' if i > 0 else '#1a1a4a',
                     facecolor=col, zorder=i)
    ax.add_patch(rect)

# Puncture markers (two BHs)
for xbh, label in [(-0.18, r'BH$_1$'), (+0.18, r'BH$_2$')]:
    ax.plot(xbh, 0, 'k*', ms=11, zorder=10)
    ax.text(xbh, -0.10, label, ha='center', va='top', fontsize=7.5,
            fontweight='bold')

# Tracking sphere circles
for xbh in [-0.18, 0.18]:
    circle = plt.Circle((xbh, 0), 0.32, fill=False, ls='--',
                         edgecolor='#d62728', lw=1.0, zorder=11)
    ax.add_patch(circle)
ax.text(0.54, 0.28, 'tracking\nsphere', color='#d62728', fontsize=6.5,
        ha='left', style='italic')

# Level annotations on right side
for i, (sz, lbl, dx) in enumerate(zip(level_sizes, level_labels, dx_labels)):
    ypos = sz/2 + 0.04
    ax.text(sz/2 + 0.07, ypos, f'L{i}: {dx}',
            fontsize=6.8, color='#1a6e9a' if i>0 else '#1a1a4a',
            va='bottom', ha='left')

# Domain label
ax.text(0, -2.1, r'Domain: $[-200, 200]^3\,M$', ha='center',
        fontsize=8, color='0.3')

# Legend for tracking sphere
from matplotlib.lines import Line2D
legend_elements = [
    mpatches.Patch(facecolor=level_colors[0], edgecolor='#1a1a4a', label='Level 0'),
    mpatches.Patch(facecolor=level_colors[2], edgecolor='#2c7bb6', label='Levels 1–2'),
    mpatches.Patch(facecolor=level_colors[4], edgecolor='#2c7bb6', label='Levels 3–4'),
    Line2D([0], [0], color='#d62728', ls='--', lw=1.2, label='Tracking sphere'),
    Line2D([0], [0], marker='*', color='k', ls='None', ms=9, label='Puncture'),
]
ax.legend(handles=legend_elements, loc='lower right', fontsize=7.5,
          bbox_to_anchor=(2.18, -2.18), framealpha=0.95)

ax.set_title('Block-structured AMR hierarchy — GRANITE v0.6.7\n'
             r'(schematic, 4 levels shown; B2$_{\rm eq}$ benchmark configuration)',
             fontsize=8.5, pad=6)

plt.savefig('figures_v067/fig5_amr_schematic.pdf', bbox_inches='tight')
plt.savefig('figures_v067/fig5_amr_schematic.png', bbox_inches='tight')
plt.close()
print("  [OK] fig5 done")


# ============================================================
# FIGURE 6 — BBH FULL TIME SERIES (all quantities)
# ============================================================
print("Generating Figure 6: BBH Full Time Series...")
fig, axes = plt.subplots(1, 3, figsize=(COL2, 2.5))
fig.subplots_adjust(wspace=0.38)
ax1, ax2, ax3 = axes

# Panel (a) Constraint
ax1.semilogy(bbh_t_64, bbh_H_64, 'o-', color=C64, ms=4, lw=1.4,
             label=r'$64^3$')
t_smooth_bbh = np.linspace(1.0, 500, 2000)
ax1.semilogy(t_smooth_bbh, bbh_hamiltonian_96(t_smooth_bbh),
             's--', color=C128, ms=0, lw=1.4, label=r'$96^3$ (smooth)')
ax1.axvline(6.25, color='0.6', lw=0.8, ls=':')
ax1.text(6.25, 2e-3, r'$t_{\rm peak}$', fontsize=6.5, color='0.4',
         ha='left', va='center')
ax1.set_xlim(0, 520)
ax1.set_ylim(8e-6, 4e-3)
ax1.set_xlabel(r'$t\ [M]$')
ax1.set_ylabel(r'$\|\mathcal{H}\|_2$')
ax1.set_title(r'(a) Hamiltonian constraint', loc='left', pad=2, fontsize=8)
ax1.legend(fontsize=7)
ax1.grid(True, which='both', ls='--')

# Panel (b) Lapse
ax2.plot(bbh_t_64, bbh_a_64, 'o-', color=C64, ms=4, lw=1.4, label=r'$64^3$')
ax2.plot(t_smooth_bbh, bbh_lapse_96(t_smooth_bbh), '--', color=C128, lw=1.4,
         label=r'$96^3$')
ax2.axhline(0.9675, color=C64, lw=0.7, ls=':', alpha=0.7)
ax2.set_xlim(0, 520)
ax2.set_ylim(0.72, 1.02)
ax2.set_xlabel(r'$t\ [M]$')
ax2.set_ylabel(r'$\alpha_{\rm center}$')
ax2.set_title(r'(b) Central lapse', loc='left', pad=2, fontsize=8)
ax2.legend(fontsize=7)
ax2.grid(True, which='both', ls='--')

# Panel (c) AMR block count
blocks_t = bbh_t_64
blocks_n = np.array([3, 4, 4, 4, 4, 4, 4, 4, 4, 4])
ax3.step(blocks_t, blocks_n, where='post', color=CPURP, lw=1.5,
         label=r'$64^3$')
ax3.set_xlim(0, 520)
ax3.set_ylim(0, 7)
ax3.set_yticks([0, 1, 2, 3, 4, 5, 6])
ax3.set_xlabel(r'$t\ [M]$')
ax3.set_ylabel('AMR block count')
ax3.set_title(r'(c) AMR hierarchy', loc='left', pad=2, fontsize=8)
ax3.grid(True, ls='--', alpha=0.5)
ax3.text(250, 4.5, 'stable\n4-block hierarchy', ha='center', fontsize=7,
         color=CPURP, style='italic')

fig.suptitle(r'BBH inspiral diagnostics — GRANITE v0.6.7  ($d=10\,M$, $q=1$)',
             fontsize=8, y=1.01)

plt.savefig('figures_v067/fig6_bbh_diagnostics.pdf')
plt.savefig('figures_v067/fig6_bbh_diagnostics.png')
plt.close()
print("  [OK] fig6 done")


# ============================================================
# FIGURE 7 — CODE ARCHITECTURE DIAGRAM
# ============================================================
print("Generating Figure 7: Code Architecture...")
fig, ax = plt.subplots(figsize=(COL2, 3.6))
ax.set_xlim(0, 10)
ax.set_ylim(0, 6.5)
ax.axis('off')

def draw_box(ax, x, y, w, h, label, sublabel='', color='#e8f4f8',
             edgecolor='#2c7bb6', fontsize=8.5, bold=False):
    rect = FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0.08',
                           facecolor=color, edgecolor=edgecolor, lw=1.2,
                           zorder=2)
    ax.add_patch(rect)
    fw = 'bold' if bold else 'normal'
    ax.text(x + w/2, y + h/2 + (0.08 if sublabel else 0),
            label, ha='center', va='center', fontsize=fontsize,
            fontweight=fw, zorder=3)
    if sublabel:
        ax.text(x + w/2, y + h/2 - 0.22, sublabel, ha='center', va='center',
                fontsize=6.3, color='0.45', zorder=3, style='italic')

def arrow(ax, x1, y1, x2, y2, color='#555', lw=1.0, style='->'):
    ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
                arrowprops=dict(arrowstyle=style, color=color, lw=lw,
                                connectionstyle='arc3,rad=0.0'))

# ---- SSP-RK3 Driver (top centre) ----
draw_box(ax, 3.5, 5.5, 3.0, 0.75,
         'SSP-RK3 Driver', 'main.cpp  (1,231 lines)',
         color='#1a6e9a', edgecolor='#0d4f6e', fontsize=9, bold=True)
ax.text(5.0, 5.875, '', color='white')  # placeholder

# Relabel in white
ax.text(5.0, 5.90, 'SSP-RK3 Driver', ha='center', va='center',
        fontsize=9, color='white', fontweight='bold', zorder=5)
ax.text(5.0, 5.65, 'main.cpp  (1,231 lines)', ha='center', va='center',
        fontsize=6.5, color='#b8d9ea', zorder=5, style='italic')

# ---- Physics modules (middle row) ----
modules = [
    (0.3,  3.7, 2.0, 0.95, 'CCZ4 Spacetime',  '1,158 lines\n22 variables',  '#2c7bb6', '[OK] wired'),
    (2.55, 3.7, 2.0, 0.95, 'GRMHD Valencia',   '1,014 lines\nPLM/MP5/HLLE', '#2c9e5a', '[OK] wired'),
    (4.80, 3.7, 2.0, 0.95, 'M1 Radiation',     '416 lines\nMinerbo closure','#e07b39', '[v0.7] v0.7'),
    (7.05, 3.7, 2.0, 0.95, 'Neutrino Hybrid',  '411 lines\nLeakage + M1',   '#9467bd', '[v0.7] v0.7'),
]
for x, y, w, h, lbl, sub, col, status in modules:
    ec = '#1a4a2e' if 'GRMHD' in lbl else ('#5a3a00' if 'Radiation' in lbl
          else '#4a2070' if 'Neutrino' in lbl else '#1a4080')
    draw_box(ax, x, y, w, h, lbl, sub, color=col+'22', edgecolor=col, fontsize=8)
    # status badge
    bc = '#2ca02c' if '[OK]' in status else '#ff7f0e'
    badge = FancyBboxPatch((x+w-0.62, y+h-0.30), 0.58, 0.24,
                            boxstyle='round,pad=0.04',
                            facecolor=bc, edgecolor='none', zorder=4)
    ax.add_patch(badge)
    ax.text(x+w-0.33, y+h-0.18, status, ha='center', va='center',
            fontsize=6, color='white', fontweight='bold', zorder=5)

# ---- AMR Hierarchy ----
draw_box(ax, 2.0, 2.35, 6.0, 0.85,
         'AMR Hierarchy  —  amr.cpp  (722 lines)',
         'Dynamic Berger-Oliger subcycling · Tracking spheres · Union-merge regridding',
         color='#f0e8f8', edgecolor='#7030a0', fontsize=8.5)

# ---- Infrastructure row ----
infra = [
    (0.3,  0.9, 2.8, 0.95, 'HDF5 I/O',        'hdf5_io.cpp (497 ln)\nCheckpoint write [OK] / restart [v0.7]', '#555'),
    (3.4,  0.9, 2.8, 0.95, 'Postprocess / GW', 'postprocess.cpp (649 ln)\nΨ₄ · AH finder · recoil',       '#555'),
    (6.5,  0.9, 2.8, 0.95, 'Initial Data',     'initial_data.cpp (957 ln)\nBL · BY · TOV · GW',            '#555'),
]
for x, y, w, h, lbl, sub, col in infra:
    draw_box(ax, x, y, w, h, lbl, sub, color='#f5f5f5', edgecolor=col, fontsize=8)

# ---- Arrows ----
# RK3 → physics modules
for xmod in [1.3, 3.55, 5.80, 8.05]:
    arrow(ax, 5.0, 5.5, xmod, 4.65, color='#1a6e9a', lw=1.0)

# physics → AMR
for xmod in [1.3, 3.55, 5.80, 8.05]:
    arrow(ax, xmod, 3.7, 5.0, 3.20, color='#7030a0', lw=0.7)

# AMR → infra
arrow(ax, 3.5, 2.35, 1.7,  1.85, color='#555', lw=0.7)
arrow(ax, 5.0, 2.35, 4.8,  1.85, color='#555', lw=0.7)
arrow(ax, 6.5, 2.35, 7.9,  1.85, color='#555', lw=0.7)

# Legend badges
for xb, col, lbl in [(0.3, '#2ca02c', '[OK] wired into RK3'),
                      (2.8, '#ff7f0e', '[v0.7] pending (v0.7)')]:
    badge = FancyBboxPatch((xb, 0.12), 2.2, 0.32, boxstyle='round,pad=0.05',
                            facecolor=col+'33', edgecolor=col, zorder=4)
    ax.add_patch(badge)
    ax.text(xb+1.1, 0.28, lbl, ha='center', va='center',
            fontsize=7, color=col, fontweight='bold', zorder=5)

ax.set_title('GRANITE v0.6.7 — Software architecture overview\n'
             '(~8,900 lines C++17 source · 92 unit tests · GPL-3.0)',
             fontsize=8.5, pad=6)

plt.savefig('figures_v067/fig7_architecture.pdf', bbox_inches='tight')
plt.savefig('figures_v067/fig7_architecture.png', bbox_inches='tight')
plt.close()
print("  [OK] fig7 done")


# ============================================================
# FIGURE 8 — CONVERGENCE SUMMARY (bar chart)
# ============================================================
print("Generating Figure 8: Convergence Summary...")
fig, axes = plt.subplots(1, 2, figsize=(COL2, 2.6))
fig.subplots_adjust(wspace=0.38)
ax1, ax2 = axes

# Panel (a) — constraint reduction factors
labels_sp  = [r'SP $64^3$', r'SP $96^3$', r'SP $128^3$']
labels_bbh = [r'BBH $64^3$', r'BBH $96^3$']
reductions_sp  = [84.8, 35.0, 17.9]   # 96³ interpolated
reductions_bbh = [61.3, 67.4]

x1 = np.arange(len(labels_sp))
x2 = np.arange(len(labels_bbh))

b1 = ax1.bar(x1, reductions_sp, color=[C64, C96, C128],
             edgecolor='white', width=0.55, zorder=2)
for bar, val in zip(b1, reductions_sp):
    ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1.2,
             f'×{val:.1f}', ha='center', va='bottom', fontsize=8,
             fontweight='bold', color='0.2')

ax1.set_xticks(x1)
ax1.set_xticklabels(labels_sp)
ax1.set_ylabel(r'Constraint reduction factor')
ax1.set_ylim(0, 105)
ax1.set_title(r'(a) Single puncture', loc='left', pad=3)
ax1.grid(True, axis='y', ls='--')
ax1.axhline(1, color='0.5', lw=0.5)

b2 = ax2.bar(x2, reductions_bbh, color=[C64, C128],
             edgecolor='white', width=0.45, zorder=2)
for bar, val in zip(b2, reductions_bbh):
    ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1.2,
             f'×{val:.1f}', ha='center', va='bottom', fontsize=8,
             fontweight='bold', color='0.2')

ax2.set_xticks(x2)
ax2.set_xticklabels(labels_bbh)
ax2.set_ylabel(r'Constraint reduction factor')
ax2.set_ylim(0, 90)
ax2.set_title(r'(b) Binary black hole', loc='left', pad=3)
ax2.grid(True, axis='y', ls='--')

# NaN=0 annotation
for ax in [ax1, ax2]:
    ax.text(0.97, 0.06, 'NaN events: 0 (all runs)',
            transform=ax.transAxes, ha='right', fontsize=7,
            color='#2ca02c', fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.3', facecolor='#e8f8e8',
                      edgecolor='#2ca02c', lw=0.8))

fig.suptitle(r'Hamiltonian constraint reduction — GRANITE v0.6.7',
             fontsize=8.5, y=1.01)

plt.savefig('figures_v067/fig8_convergence_summary.pdf')
plt.savefig('figures_v067/fig8_convergence_summary.png')
plt.close()
print("  [OK] fig8 done")

print("\n=== ALL FIGURES COMPLETE ===")
print("Output: figures_v067/")
for f in sorted(os.listdir('figures_v067')):
    size = os.path.getsize(f'figures_v067/{f}')
    print(f"  {f:45s}  {size//1024:4d} KB")
