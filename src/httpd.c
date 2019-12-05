#include "httpd.h"

static SSL_CTX* evssl_init(void) {
    SSL_CTX* server_ctx;

    /* Initialize the OpenSSL library */
    SSL_library_init();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
    /* We MUST have entropy, or else there's no point to crypto. */
    if (!RAND_poll())
        return NULL;

    server_ctx = SSL_CTX_new(SSLv23_server_method());
    if (!server_ctx) {
        logger(ERROR, "Couldn't create server context.");
        return NULL;
    }
    SSL_CTX_set_verify(server_ctx, SSL_VERIFY_NONE, NULL); 
    if (!SSL_CTX_use_certificate_chain_file(server_ctx, "cert") ||
        !SSL_CTX_use_PrivateKey_file(server_ctx, "pkey", SSL_FILETYPE_PEM)) {
        puts(
            "Couldn't read 'pkey' or 'cert' file.  To generate a key\n"
            "and self-signed certificate, run:\n"
            "  openssl genrsa -out pkey 2048\n"
            "  openssl req -new -key pkey -out cert.req\n"
            "  openssl x509 -req -days 365 -in cert.req -signkey pkey -out "
            "cert");
        return NULL;
    }
    SSL_CTX_set_options(server_ctx, SSL_OP_NO_SSLv2);

    return server_ctx;
}

int main(int argc, char* argv[]) {
    // return value of main function
    int ret = 0;
    struct event* term = NULL;

    // parse options from command line
    struct options opt = parse_opts(argc, argv);

    // create event config
    struct event_config* cfg = event_config_new();
    struct event_base* base = event_base_new_with_config(cfg);
    if (!base) {
        logger(ERROR, "Couldn't create an event_base: exiting\n");
        ret = 1;
    }
    // free event config when finished
    event_config_free(cfg);
    cfg = NULL;

    // create a new evhttp object to handle requests.
    struct evhttp* http = evhttp_new(base);
    if (!http) {
        logger(ERROR, "Couldn't create evhttp. Exiting.\n");
        ret = 1;
    }

    SSL_CTX* ctx = evssl_init();
    if (ctx == NULL) {
        logger(ERROR, "Couldn't init ssl.");
        goto err;
    }

    /* This is the magic that lets evhttp use SSL. */
    evhttp_set_bevcb(http, bevcb, ctx);

    /* We want to accept arbitrary requests, so we need to set a "generic"
     * cb.  We can also add callbacks for specific paths. */
    evhttp_set_gencb(http, handle_request_cb, &opt);

    // bind socket to http server
    struct evhttp_bound_socket* handle =
        evhttp_bind_socket_with_handle(http, "0.0.0.0", opt.port);
    if (!handle) {
        logger(ERROR, "Couldn't bind to port %d. Exiting.\n", opt.port);
        ret = 1;
        goto err;
    }

    if (display_listen_sock(handle)) {
        ret = 1;
        goto err;
    }

    // add signal handler to event base
    term = evsignal_new(base, SIGINT, do_term, base);
    if (!term)
        goto err;
    if (event_add(term, NULL))
        goto err;

    // dispatch event
    event_base_dispatch(base);

    // error handling
err:
    if (cfg)
        event_config_free(cfg);
    if (http)
        evhttp_free(http);
    if (term)
        event_free(term);
    if (base)
        event_base_free(base);
    if (ctx)
        SSL_CTX_free(ctx);
    return ret;
}
