#!/usr/bin/env python3
"""
Simple terminal animations.
"""

import sys
import os
import time
import random
from colours import Colors

class Animation:
    """Collection of text‑based animations."""

    @staticmethod
    def spinner(seconds: float, message: str = "Processing") -> None:
        """Display a rotating spinner for the given number of seconds."""
        chars = ['|', '/', '-', '\\']
        steps = int(seconds * 10)
        for _ in range(steps):
            for char in chars:
                sys.stdout.write(f'\r{Colors.CYAN}{char} {message}...{Colors.RESET}')
                sys.stdout.flush()
                time.sleep(0.1)
        sys.stdout.write('\r' + ' ' * (len(message) + 10) + '\r')
        sys.stdout.flush()

    @staticmethod
    def progress_bar(total: int, prefix: str = "Progress", length: int = 30) -> None:
        """Draw a filling progress bar."""
        for i in range(total + 1):
            percent = (i * 100) // total
            filled = int(length * i // total)
            bar = '#' * filled + '-' * (length - filled)
            sys.stdout.write(f'\r{prefix}: |{Colors.GREEN}{bar}{Colors.RESET}| {percent}%')
            sys.stdout.flush()
            time.sleep(0.05)
        print()

    @staticmethod
    def typewriter(text: str, speed: float = 0.03) -> None:
        """Print text character by character."""
        for char in text:
            sys.stdout.write(char)
            sys.stdout.flush()
            time.sleep(speed)

    @staticmethod
    def matrix_rain(lines: int = 10, duration: float = 2) -> None:
        """Simulate the Matrix digital rain."""
        chars = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{}|;:,.<>?/~`"
        end_time = time.time() + duration
        platform_clear = 'cls' if sys.platform.startswith('win') else 'clear'
        while time.time() < end_time:
            line = ''.join(random.choice(chars) for _ in range(80))
            r = random.randint(100, 255)
            colour = random.choice([
                Colors.GREEN,
                Colors.BRIGHT_GREEN,
                Colors.rgb(0, r, 0)
            ])
            sys.stdout.write(f'{colour}{line}{Colors.RESET}\n')
            sys.stdout.flush()
            time.sleep(0.05)
        os.system(platform_clear)