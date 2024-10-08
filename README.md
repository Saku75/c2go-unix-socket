# C2GO: Unix socket

This is a small prototype to showcase a go API interfacing with a C unix socket.  
It is made as a proof of concept to how this could be done.
Tough it is not optimized.

## Start prime calculator

```bash
cd ./prime-calculator
make
./dist/prime-calculator
```

## Start api

```bash
cd ./api
go run main.go
```

## Start web

```bash
cd ./web
pnpm i
pnpm dev
```
