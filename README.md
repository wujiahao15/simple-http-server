# simple-http-server

A simple http server implemted by c which supports file upload and download

## Development Environment

``` bash
macOS Catalina
gcc version: Apple clang version 11.0.0 (clang-1100.0.33.12)
```

## Roadmap

* [x] 支持`HTTP GET` 方法
* [x] 可以下载文件
* [x] 支持`HTTP POST` 方法
* [x] 可以上传文件
* [ ] 支持 HTTP 分块传输
* [ ] 支持 HTTP 持久连接
* [ ] 支持 HTTP 管道
* [ ] 使用 `openssl` 库，支持 HTTPS
* [x] 使用 `libevent` 支持多路并发
