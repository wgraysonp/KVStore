import socket
import sys
import argparse
import typing


def get_parser() -> argparse.ArgumentParser:
    """Create an argument parser object and its arguments.

    Returns:
        parser: argument parser
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", type=str, help="name to pass to server")
    return parser


def main() -> None:
    parser = get_parser()
    args = parser.parse_args()


if __name__ == "__main__":
    main()
