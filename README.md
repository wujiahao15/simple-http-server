# simple-http-server

A simple http server implemted by c which supports file upload and download using `openssl` and `libevent`.

This branch is based on `evhttp` of `libevent`. 

## Development Environment

``` bash
Ubuntu 16.04
gcc version: gcc (Ubuntu 5.4.0-6ubuntu1~16.04.11) 5.4.0 20160609
```

## Roadmap

* [x] 支持`HTTP GET` 方法
* [x] 可以下载文件
* [x] 支持`HTTP POST` 方法
* [x] 可以上传文件
* [ ] 支持 HTTP 分块传输
* [ ] 支持 HTTP 持久连接
* [ ] 支持 HTTP 管道
* [x] 使用 `openssl` 库，支持 HTTPS
* [x] 使用 `libevent` 支持多路并发
