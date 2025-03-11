import subprocess
import logging


def run_subprocess(args, path):
    result = subprocess.run(
        args, cwd=path, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if result.returncode != 0:
        logging.error(f"Subprocess failed with return code {result.returncode}")
        logging.error(result.stderr)
        exit(result.returncode)
