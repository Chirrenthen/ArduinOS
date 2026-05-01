#!/usr/bin/env python3
"""
Colour utilities for terminal output.
"""

class Colors:
    """ANSI color and style helpers."""

    # Reset and styles
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'
    ITALIC = '\033[3m'
    UNDERLINE = '\033[4m'
    BLINK = '\033[5m'
    REVERSE = '\033[7m'
    HIDDEN = '\033[8m'
    STRIKE = '\033[9m'

    # Foreground colours
    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'

    # Bright foreground
    BRIGHT_BLACK = '\033[90m'
    BRIGHT_RED = '\033[91m'
    BRIGHT_GREEN = '\033[92m'
    BRIGHT_YELLOW = '\033[93m'
    BRIGHT_BLUE = '\033[94m'
    BRIGHT_MAGENTA = '\033[95m'
    BRIGHT_CYAN = '\033[96m'
    BRIGHT_WHITE = '\033[97m'

    @staticmethod
    def rgb(r: int, g: int, b: int, bg: bool = False) -> str:
        """Return ANSI escape for true colour (foreground or background)."""
        code = 48 if bg else 38
        return f'\033[{code};2;{r};{g};{b}m'

    @staticmethod
    def gradient(text: str, start_rgb: tuple, end_rgb: tuple) -> str:
        """Apply a smooth colour gradient across the text."""
        result = ""
        length = max(len(text) - 1, 1)
        for i, char in enumerate(text):
            t = i / length
            r = int(start_rgb[0] + (end_rgb[0] - start_rgb[0]) * t)
            g = int(start_rgb[1] + (end_rgb[1] - start_rgb[1]) * t)
            b = int(start_rgb[2] + (end_rgb[2] - start_rgb[2]) * t)
            result += Colors.rgb(r, g, b) + char
        result += Colors.RESET
        return result