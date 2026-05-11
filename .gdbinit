set pagination off
set print pretty on
set print object on
set print demangle on

# Evitar entrar en libc / STL
skip -rfu ^std::
skip -rfu ^__gnu_cxx::
skip -rfu ^__.*
skip file /usr/include/c++/*
skip file /usr/lib/*

# Opcional: no entrar en syscalls
skip -rfu ^__libc_

# Mejor stepping
set step-mode on