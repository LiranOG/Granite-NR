"""
Terminal rendering logic for GRANITE telemetry using the rich library.
"""
from typing import Optional

from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

from granite_analysis.core.models import CflEvent, NanEvent, SimulationStep


class GraniteUI:
    def __init__(self, console: Optional[Console] = None):
        self.console = console or Console()

    def get_phase_classification(self, alpha: Optional[float], ham: Optional[float]) -> tuple[str, str]:
        if alpha is None or ham is None:
            return "CRASHED (NaN)", "bold red"
        if alpha <= 1e-4:
            return "Lapse at floor — NaN imminent", "bold red"
        if alpha < 0.005:
            return "Gauge Collapse", "bold red"
        if alpha < 0.05:
            return "Deep Collapse", "yellow"
        if ham > 0.1:
            return "Constraint Explosion", "bold red"
        if ham > 0.01:
            return "Growing Constraint", "yellow"
        if alpha > 0.25:
            return "Trumpet Settling", "green"
        return "Evolving", "cyan"

    def render_step(self, step: SimulationStep) -> None:
        a_str = f"{step.alpha_center:.5e}" if step.alpha_center is not None else "NaN"
        h_str = f"{step.ham_l2:.5e}" if step.ham_l2 is not None else "NaN"

        phase_msg, phase_color = self.get_phase_classification(
            step.alpha_center, step.ham_l2
        )

        a_color = "bold red" if step.alpha_center is None or step.alpha_center < 0.005 else ("yellow" if step.alpha_center < 0.05 else "green")
        h_color = "bold red" if step.ham_l2 is None or step.ham_l2 > 0.1 else ("yellow" if step.ham_l2 > 0.01 else "green")

        blocks = str(step.blocks) if step.blocks is not None else "?"

        table = Table(show_header=False, box=None, padding=(0, 2))
        table.add_row(
            Text(f"step={step.step}", style="bold white"),
            Text(f"t={step.t:.4f}M", style="cyan"),
            Text(f"α={a_str}", style=a_color),
            Text(f"‖H‖₂={h_str}", style=h_color),
            Text(f"Blocks: {blocks}", style="blue"),
            Text(f"Phase: {phase_msg}", style=phase_color),
        )
        self.console.print(table)

    def render_nan(self, event: NanEvent) -> None:
        panel = Panel(
            f"Location: ({event.i}, {event.j}, {event.k})\nValue: {event.value}",
            title=f"[bold red]💥 NaN Detected: {event.var_type} var={event.var_id} at step {event.step}[/]",
            border_style="red"
        )
        self.console.print(panel)

    def render_cfl(self, event: CflEvent) -> None:
        self.console.print(f"[bold yellow]⚠ CFL Warning:[/bold yellow] CFL={event.cfl_value:.4f}")
