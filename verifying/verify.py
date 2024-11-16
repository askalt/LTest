#!/usr/bin/env python3
import os
import subprocess
import sys

import click

file_dir = os.path.join(
    os.path.dirname(__file__))

project_dir = os.path.join(file_dir, "..")
build_dir = os.path.join(project_dir, "build")

targets_path = ["verifying", "targets"]
targets_dir = os.path.join(build_dir, *targets_path)

def read_file(path):
    with open(path) as f:
        return f.read()


def write_file(path, content):
    with open(path, "w") as f:
        f.write(content)


def run_command_and_get_output(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=None,
        env=None,
        input=None,
        verbose=False,
):
    if env is None:
        env = os.environ
    if cwd is not None:
        env["PWD"] = str(cwd)
    if verbose:
        print(cmd)
    process = subprocess.Popen(
        cmd,
        env=env,
        cwd=cwd,
        stderr=stderr,
        stdout=stdout,
        stdin=subprocess.PIPE,
    )

    if input is not None:
        input = bytes(input, 'utf-8')
    out, _ = process.communicate(input)
    if out is not None:
        out = out.decode('utf-8')
        print(out)
    return process.returncode, out


@click.group()
def cmd():
    pass


def build(target, verbose):
    args = ["cmake", "--build", "build", "--target", os.path.join(*targets_path, target)]
    return run_command_and_get_output(args, stdout=sys.stdout, verbose=verbose)

@cmd.command()
@click.option("-g", "--target", required=True, help="target name", type=str)
@click.option("-t", "--threads", help="threads count", type=int)
@click.option("--tasks", help="tasks per round", type=int)
@click.option("--switches", help="max switches per round", type=int,
              default=None)
@click.option("-r", "--rounds", help="number of rounds", type=int)
@click.option("-v", "--verbose", help="verbose output", type=bool,
              is_flag=True)
@click.option("-s", "--strategy", type=str)
@click.option("-w", "--weights", help="weights for random strategy", type=str)
def run(target, threads, tasks, switches, rounds, verbose, strategy, weights):
    threads = threads or 2
    tasks = tasks or 15
    if switches != 0:
        switches = switches or 100000000
    rounds = rounds or 5
    strategy = strategy or "rr"
    weights = weights or ""
    args = list(
        map(str, ["./" + target, threads, tasks, switches, rounds, 1 if verbose else 0,
                  strategy, weights]))
    rc, _ = build(target, verbose)
    assert(rc == 0)
    run_command_and_get_output(args, cwd=targets_dir, stdout=sys.stdout, verbose=verbose)


cli = click.CommandCollection(sources=[cmd])

if __name__ == "__main__":
    cli()
