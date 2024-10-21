#pragma once

int cmd_build(int argc, char const* argv[]);

int cmd_run(int argc, char const* argv[]);

int cmd_test(int argc, char const* argv[]);

int cmd_clean(int argc, char const* argv[]);

int cmd_validate(int argc, char const* argv[]);

int cmd_deps(int argc, char const* argv[]);
int cmd_deps_add(int argc, char const* argv[]);
int cmd_deps_remove(int argc, char const* argv[]);
int cmd_deps_update(int argc, char const* argv[]);
int cmd_deps_list(int argc, char const* argv[]);
int cmd_deps_help(int argc, char const* argv[]);

int cmd_help(int argc, char const* argv[]);

int cmd_version(int argc, char const* argv[]);
