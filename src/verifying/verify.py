#!/usr/bin/env python3
import os
import subprocess

import click

file_dir = os.path.join(
    os.path.dirname(__file__))

artifacts_dir = os.path.join(file_dir, "artifacts")
runtime_dir = os.path.join(file_dir, "..", "runtime")

# Don't forget rebuild llvm pass after changes.
llvm_plugin_path = os.path.join(
    file_dir, "..", "build", "codegen", "CoroGenPass.so")

deps = list(map(lambda f: os.path.join(runtime_dir, f), [
    "lib.cpp", "scheduler.cpp", "lin_check.cpp", "logger.cpp",
    "verifying.cpp",
]))

clang = "clang++"
opt = "opt"
llvm_dis = "llvm-dis"
build_flags = ["-O3", "-std=c++2a"]


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
):
    if env is None:
        env = os.environ
    if cwd is not None:
        env["PWD"] = str(cwd)
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
    out = out.decode('utf-8')

    # This print is here to make running tests with -s flag more verbose
    print(out)

    return process.returncode, out


@click.group()
def cmd():
    pass


@cmd.command()
@click.option("-t", "--threads", help="threads count", type=int)
@click.option("--tasks", help="tasks per round", type=int)
@click.option("--switches", help="max switches per round", type=int,
              default=None)
@click.option("-r", "--rounds", help="number of rounds", type=int)
@click.option("-v", "--verbose", help="verbose output", type=bool,
              is_flag=True)
@click.option("-s", "--strategy", type=str)
@click.option("-w", "--weights", help="weights for random strategy", type=str)
def run(threads, tasks, switches, rounds, verbose, strategy, weights):
    if not os.path.exists(os.path.join(artifacts_dir, "run")):
        print("firstly, build run")
        return

    threads = threads or 2
    tasks = tasks or 15
    if switches != 0:
        switches = switches or 100000000
    rounds = rounds or 5
    strategy = strategy or "rr"
    weights = weights or ""
    args = list(
        map(str, [threads, tasks, switches, rounds, 1 if verbose else 0,
                  strategy, weights]))
    cmd = ["./run"]
    cmd.extend(args)
    run_command_and_get_output(cmd, cwd=artifacts_dir)


@cmd.command()
@click.option("-s", "--src", required=True, help="source directory name",
              type=click.File("r"))
@click.option("-g", "--debug", help="build with -g", type=bool, is_flag=True)
def build(src, debug):
    # Create artifacts dir.
    if not os.path.exists(artifacts_dir):
        os.mkdir(artifacts_dir)

    # Build target.
    cmd = [clang]
    cmd.extend(build_flags)
    cmd.extend(["-c", "-emit-llvm", src.name,
                "-o", os.path.join(artifacts_dir, "bytecode.bc")])
    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0

    # Run llvm pass.
    res_bytecode_path = os.path.join(artifacts_dir, "res.bc")
    cmd = [opt, "--load-pass-plugin", llvm_plugin_path,
           "-passes=coro_gen", "bytecode.bc", "-o", "res.bc"]
    rc, _ = run_command_and_get_output(cmd, cwd=artifacts_dir)
    assert rc == 0

    # Run llvm-dis (debug purposes).
    if debug:
        cmd = [llvm_dis, "res.bc", "-o", "res.ll"]
        rc, _ = run_command_and_get_output(cmd, cwd=artifacts_dir)
        assert rc == 0

    # Build run binary.
    cmd = [clang]
    cmd.extend(build_flags)
    if debug:
        cmd.extend(["-g"])
    cmd.extend([res_bytecode_path])
    cmd.extend(deps)
    cmd.extend(["-o", os.path.join(artifacts_dir, "run")])
    rc, _ = run_command_and_get_output(cmd, cwd=artifacts_dir)
    assert rc == 0


cli = click.CommandCollection(sources=[cmd])

if __name__ == "__main__":
    cli()
