various tools to work with flyff files. I mainly use linux so these work best in a linux terminal,
but they should compile on windows as well

# how to build (linux)

```sh
./build.sh
```

# flyffres
extracts .res archives

## example: extracting a single res file

```sh
./flyffres /path/to/file.res
cd /path/to/file.res_/
```

## example: finding and extracting all res files


```sh
find /path/to/FlyffUS/ -name *.res -exec ./flyffres {} \;
```

# flyffjobs
show properties for each job

## example

```sh
./flyffjobs /path/to/FlyffUS
```
